#include "modbus_tcp_server.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace modbus_tcp_server {

static const char *TAG = "modbus_tcp";

// Per-call read timeout for client.readBytes() — must be short enough that a
// stale/broken connection doesn't block the ESPHome loop for too long.
static const uint32_t kClientReadTimeoutMs = 200;

// Total session timeout — how long an idle connected client is kept alive.
static const uint32_t kSessionTimeoutMs = 2000;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ModbusTcpServer::setup() {
  // ── Carlo Gavazzi EM24DINAV53XE1X identification registers ───────────────
  // Source: github.com/victronenergy/dbus-modbus-client/carlo_gavazzi.py
  // The Cerbo GX probes register 0x000B over TCP to identify the meter.

  // 0x000B: Model ID = 1651 → EM24DINAV53XE1X (3-phase)
  write_holding_register(0x000b, 1651);

  // 0x0302: Hardware version
  write_holding_register(0x0302, 0x0100);

  // 0x0304: Firmware version
  write_holding_register(0x0304, 0x0100);

  // 0x1002: Phase config — 0 = 3P.n (three-phase), 3 = 1P (single-phase)
  write_holding_register(0x1002, (phases_ == 1) ? 3 : 0);

  // 0x5000-0x5006: Serial number (7 registers, ASCII "0000000")
  for (uint16_t r = 0x5000; r <= 0x5006; r++)
    write_holding_register(r, 0x3030);

  // 0xa000: Application mode = 7 (mode H, required by driver)
  write_holding_register(0xa000, 7);

  // 0xa100: Switch position = 3 → "Locked" (prevents physical mode changes)
  write_holding_register(0xa100, 3);

  server_ = new WiFiServer(port_);
  // Don't call begin() here — WiFi may not be connected yet.
  // start_server_() is called by loop() on first WiFi connect.
}

void ModbusTcpServer::start_server_() {
  server_->stop();   // release any existing socket before re-binding
  server_->begin();
  server_started_ = true;
  ESP_LOGI(TAG, "Modbus TCP server listening on port %d (unit ID %d)", port_, unit_id_);
}

// ---------------------------------------------------------------------------
// Main loop — non-blocking state machine.
// ---------------------------------------------------------------------------

void ModbusTcpServer::loop() {
  if (!server_)
    return;

  // Restart the server socket when WiFi reconnects after a drop.
  bool wifi_connected = network::is_connected();
  if (wifi_connected && !wifi_was_connected_) {
    ESP_LOGI(TAG, "WiFi (re)connected — restarting Modbus TCP server");
    start_server_();
  }
  wifi_was_connected_ = wifi_connected;

  if (!wifi_connected || !server_started_)
    return;

  // ── Handle active client ──────────────────────────────────────────────────
  if (client_.connected()) {
    if ((millis() - client_start_ms_) >= kSessionTimeoutMs) {
      client_.stop();
      ESP_LOGI(TAG, "Client timed out, closing");
      return;
    }
    if (client_.available() >= 7) {  // MBAP header (6) + unit ID (1)
      if (handle_request_(client_)) {
        client_start_ms_ = millis();  // reset idle timer on valid exchange
      }
    }
    return;  // yield control back to ESPHome main loop
  }

  // Client was active last iteration but has since disconnected
  if (client_) {
    client_.stop();
    ESP_LOGI(TAG, "Client disconnected");
  }

  // ── Accept new client ─────────────────────────────────────────────────────
  WiFiClient incoming = server_->accept();
  if (incoming) {
    incoming.setTimeout(kClientReadTimeoutMs);
    client_ = incoming;
    client_start_ms_ = millis();
    ESP_LOGI(TAG, "Client connected: %s", client_.remoteIP().toString().c_str());
  }
}

// ---------------------------------------------------------------------------
// Typed setters — EM24 register map, little-endian 32-bit (s32l)
// ---------------------------------------------------------------------------

void ModbusTcpServer::write_s32l_(uint16_t base_addr, int32_t value) {
  write_holding_register(base_addr,     (uint16_t)(value & 0xFFFF));
  write_holding_register(base_addr + 1, (uint16_t)((value >> 16) & 0xFFFF));
}

