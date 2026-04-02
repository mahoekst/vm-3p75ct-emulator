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

  void setup() override;
  void loop() override;

  // Priority: start after WiFi is up
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Register access — called from ESPHome sensor lambdas
  bool write_holding_register(uint16_t address, uint16_t value);
  uint16_t read_holding_register(uint16_t address);

 protected:
  uint16_t port_{502};
  uint8_t unit_id_{1};
  WiFiServer *server_{nullptr};

  // Sparse register map — only allocated addresses consume memory
  std::map<uint16_t, uint16_t> registers_;

  void handle_client_(WiFiClient &client);
  void handle_request_(WiFiClient &client);
  void send_exception_(WiFiClient &client, uint16_t transaction_id,
                       uint8_t function_code, uint8_t exception_code);
};

}  // namespace modbus_tcp_server
}  // namespace esphome
