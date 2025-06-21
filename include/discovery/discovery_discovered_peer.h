#pragma once

#include <cstdint>
#include <string>

#include "discovery_ip_port.h"

namespace discovery {

class DiscoveredPeer {
 public:
  DiscoveredPeer() = default;

  const IpPort& ip_port() const { return ip_port_; }
  void set_ip_port(const IpPort& ip_port) { ip_port_ = ip_port; }

  const std::string& user_data() const { return user_data_; }

  uint64_t last_received_packet() const { return last_received_packet_; }

  void SetUserData(const std::string& user_data, uint64_t last_received_packet) {
    user_data_ = user_data;
    last_received_packet_ = last_received_packet;
  }

  int64_t last_updated() const { return last_updated_; }
  void set_last_updated(int64_t last_updated) { last_updated_ = last_updated; }

 private:
  IpPort ip_port_;
  std::string user_data_;
  uint64_t last_received_packet_ = 0;
  int64_t last_updated_ = 0;
};

}  // namespace discovery