void ModbusTcpServer::update_total_power_() {
  int32_t total = raw_power_[0] + raw_power_[1] + raw_power_[2];
  write_s32l_(0x0028, total);
}

// Phase-to-neutral voltage: s32l ÷10 → V, base 0x0000, step 2
void ModbusTcpServer::set_voltage(uint8_t phase, float volts) {
  if (phase < 1 || phase > 3) return;
  write_s32l_(0x0000 + (phase - 1) * 2, (int32_t)(volts * 10.0f));
}

// Line-to-line voltage: EM24 doesn't expose L-L via Modbus — no-op
void ModbusTcpServer::set_ll_voltage(uint8_t phase, float volts) {
  (void)phase; (void)volts;
}

// Current: s32l ÷1000 → A, base 0x000c, step 2
void ModbusTcpServer::set_current(uint8_t phase, float amps) {
  if (phase < 1 || phase > 3) return;
  write_s32l_(0x000c + (phase - 1) * 2, (int32_t)(amps * 1000.0f));
}

// Active power: s32l ÷10 → W, base 0x0012, step 2; total at 0x0028
void ModbusTcpServer::set_power(uint8_t phase, float watts) {
  if (phase < 1 || phase > 3) return;
  raw_power_[phase - 1] = (int32_t)(watts * 10.0f);
  write_s32l_(0x0012 + (phase - 1) * 2, raw_power_[phase - 1]);
  update_total_power_();
}

// Frequency: u16 ÷10 → Hz, register 0x0033
void ModbusTcpServer::set_frequency(float hz) {
  write_holding_register(0x0033, (uint16_t)(hz * 10.0f));
}

// Energy import: s32l ÷10 → kWh, registers 0x0034-35
void ModbusTcpServer::set_energy_import(float kwh) {
  write_s32l_(0x0034, (int32_t)(kwh * 10.0f));
}

// Energy export: s32l ÷10 → kWh, registers 0x004e-4f
void ModbusTcpServer::set_energy_export(float kwh) {
  write_s32l_(0x004e, (int32_t)(kwh * 10.0f));
}

// Per-phase energy import: s32l ÷10 → kWh, base 0x0040, step 2
void ModbusTcpServer::set_phase_energy_import(uint8_t phase, float kwh) {
  if (phase < 1 || phase > 3) return;
  write_s32l_(0x0040 + (phase - 1) * 2, (int32_t)(kwh * 10.0f));
}

// ---------------------------------------------------------------------------
// Typed getters — read back from register map for ESPHome template sensors
// ---------------------------------------------------------------------------

int32_t read_s32l(std::map<uint16_t, uint16_t> &regs, uint16_t base_addr) {
  uint16_t lo = regs.count(base_addr)     ? regs[base_addr]     : 0;
  uint16_t hi = regs.count(base_addr + 1) ? regs[base_addr + 1] : 0;
  return (int32_t)((uint32_t)hi << 16 | lo);
}

float ModbusTcpServer::get_voltage(uint8_t phase) {
  if (phase < 1 || phase > 3) return 0.0f;
  return read_s32l(registers_, 0x0000 + (phase - 1) * 2) / 10.0f;
}

float ModbusTcpServer::get_current(uint8_t phase) {
  if (phase < 1 || phase > 3) return 0.0f;
  return read_s32l(registers_, 0x000c + (phase - 1) * 2) / 1000.0f;
}

float ModbusTcpServer::get_power(uint8_t phase) {
  if (phase < 1 || phase > 3) return 0.0f;
  return read_s32l(registers_, 0x0012 + (phase - 1) * 2) / 10.0f;
}

float ModbusTcpServer::get_total_power() {
  return read_s32l(registers_, 0x0028) / 10.0f;
}

float ModbusTcpServer::get_frequency() {
  return (registers_.count(0x0033) ? registers_[0x0033] : 0) / 10.0f;
}

float ModbusTcpServer::get_energy_import() {
  return read_s32l(registers_, 0x0034) / 10.0f;
}

float ModbusTcpServer::get_energy_export() {
  return read_s32l(registers_, 0x004e) / 10.0f;
}

