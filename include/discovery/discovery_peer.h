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

// Returns current time in milliseconds since some unspecified epoch
int64_t NowTime();

// Sleep for the specified duration
void SleepFor(std::chrono::milliseconds duration);

// For backward compatibility
inline void SleepFor(int64_t time_ms) { SleepFor(std::chrono::milliseconds(time_ms)); }

class PeerEnvInterface {
 public:
  virtual ~PeerEnvInterface() = default;

  virtual void SetUserData(const std::string& user_data) = 0;
  virtual std::list<DiscoveredPeer> ListDiscovered() = 0;
  virtual void Exit() = 0;
};

}  // namespace impl

class Peer {
 public:
  Peer();
  ~Peer();

  // Non-copyable
  Peer(const Peer&) = delete;
  Peer& operator=(const Peer&) = delete;

  // Non-movable (due to internal threading)
  Peer(Peer&&) = delete;
  Peer& operator=(Peer&&) = delete;

  /**
   * \brief Starts discovery peer.
   */
  bool Start(const PeerParameters& parameters, const std::string& user_data);

  /**
   * \brief Sets user data of the started discovery peer.
   */
  void SetUserData(const std::string& user_data);

  /**
   * \brief Lists all discovered peers.
   */
  std::list<DiscoveredPeer> ListDiscovered() const;

  /**
   * \brief Stops discovery peer immediately. Working threads will finish
   * execution later.
   */
  void Stop();

  /**
   * \brief Stops discovery peer and wait for all working threads to finish
   * execution.
   */
  void StopAndWaitForThreads();

  /**
   * \brief Stops discovery peer and potentially waits for all working threads
   * to finish execution.
   *
   * \deprecated This function is deprecated. Use Stop()
   * and StopAndWaitForThreads() instead.
   */
  void Stop(bool wait_for_threads);

 private:
  std::shared_ptr<impl::PeerEnvInterface> env_;
  std::unique_ptr<std::thread> sending_thread_;
  std::unique_ptr<std::thread> receiving_thread_;
};

bool Same(PeerParameters::SamePeerMode mode, const IpPort& lhv, const IpPort& rhv);
bool Same(PeerParameters::SamePeerMode mode, const std::list<DiscoveredPeer>& lhv,
          const std::list<DiscoveredPeer>& rhv);

}  // namespace discovery
