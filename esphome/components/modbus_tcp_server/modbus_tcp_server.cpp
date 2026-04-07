#include "modbus_tcp_server.h"
#include "esphome/core/log.h"
#include <WiFi.h>

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
//
// One client is handled at a time. The active client is stored in client_ so
// that loop() returns to ESPHome between every request, keeping all other
// components (HA API, web server, sensors) running normally.
// ---------------------------------------------------------------------------

void ModbusTcpServer::loop() {
  if (!server_)
    return;

  // Restart the server socket when WiFi reconnects after a drop.
  // On ESP32 Arduino the lwIP stack resets on reconnect, invalidating the
  // existing server socket — begin() must be called again.
  bool wifi_connected = (WiFi.status() == WL_CONNECTED);
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
// Modbus TCP frame parser — handles a single FC03 request/response exchange.
// Returns true if a valid Modbus request was received and a response was sent.
//
// Modbus TCP ADU layout:
//   [Transaction ID : 2 bytes]
//   [Protocol ID    : 2 bytes] (always 0x0000)
//   [Length         : 2 bytes] (bytes from Unit ID to end of PDU)
//   [Unit ID        : 1 byte ]
//   [PDU            : N bytes] (Function Code + Data)
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

  // Guard: length must be at least 2 (unit ID byte + at least 1 PDU byte).
  // Without this check, length=0 would underflow to 65535 on the next line.
  if (length < 2) {
    ESP_LOGW(TAG, "MBAP length too small: %d", length);
    return false;
  }

  // PDU length = Length field minus the 1-byte Unit ID
  uint16_t pdu_len = length - 1;
  if (pdu_len > 253) {
    ESP_LOGW(TAG, "PDU length %d exceeds Modbus maximum of 253", pdu_len);
    return false;
  }

  uint8_t pdu[253];
  if (client.readBytes(pdu, pdu_len) != pdu_len)
    return false;

  uint8_t function_code = pdu[0];

  if (function_code == 0x03) {
    // Read Holding Registers: [FC=0x03][Start_H][Start_L][Qty_H][Qty_L]
    if (pdu_len < 5) {
      send_exception_(client, transaction_id, function_code, 0x03 /* illegal data value */);
      return true;
    }

    uint16_t start_addr = ((uint16_t)pdu[1] << 8) | pdu[2];
    uint16_t quantity   = ((uint16_t)pdu[3] << 8) | pdu[4];

    if (quantity == 0 || quantity > 125) {
      send_exception_(client, transaction_id, function_code, 0x03);
      return true;
    }

    // Guard: ensure start_addr + quantity does not wrap the 16-bit address space.
    // Without this, reading near address 65535 would silently wrap back to 0.
    if ((uint32_t)start_addr + (uint32_t)quantity > 0x10000U) {
      send_exception_(client, transaction_id, function_code, 0x02 /* illegal data address */);
      return true;
    }

    ESP_LOGD(TAG, "FC03 addr=%d qty=%d", start_addr, quantity);

    // Build response:
    // MBAP (6) + Unit ID (1) + FC (1) + ByteCount (1) + registers (qty*2)
    uint16_t byte_count  = quantity * 2;
    uint16_t mbap_length = 3 + byte_count;  // unit_id + FC + byte_count + data

    uint8_t response[9 + 125 * 2];
    response[0] = (uint8_t)(transaction_id >> 8);
    response[1] = (uint8_t)(transaction_id & 0xFF);
    response[2] = 0x00;
    response[3] = 0x00;
    response[4] = (uint8_t)(mbap_length >> 8);
    response[5] = (uint8_t)(mbap_length & 0xFF);
    response[6] = unit_id_;
    response[7] = 0x03;
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
    send_exception_(client, transaction_id, function_code, 0x01 /* illegal function */);
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
    0x00, 0x00,   // protocol ID
    0x00, 0x03,   // length = 3 (unit_id + error FC + exception code)
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
