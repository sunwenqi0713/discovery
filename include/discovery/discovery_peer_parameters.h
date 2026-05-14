#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace discovery {

/// @brief Configuration for a `Peer` instance.
/// @details Controls which role the peer plays (discoverer, discoverable, or
/// both), the UDP port and transport mode (broadcast/multicast), send
/// interval, and how long a silent peer is retained before expiry.
class PeerParameters {
 public:
  /// @brief Determines how discovered peers are deduplicated.
  enum class SamePeerMode {
    /// Match peers by IP address only.
    kIp,
    /// Match peers by both IP address and port.
    kIpAndPort,
  };

  static constexpr SamePeerMode kSamePeerIp = SamePeerMode::kIp;
  static constexpr SamePeerMode kSamePeerIpAndPort = SamePeerMode::kIpAndPort;

  PeerParameters() = default;

  /// @brief Returns the application identifier used for peer filtering.
  uint32_t applicationId() const { return application_id_; }
  /// @brief Sets the application identifier used for peer filtering.
  /// @param application_id Application identifier shared by compatible peers.
  void setApplicationId(uint32_t application_id) { application_id_ = application_id; }

  /// @brief Returns whether UDP broadcast is enabled.
  bool canUseBroadcast() const { return can_use_broadcast_; }
  /// @brief Enables or disables UDP broadcast.
  /// @param can_use_broadcast `true` to enable broadcast.
  void setCanUseBroadcast(bool can_use_broadcast) { can_use_broadcast_ = can_use_broadcast; }

  /// @brief Returns whether UDP multicast is enabled.
  bool canUseMulticast() const { return can_use_multicast_; }
  /// @brief Enables or disables UDP multicast.
  /// @param can_use_multicast `true` to enable multicast.
  void setCanUseMulticast(bool can_use_multicast) { can_use_multicast_ = can_use_multicast; }

  /// @brief Returns the UDP port used for discovery traffic.
  uint16_t port() const { return port_; }
  /// @brief Sets the UDP port used for discovery traffic.
  /// @param port UDP port number.
  void setPort(uint16_t port) { port_ = port; }

  /// @brief Returns the multicast group address in host byte order.
  uint32_t multicastGroupAddress() const { return multicast_group_address_; }
  /// @brief Sets the multicast group address in host byte order.
  /// @param multicast_group_address Multicast group address.
  void setMulticastGroupAddress(uint32_t multicast_group_address) {
    multicast_group_address_ = multicast_group_address;
  }

  /// @brief Returns the send interval.
  std::chrono::milliseconds sendTimeout() const { return send_timeout_; }
  /// @brief Sets the send interval.
  /// @param timeout Non-negative send interval.
  void setSendTimeout(std::chrono::milliseconds timeout) {
    if (timeout.count() >= 0) {
      send_timeout_ = timeout;
    }
  }

  /// @brief Returns the send interval in milliseconds.
  int64_t sendTimeoutMs() const { return send_timeout_.count(); }
  /// @brief Sets the send interval in milliseconds.
  /// @param timeout_ms Non-negative send interval in milliseconds.
  void setSendTimeoutMs(int64_t timeout_ms) {
    if (timeout_ms >= 0) {
      send_timeout_ = std::chrono::milliseconds(timeout_ms);
    }
  }

  /// @brief Returns the time-to-live for discovered peers.
  std::chrono::milliseconds discoveredPeerTtl() const { return discovered_peer_ttl_; }
  /// @brief Sets the time-to-live for discovered peers.
  /// @param ttl Non-negative peer TTL.
  void setDiscoveredPeerTtl(std::chrono::milliseconds ttl) {
    if (ttl.count() >= 0) {
      discovered_peer_ttl_ = ttl;
    }
  }

  /// @brief Returns the peer TTL in milliseconds.
  int64_t discoveredPeerTtlMs() const { return discovered_peer_ttl_.count(); }
  /// @brief Sets the peer TTL in milliseconds.
  /// @param ttl_ms Non-negative peer TTL in milliseconds.
  void setDiscoveredPeerTtlMs(int64_t ttl_ms) {
    if (ttl_ms >= 0) {
      discovered_peer_ttl_ = std::chrono::milliseconds(ttl_ms);
    }
  }

  /// @brief Returns whether this peer advertises itself.
  bool canBeDiscovered() const { return can_be_discovered_; }
  /// @brief Enables or disables advertising this peer.
  /// @param can_be_discovered `true` to advertise this peer.
  void setCanBeDiscovered(bool can_be_discovered) { can_be_discovered_ = can_be_discovered; }

  /// @brief Returns whether this peer listens for others.
  bool canDiscover() const { return can_discover_; }
  /// @brief Enables or disables discovery of other peers.
  /// @param can_discover `true` to listen for peers.
  void setCanDiscover(bool can_discover) { can_discover_ = can_discover; }

  /// @brief Returns whether packets sent by this peer are processed locally.
  bool discoverSelf() const { return discover_self_; }
  /// @brief Enables or disables self-discovery.
  /// @param discover_self `true` to include this peer in discovery results.
  void setDiscoverSelf(bool discover_self) { discover_self_ = discover_self; }

  /// @brief Returns the deduplication mode used for discovered peers.
  SamePeerMode samePeerMode() const { return same_peer_mode_; }
  /// @brief Sets the deduplication mode used for discovered peers.
  /// @param same_peer_mode Deduplication mode.
  void setSamePeerMode(SamePeerMode same_peer_mode) { same_peer_mode_ = same_peer_mode; }

  /// @brief Validates parameter consistency.
  /// @return An empty string when valid; otherwise a human-readable error.
  std::string validate() const {
    if (port_ == 0) return "port must not be 0";
    if (!can_use_broadcast_ && !can_use_multicast_) return "at least one of broadcast or multicast must be enabled";
    if (!can_discover_ && !can_be_discovered_)
      return "at least one of can_discover or can_be_discovered must be enabled";
    if (can_use_multicast_ && (multicast_group_address_ & 0xF0000000u) != 0xE0000000u)
      return "multicast_group_address is not in the 224.0.0.0/4 range";
    return {};
  }

 private:
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
