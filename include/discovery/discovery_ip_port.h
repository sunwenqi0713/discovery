#pragma once

#include <cstdint>
#include <string>

namespace discovery {

// A compact IP address and port pair used to identify a network endpoint.
// The IP is stored as a host-byte-order uint32_t.
class IpPort {
 public:
  IpPort() = default;
  IpPort(uint32_t ip, uint16_t port) : ip_(ip), port_(port) {}

  void set_ip(uint32_t ip) { ip_ = ip; }
  uint32_t ip() const { return ip_; }

  void set_port(uint16_t port) { port_ = port; }
  uint16_t port() const { return port_; }

  bool operator==(const IpPort& other) const { return ip_ == other.ip_ && port_ == other.port_; }
  bool operator!=(const IpPort& other) const { return !(*this == other); }
  bool operator<(const IpPort& other) const {
    if (ip_ < other.ip_) return true;
    if (ip_ > other.ip_) return false;
    return port_ < other.port_;
  }

 private:
  uint32_t ip_ = 0;
  uint16_t port_ = 0;
};

// Converts a host-byte-order IP address to "A.B.C.D" notation.
std::string IpToString(uint32_t ip);

// Converts an IpPort to "A.B.C.D:port" notation.
std::string IpPortToString(const IpPort& ip_port);

}  // namespace discovery
