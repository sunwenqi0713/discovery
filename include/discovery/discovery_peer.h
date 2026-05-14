#pragma once

#include <list>
#include <memory>
#include <string>
#include <thread>

#include "discovery_discovered_peer.h"
#include "discovery_peer_parameters.h"

namespace discovery {

namespace impl {

/// @brief Internal interface between `Peer` and the socket/threading environment.
/// @details Abstracted to support dependency injection in tests.
class PeerEnvInterface {
 public:
  virtual ~PeerEnvInterface() = default;

  /// @brief Updates the user data advertised by the peer.
  /// @param user_data User-defined payload to advertise.
  virtual void setUserData(const std::string& user_data) = 0;

  /// @brief Returns a snapshot of the currently discovered peers.
  /// @return A copy of the discovered peer list.
  virtual std::list<DiscoveredPeer> listDiscovered() = 0;

  /// @brief Signals the environment to stop its background work.
  virtual void requestExit() = 0;
};

}  // namespace impl

/// @brief Represents a participant in the peer discovery protocol.
/// @details A `Peer` can broadcast its own presence, listen for other peers,
/// or both, depending on the `PeerParameters` used to start it. All public
/// methods are thread-safe after `start()` returns successfully.
class Peer {
 public:
  Peer();
  ~Peer();

  Peer(const Peer&) = delete;             // Non-copyable.
  Peer& operator=(const Peer&) = delete;  // Non-copyable.
  Peer(Peer&&) = delete;                  // Non-movable.
  Peer& operator=(Peer&&) = delete;       // Non-movable.

  /// @brief Starts the peer with the given parameters and initial user data.
  /// @details Stops any previously running peer before starting a new session.
  /// @param parameters Runtime configuration for discovery behavior.
  /// @param user_data Initial payload advertised to other peers.
  /// @return `true` if startup succeeds, otherwise `false`.
  bool start(const PeerParameters& parameters, const std::string& user_data);

  /// @brief Updates the user data broadcast to other peers.
  /// @param user_data New payload to advertise.
  /// @details The change takes effect on the next send interval.
  void setUserData(const std::string& user_data);

  /// @brief Returns a snapshot of all currently discovered peers.
  /// @return A copy of the discovered peer list.
  std::list<DiscoveredPeer> listDiscovered() const;

  /// @brief Signals the peer to stop and returns immediately.
  /// @details Background threads finish asynchronously after sending a
  /// departure packet.
  void stop();

  /// @brief Signals the peer to stop and waits for all background threads.
  void stopAndWait();

 private:
  void stopImpl(bool wait_for_threads);

  std::shared_ptr<impl::PeerEnvInterface> env_;
  std::unique_ptr<std::thread> sending_thread_;
  std::unique_ptr<std::thread> receiving_thread_;
};

/// @brief Tests whether two endpoints represent the same peer.
/// @param mode Deduplication mode to apply.
/// @param lhs Left-hand endpoint.
/// @param rhs Right-hand endpoint.
/// @return `true` if the endpoints are considered equal under `mode`.
bool isSame(PeerParameters::SamePeerMode mode, const IpPort& lhs, const IpPort& rhs);

/// @brief Tests whether two peer lists contain the same peer set.
/// @param mode Deduplication mode to apply.
/// @param lhs Left-hand peer list.
/// @param rhs Right-hand peer list.
/// @return `true` if both lists contain the same peers under `mode`.
bool isSame(PeerParameters::SamePeerMode mode, const std::list<DiscoveredPeer>& lhs,
            const std::list<DiscoveredPeer>& rhs);

}  // namespace discovery
