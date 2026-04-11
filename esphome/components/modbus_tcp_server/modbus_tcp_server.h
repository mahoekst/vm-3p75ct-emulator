#pragma once

#include "esphome/core/component.h"
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <map>

namespace esphome {
namespace modbus_tcp_server {

class ModbusTcpServer : public Component {
 public:
  void set_port(uint16_t port) { port_ = port; }
  void set_unit_id(uint8_t unit_id) { unit_id_ = unit_id; }
  void set_phases(uint8_t phases) { phases_ = phases; }
  void set_serial_number(const char *serial) { serial_number_ = serial; }

  void setup() override;
  void loop() override;

  // Priority: start after WiFi is up
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // ── Typed setters — called from generated HA sensor subscriptions ────────
  // phase: 1 = L1, 2 = L2, 3 = L3
  void set_voltage(uint8_t phase, float volts);    // L-N, s32l ÷10
  void set_ll_voltage(uint8_t phase, float volts); // L-L — no-op (EM24 doesn't expose via Modbus)
  void set_current(uint8_t phase, float amps);     // s32l ÷1000
  void set_power(uint8_t phase, float watts);      // s32l ÷10; total auto-updated
  void set_frequency(float hz);                    // u16 ÷10
  void set_energy_import(float kwh);               // total s32l ÷10 → 0x0034
  void set_energy_export(float kwh);               // total s32l ÷10 → 0x004e
  void set_phase_energy_import(uint8_t phase, float kwh); // per-phase s32l ÷10 → 0x0040/42/44

  // Typed getters — for template sensors in the YAML web UI
  float get_voltage(uint8_t phase);
  float get_current(uint8_t phase);
  float get_power(uint8_t phase);
  float get_total_power();
  float get_frequency();
  float get_energy_import();
  float get_energy_export();
  float get_phase_energy_import(uint8_t phase);

  // Low-level register access — still available for on_boot dummy values
  bool write_holding_register(uint16_t address, uint16_t value);
  uint16_t read_holding_register(uint16_t address);

 protected:
  uint16_t port_{502};
  uint8_t unit_id_{1};
  uint8_t phases_{3};
  const char *serial_number_{"00000000000000"};
  WiFiServer *server_{nullptr};

  // Track WiFi state to detect reconnects and restart the server socket
  bool wifi_was_connected_{false};
  // Set to true once server_->begin() has succeeded at least once
  bool server_started_{false};

  // Active client state — persists across loop() calls (non-blocking design)
  WiFiClient client_;
  uint32_t client_start_ms_{0};

  // Sparse register map — only allocated addresses consume memory
  std::map<uint16_t, uint16_t> registers_;

  // Cached per-phase power (raw, ÷10 → W) for total register recalculation
  int32_t raw_power_[3]{0, 0, 0};

  // Helper: write a 32-bit signed value as two 16-bit little-endian words
  void write_s32l_(uint16_t base_addr, int32_t value);
  // Helper: recalculate and write total power register (0x0028-29)
  void update_total_power_();

  void start_server_();
  bool handle_request_(WiFiClient &client);
  void send_exception_(WiFiClient &client, uint16_t transaction_id,
                       uint8_t function_code, uint8_t exception_code);
};

}  // namespace modbus_tcp_server
}  // namespace esphome
