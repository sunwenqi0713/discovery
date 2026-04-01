#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <string>
#include <thread>

#include "discovery_discovered_peer.h"
#include "discovery_peer_parameters.h"

namespace discovery {

namespace impl {

// Returns the current time as milliseconds since an unspecified epoch.
int64_t NowTime();

// Suspends the calling thread for the given duration.
void SleepFor(std::chrono::milliseconds duration);

// Overload accepting raw milliseconds for convenience.
inline void SleepFor(int64_t time_ms) { SleepFor(std::chrono::milliseconds(time_ms)); }

// Internal interface between Peer and the socket/threading environment.
// Abstracted here to support dependency injection in tests.
class PeerEnvInterface {
 public:
  virtual ~PeerEnvInterface() = default;

  virtual void SetUserData(const std::string& user_data) = 0;
  virtual std::list<DiscoveredPeer> ListDiscovered() = 0;
  virtual void Exit() = 0;
};

}  // namespace impl

// Represents a participant in the peer discovery protocol.
//
// A Peer can broadcast its own presence, listen for other peers, or both,
// depending on the PeerParameters it is started with. All public methods are
// thread-safe after Start() returns successfully.
class Peer {
 public:
  Peer();
  ~Peer();

  Peer(const Peer&) = delete;             // Non-copyable.
  Peer& operator=(const Peer&) = delete;  // Non-copyable.
  Peer(Peer&&) = delete;                  // Non-movable.
  Peer& operator=(Peer&&) = delete;       // Non-movable.

  // Starts the peer with the given parameters and initial user data.
  // Stops any previously running peer first. Returns false if the parameters
  // are invalid or socket setup fails.
  bool Start(const PeerParameters& parameters, const std::string& user_data);

  // Updates the user data broadcast to other peers. May be called at any
  // time after Start(); the change takes effect on the next send interval.
  void SetUserData(const std::string& user_data);

  // Returns a snapshot of all currently discovered peers.
  std::list<DiscoveredPeer> ListDiscovered() const;

  // Signals the peer to stop and returns immediately. Background threads
  // will finish on their own after sending a departure packet.
  void Stop();

  // Signals the peer to stop and blocks until all background threads exit.
  void StopAndWaitForThreads();

  // Deprecated: use Stop() or StopAndWaitForThreads() instead.
  [[deprecated("Use Stop() or StopAndWaitForThreads() instead.")]]
  void Stop(bool wait_for_threads);

 private:
  void StopImpl(bool wait_for_threads);

  std::shared_ptr<impl::PeerEnvInterface> env_;
  std::unique_ptr<std::thread> sending_thread_;
  std::unique_ptr<std::thread> receiving_thread_;
};

// Returns true if lhv and rhv refer to the same peer under the given mode.
bool Same(PeerParameters::SamePeerMode mode, const IpPort& lhv, const IpPort& rhv);

// Returns true if lhv and rhv contain exactly the same set of peers.
bool Same(PeerParameters::SamePeerMode mode, const std::list<DiscoveredPeer>& lhv,
          const std::list<DiscoveredPeer>& rhv);

}  // namespace discovery
