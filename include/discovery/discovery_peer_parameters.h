#pragma once

#include <chrono>
#include <cstdint>

#include "discovery_protocol_version.h"

namespace discovery {

class PeerParameters {
 public:
  enum class SamePeerMode {
    kIp,
    kIpAndPort,
  };

  // Convenience aliases for backward compatibility
  static constexpr SamePeerMode kSamePeerIp = SamePeerMode::kIp;
  static constexpr SamePeerMode kSamePeerIpAndPort = SamePeerMode::kIpAndPort;

  PeerParameters() = default;

  ProtocolVersion min_supported_protocol_version() const { return min_supported_protocol_version_; }

  ProtocolVersion max_supported_protocol_version() const { return max_supported_protocol_version_; }

  void set_supported_protocol_version(ProtocolVersion version) {
    min_supported_protocol_version_ = version;
    max_supported_protocol_version_ = version;
  }

  void set_supported_protocol_versions(ProtocolVersion min_version, ProtocolVersion max_version) {
    min_supported_protocol_version_ = min_version;
    max_supported_protocol_version_ = max_version;
  }

  uint32_t application_id() const { return application_id_; }
  void set_application_id(uint32_t application_id) { application_id_ = application_id; }

  bool can_use_broadcast() const { return can_use_broadcast_; }
  void set_can_use_broadcast(bool can_use_broadcast) { can_use_broadcast_ = can_use_broadcast; }

  bool can_use_multicast() const { return can_use_multicast_; }
  void set_can_use_multicast(bool can_use_multicast) { can_use_multicast_ = can_use_multicast; }

  uint16_t port() const { return port_; }
  void set_port(uint16_t port) { port_ = port; }

  uint32_t multicast_group_address() const { return multicast_group_address_; }
  void set_multicast_group_address(uint32_t group_address) { multicast_group_address_ = group_address; }

  std::chrono::milliseconds send_timeout() const { return send_timeout_; }
  void set_send_timeout(std::chrono::milliseconds timeout) {
    if (timeout.count() >= 0) {
      send_timeout_ = timeout;
    }
  }

  // For backward compatibility
  int64_t send_timeout_ms() const { return send_timeout_.count(); }
  void set_send_timeout_ms(int64_t timeout_ms) {
    if (timeout_ms >= 0) {
      send_timeout_ = std::chrono::milliseconds(timeout_ms);
    }
  }

  std::chrono::milliseconds discovered_peer_ttl() const { return discovered_peer_ttl_; }
  void set_discovered_peer_ttl(std::chrono::milliseconds ttl) {
    if (ttl.count() >= 0) {
      discovered_peer_ttl_ = ttl;
    }
  }

  // For backward compatibility
  int64_t discovered_peer_ttl_ms() const { return discovered_peer_ttl_.count(); }
  void set_discovered_peer_ttl_ms(int64_t ttl_ms) {
    if (ttl_ms >= 0) {
      discovered_peer_ttl_ = std::chrono::milliseconds(ttl_ms);
    }
  }

  bool can_be_discovered() const { return can_be_discovered_; }
  void set_can_be_discovered(bool can_be_discovered) { can_be_discovered_ = can_be_discovered; }

  bool can_discover() const { return can_discover_; }
  void set_can_discover(bool can_discover) { can_discover_ = can_discover; }

  bool discover_self() const { return discover_self_; }
  void set_discover_self(bool discover_self) { discover_self_ = discover_self; }

  SamePeerMode same_peer_mode() const { return same_peer_mode_; }
  void set_same_peer_mode(SamePeerMode same_peer_mode) { same_peer_mode_ = same_peer_mode; }

 private:
  ProtocolVersion min_supported_protocol_version_ = kProtocolVersionCurrent;
  ProtocolVersion max_supported_protocol_version_ = kProtocolVersionCurrent;
  uint32_t application_id_ = 0;
  bool can_use_broadcast_ = true;
  bool can_use_multicast_ = false;
  uint16_t port_ = 0;
  uint32_t multicast_group_address_ = 0;
  std::chrono::milliseconds send_timeout_{5000};
  std::chrono::milliseconds discovered_peer_ttl_{10000};
  bool can_be_discovered_ = false;
  bool can_discover_ = false;
  bool discover_self_ = false;
  SamePeerMode same_peer_mode_ = SamePeerMode::kIpAndPort;
};

}  // namespace discovery