float ModbusTcpServer::get_phase_energy_import(uint8_t phase) {
  if (phase < 1 || phase > 3) return 0.0f;
  return read_s32l(registers_, 0x0040 + (phase - 1) * 2) / 10.0f;
}

// ---------------------------------------------------------------------------
// Modbus TCP frame parser
// ---------------------------------------------------------------------------

bool ModbusTcpServer::handle_request_(WiFiClient &client) {
  uint8_t header[7];
  if (client.readBytes(header, 7) != 7)
    return false;

  uint16_t transaction_id = ((uint16_t)header[0] << 8) | header[1];
  uint16_t protocol_id    = ((uint16_t)header[2] << 8) | header[3];
  uint16_t length         = ((uint16_t)header[4] << 8) | header[5];
  // header[6] = unit ID (accepted regardless of value to be permissive)

  if (protocol_id != 0x0000) {
    ESP_LOGW(TAG, "Non-Modbus protocol ID 0x%04X, dropping", protocol_id);
    return false;
  }

  if (length < 2) {
    ESP_LOGW(TAG, "MBAP length too small: %d", length);
    return false;
  }

  uint16_t pdu_len = length - 1;
  if (pdu_len > 253) {
    ESP_LOGW(TAG, "PDU length %d exceeds Modbus maximum of 253", pdu_len);
    return false;
  }

  uint8_t pdu[253];
  if (client.readBytes(pdu, pdu_len) != pdu_len)
    return false;

  uint8_t function_code = pdu[0];

  if (function_code == 0x03 || function_code == 0x04) {
    if (pdu_len < 5) {
      send_exception_(client, transaction_id, function_code, 0x03);
      return true;
    }

    uint16_t start_addr = ((uint16_t)pdu[1] << 8) | pdu[2];
    uint16_t quantity   = ((uint16_t)pdu[3] << 8) | pdu[4];

    if (quantity == 0 || quantity > 125) {
      send_exception_(client, transaction_id, function_code, 0x03);
      return true;
    }

    if ((uint32_t)start_addr + (uint32_t)quantity > 0x10000U) {
      send_exception_(client, transaction_id, function_code, 0x02);
      return true;
    }

    ESP_LOGD(TAG, "FC%02X addr=0x%04X qty=%d", function_code, start_addr, quantity);

    uint16_t byte_count  = quantity * 2;
    uint16_t mbap_length = 3 + byte_count;

    uint8_t response[9 + 125 * 2];
    response[0] = (uint8_t)(transaction_id >> 8);
    response[1] = (uint8_t)(transaction_id & 0xFF);
    response[2] = 0x00;
    response[3] = 0x00;
    response[4] = (uint8_t)(mbap_length >> 8);
    response[5] = (uint8_t)(mbap_length & 0xFF);
    response[6] = unit_id_;
    response[7] = function_code;
    response[8] = (uint8_t)byte_count;

    for (uint16_t i = 0; i < quantity; i++) {
      uint16_t val = read_holding_register(start_addr + i);
      response[9 + i * 2]     = (uint8_t)(val >> 8);
      response[9 + i * 2 + 1] = (uint8_t)(val & 0xFF);
    }

    client.write(response, 9 + byte_count);
    return true;

  } else {
    ESP_LOGD(TAG, "Unsupported function code 0x%02X", function_code);
    send_exception_(client, transaction_id, function_code, 0x01);
    return true;
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void ModbusTcpServer::send_exception_(WiFiClient &client, uint16_t transaction_id,
                                      uint8_t function_code, uint8_t exception_code) {
  uint8_t response[9] = {
    (uint8_t)(transaction_id >> 8),
    (uint8_t)(transaction_id & 0xFF),
    0x00, 0x00,
    0x00, 0x03,
    unit_id_,
    (uint8_t)(function_code | 0x80),
    exception_code,
  };
  client.write(response, sizeof(response));
}

bool ModbusTcpServer::write_holding_register(uint16_t address, uint16_t value) {
  registers_[address] = value;
  return true;
}

uint16_t ModbusTcpServer::read_holding_register(uint16_t address) {
  auto it = registers_.find(address);
  return (it != registers_.end()) ? it->second : 0;
}

}  // namespace modbus_tcp_server
}  // namespace esphome
