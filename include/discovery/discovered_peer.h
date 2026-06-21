#pragma once

#include <cstdint>
#include <string>

#include "ip_port.h"

namespace discovery {

/// @brief Represents a remote peer discovered on the network.
/// @details Stores the peer address, its most recently received user data, and
/// the timestamp of the last received packet used for TTL expiry.
class DiscoveredPeer {
 public:
  DiscoveredPeer() = default;

  /// @brief Returns the peer endpoint.
  const IpPort& ipPort() const { return ip_port_; }
  /// @brief Sets the peer endpoint.
  /// @param ip_port Peer endpoint.
  void setIpPort(const IpPort& ip_port) { ip_port_ = ip_port; }

  /// @brief Returns the latest advertised user data.
  const std::string& userData() const { return user_data_; }

  /// @brief Returns the snapshot index that last updated the user data.
  /// @details Useful for out-of-order detection and debugging.
  uint64_t lastReceivedPacket() const { return last_received_packet_; }

  /// @brief Initializes user data for a newly discovered peer.
  /// @param user_data User-defined payload.
  /// @param snapshot_index Snapshot index of the packet.
  void initUserData(const std::string& user_data, uint64_t snapshot_index) {
    user_data_ = user_data;
    last_received_packet_ = snapshot_index;
  }

  /// @brief Updates user data when the incoming snapshot index is newer.
  /// @param user_data User-defined payload.
  /// @param snapshot_index Snapshot index of the packet.
  /// @return `true` if the stored payload was updated.
  bool tryUpdateUserData(const std::string& user_data, uint64_t snapshot_index) {
    if (snapshot_index <= last_received_packet_) return false;
    user_data_ = user_data;
    last_received_packet_ = snapshot_index;
    return true;
  }

  /// @brief Returns the timestamp of the most recently received packet.
  int64_t lastUpdated() const { return last_updated_; }
  /// @brief Sets the timestamp of the most recently received packet.
  /// @param last_updated Timestamp in milliseconds.
  void setLastUpdated(int64_t last_updated) { last_updated_ = last_updated; }

 private:
  IpPort ip_port_;
  std::string user_data_;
  uint64_t last_received_packet_ = 0;
  int64_t last_updated_ = 0;
};

}  // namespace discovery
