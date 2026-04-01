#pragma once

#include <cstdint>
#include <string>

#include "discovery_ip_port.h"

namespace discovery {

// Represents a remote peer that has been discovered on the network.
//
// Holds the peer's address, its most recently received user data, and
// the timestamp of the last received packet (used for TTL expiry).
class DiscoveredPeer {
 public:
  DiscoveredPeer() = default;

  const IpPort& ip_port() const { return ip_port_; }
  void set_ip_port(const IpPort& ip_port) { ip_port_ = ip_port; }

  const std::string& user_data() const { return user_data_; }

  // Returns the snapshot_index of the last packet that updated user_data.
  // Used to discard stale packets that arrive out of order.
  uint64_t last_received_packet() const { return last_received_packet_; }

  // Updates user_data only when snapshot_index is newer than the last seen one.
  void SetUserData(const std::string& user_data, uint64_t last_received_packet) {
    user_data_ = user_data;
    last_received_packet_ = last_received_packet;
  }

  // Timestamp (ms) of the most recently received packet from this peer.
  int64_t last_updated() const { return last_updated_; }
  void set_last_updated(int64_t last_updated) { last_updated_ = last_updated; }

 private:
  IpPort ip_port_;
  std::string user_data_;
  uint64_t last_received_packet_ = 0;
  int64_t last_updated_ = 0;
};

}  // namespace discovery
