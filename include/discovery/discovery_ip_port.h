#pragma once

#include <cstdint>
#include <string>

namespace discovery {

/// @brief Compact IP address and port pair identifying a network endpoint.
/// @details The IP address is stored as a host-byte-order `uint32_t`.
class IpPort {
 public:
  IpPort() = default;
  IpPort(uint32_t ip, uint16_t port) : ip_(ip), port_(port) {}

  /// @brief Sets the IP address in host byte order.
  /// @param ip IPv4 address in host byte order.
  void setIp(uint32_t ip) { ip_ = ip; }
  /// @brief Returns the IP address in host byte order.
  uint32_t ip() const { return ip_; }

  /// @brief Sets the port.
  /// @param port UDP/TCP port number.
  void setPort(uint16_t port) { port_ = port; }
  /// @brief Returns the port.
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

/// @brief Converts a host-byte-order IP address to dotted decimal notation.
/// @param ip IPv4 address in host byte order.
/// @return A string in `"A.B.C.D"` form.
std::string ipToString(uint32_t ip);

/// @brief Converts an endpoint to `"A.B.C.D:port"` notation.
/// @param ip_port Endpoint to format.
/// @return Formatted endpoint string.
std::string ipPortToString(const IpPort& ip_port);

}  // namespace discovery
