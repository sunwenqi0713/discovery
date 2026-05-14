#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <thread>

#include "discovery/discovery_peer.h"
#include "discovery/discovery_protocol.h"

// ============================================================================
// Platform socket abstraction
// ============================================================================

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketFd = SOCKET;
using SocklenType = int;
constexpr SocketFd kInvalidSocket = INVALID_SOCKET;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using SocketFd = int;
using SocklenType = socklen_t;
constexpr SocketFd kInvalidSocket = -1;
#endif

namespace {

void initSocketLibrary() {
#if defined(_WIN32)
  static std::once_flag once;
  std::call_once(once, [] {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
  });
#endif
}

void closeSocket(SocketFd fd) {
#if defined(_WIN32)
  closesocket(fd);
#else
  close(fd);
#endif
}

void setReceiveTimeout(SocketFd fd, int ms) {
#if defined(_WIN32)
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
  timeval tv{ms / 1000, 1000 * (ms % 1000)};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
}

/// @brief Returns the most recent socket error as `"[code] description"`.
std::string lastSocketError() {
#if defined(_WIN32)
  int code = WSAGetLastError();
  char buf[256] = {};
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(code), 0, buf,
                 sizeof(buf), nullptr);
  std::string msg(buf);
  while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ')) msg.pop_back();
  return "[" + std::to_string(code) + "] " + msg;
#else
  return "[" + std::to_string(errno) + "] " + strerror(errno);
#endif
}

/// @brief Reports whether the last socket error was a receive timeout.
/// @return `true` when the most recent socket error represents a timeout.
bool isSocketTimeout() {
#if defined(_WIN32)
  return WSAGetLastError() == WSAETIMEDOUT;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

uint32_t generateRandomId() {
  static std::mutex mtx;
  static std::mt19937 rng{std::random_device{}()};
  static std::uniform_int_distribution<uint32_t> dist;
  std::lock_guard<std::mutex> lock(mtx);
  return dist(rng);
}

int64_t nowTime() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void sleepFor(std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); }

/// @brief RAII owner of a socket file descriptor.
struct SocketHandle {
  SocketFd fd = kInvalidSocket;

  SocketHandle() = default;
  explicit SocketHandle(SocketFd fd) : fd(fd) {}
  ~SocketHandle() { reset(); }

  SocketHandle(const SocketHandle&) = delete;
  SocketHandle& operator=(const SocketHandle&) = delete;

  SocketHandle(SocketHandle&& other) noexcept : fd(other.fd) { other.fd = kInvalidSocket; }
  SocketHandle& operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
      reset();
      fd = other.fd;
      other.fd = kInvalidSocket;
    }
    return *this;
  }

  bool isValid() const { return fd != kInvalidSocket; }

  void reset() {
    if (isValid()) {
      closeSocket(fd);
      fd = kInvalidSocket;
    }
  }
};

/// @brief Tracks when a periodic action should run again.
class Interval {
 public:
  explicit Interval(int64_t periodMs) : periodMs_(periodMs) {}

  /// @brief Returns whether the interval should fire at `nowMs`.
  bool isReady(int64_t nowMs) const { return lastMs_ == 0 || (nowMs - lastMs_ >= periodMs_); }

  /// @brief Returns milliseconds until the next firing time.
  int64_t msUntilNext(int64_t nowMs) const {
    if (lastMs_ == 0) return 0;
    return std::max(int64_t{0}, periodMs_ - (nowMs - lastMs_));
  }

  void reset(int64_t nowMs) { lastMs_ = nowMs; }

 private:
  int64_t periodMs_;
  int64_t lastMs_ = 0;
};

}  // namespace

// ============================================================================
// PeerEnv socket ownership and thread loop implementations
// ============================================================================

namespace discovery {
namespace impl {

/// @brief Owns sockets and drives the background send/receive loops.
/// @details Held by `shared_ptr` so worker threads can outlive the `Peer`
/// wrapper safely during shutdown.
class PeerEnv : public PeerEnvInterface, public std::enable_shared_from_this<PeerEnv> {
 public:
  PeerEnv() = default;

  PeerEnv(const PeerEnv&) = delete;
  PeerEnv& operator=(const PeerEnv&) = delete;

  /// @brief Validates parameters and opens sockets.
  /// @param parameters Runtime configuration.
  /// @param userData Initial advertised payload.
  /// @return `true` on success, otherwise `false`.
  bool start(const PeerParameters& parameters, const std::string& userData);

  // ---- PeerEnvInterface ----
  void setUserData(const std::string& userData) override;
  std::list<DiscoveredPeer> listDiscovered() override;
  void requestExit() override;

  // ---- Thread entry points (called from Peer::start) ----
  void sendingThreadFunc();
  void receivingThreadFunc();

 private:
  bool exitRequested() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return exitRequested_;
  }

  /// @brief Opens the send socket.
  bool openSendSocket();
  /// @brief Opens the receive socket.
  bool openReceiveSocket();

  /// @brief Serializes and sends a packet of the given type.
  void sendPacket(PacketType type);

  /// @brief Removes peers whose heartbeat exceeded the configured TTL.
  void evictExpiredPeers(int64_t nowMs);

  /// @brief Parses one received datagram and updates discovered peers.
  void handleReceivedPacket(int64_t nowMs, const IpPort& from, const char* packetData, size_t length);

  PeerParameters parameters_;
  uint32_t peerId_ = 0;
  SocketHandle sendSocket_;  // broadcast-capable, unbound
  SocketHandle receiveSocket_;  // bound to the configured port

  mutable std::mutex mutex_;
  bool exitRequested_ = false;
  uint64_t packetIndex_ = 0;
  std::string userData_;
  std::list<DiscoveredPeer> discoveredPeers_;
};

// ---- Startup and socket setup ----------------------------------------------

bool PeerEnv::start(const PeerParameters& parameters, const std::string& userData) {
  const std::string validationError = parameters.validate();
  if (!validationError.empty()) {
    std::cerr << "discovery::Peer: invalid parameters: " << validationError << "\n";
    return false;
  }

  parameters_ = parameters;
  userData_ = userData;
  peerId_ = generateRandomId();

  initSocketLibrary();

  if (!openSendSocket()) return false;
  if (parameters_.canDiscover() && !openReceiveSocket()) return false;

  return true;
}

bool PeerEnv::openSendSocket() {
  SocketHandle socketHandle{socket(AF_INET, SOCK_DGRAM, 0)};
  if (!socketHandle.isValid()) {
    std::cerr << "discovery::Peer: failed to create send socket: " << lastSocketError() << "\n";
    return false;
  }

  const int enabled = 1;
  if (setsockopt(socketHandle.fd, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) <
      0) {
    // Non-fatal when multicast is also available.
    std::cerr << "discovery::Peer: failed to enable SO_BROADCAST: " << lastSocketError() << "\n";
  }

  sendSocket_ = std::move(socketHandle);
  return true;
}

bool PeerEnv::openReceiveSocket() {
  SocketHandle socketHandle{socket(AF_INET, SOCK_DGRAM, 0)};
  if (!socketHandle.isValid()) {
    std::cerr << "discovery::Peer: failed to create receive socket: " << lastSocketError() << "\n";
    return false;
  }

  const int enabled = 1;
  if (setsockopt(socketHandle.fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) <
      0) {
    std::cerr << "discovery::Peer: failed to set SO_REUSEADDR: " << lastSocketError() << "\n";
  }
#ifdef SO_REUSEPORT
  if (setsockopt(socketHandle.fd, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) <
      0) {
    std::cerr << "discovery::Peer: failed to set SO_REUSEPORT: " << lastSocketError() << "\n";
  }
#endif

  if (parameters_.canUseMulticast()) {
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = htonl(parameters_.multicastGroupAddress());
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(socketHandle.fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) <
        0) {
      std::cerr << "discovery::Peer: failed to join multicast group: " << lastSocketError() << "\n";
      return false;
    }
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(parameters_.port());
  address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(socketHandle.fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    std::cerr << "discovery::Peer: failed to bind receive socket: " << lastSocketError() << "\n";
    return false;
  }

  /// @note A 1-second timeout keeps shutdown polling simple.
  /// TODO: Replace with a pipe/eventfd-style wakeup for faster shutdown.
  setReceiveTimeout(socketHandle.fd, 1000);

  receiveSocket_ = std::move(socketHandle);
  return true;
}

// ---- PeerEnvInterface ------------------------------------------------------

void PeerEnv::setUserData(const std::string& userData) {
  std::lock_guard<std::mutex> lock(mutex_);
  userData_ = userData;
}

std::list<DiscoveredPeer> PeerEnv::listDiscovered() {
  std::lock_guard<std::mutex> lock(mutex_);
  return discoveredPeers_;
}

void PeerEnv::requestExit() {
  std::lock_guard<std::mutex> lock(mutex_);
  exitRequested_ = true;
}

// ---- Thread functions ------------------------------------------------------

void PeerEnv::sendingThreadFunc() {
  Interval announceInterval{parameters_.sendTimeoutMs()};
  Interval cleanupInterval{parameters_.discoveredPeerTtlMs()};

  while (!exitRequested()) {
    const int64_t nowMs = nowTime();
    int64_t sleepMs = std::numeric_limits<int64_t>::max();

    if (parameters_.canBeDiscovered()) {
      if (announceInterval.isReady(nowMs)) {
        sendPacket(kPacketIAmHere);
        announceInterval.reset(nowMs);
      }
      sleepMs = std::min(sleepMs, announceInterval.msUntilNext(nowMs));
    }

    if (parameters_.canDiscover()) {
      if (cleanupInterval.isReady(nowMs)) {
        evictExpiredPeers(nowMs);
        cleanupInterval.reset(nowMs);
      }
      sleepMs = std::min(sleepMs, cleanupInterval.msUntilNext(nowMs));
    }

    if (sleepMs > 0 && sleepMs != std::numeric_limits<int64_t>::max()) {
      sleepFor(std::chrono::milliseconds(sleepMs));
    }
  }

  sendPacket(kPacketIAmOutOfHere);
}

void PeerEnv::receivingThreadFunc() {
  std::string buffer(kMaxPacketSize, '\0');

  while (true) {
    sockaddr_in fromAddress{};
    SocklenType fromLength = sizeof(fromAddress);

    const auto bytesReceived = recvfrom(receiveSocket_.fd, buffer.data(), static_cast<int>(kMaxPacketSize), 0,
                                        reinterpret_cast<sockaddr*>(&fromAddress), &fromLength);

    if (exitRequested()) return;

    if (bytesReceived <= 0) {
      if (!isSocketTimeout()) {
        std::cerr << "discovery::Peer: recvfrom error: " << lastSocketError() << "\n";
      }
      continue;
    }

    IpPort from{ntohl(fromAddress.sin_addr.s_addr), ntohs(fromAddress.sin_port)};
    handleReceivedPacket(nowTime(), from, buffer.data(), static_cast<size_t>(bytesReceived));
  }
}

// ---- Private helpers -------------------------------------------------------

void PeerEnv::sendPacket(PacketType type) {
  if (!sendSocket_.isValid()) return;

  std::string userData;
  uint64_t packetIndex = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    userData = userData_;
    packetIndex = packetIndex_++;
  }

  Packet packet;
  packet.setPacketType(type);
  packet.setApplicationId(parameters_.applicationId());
  packet.setPeerId(peerId_);
  packet.setSnapshotIndex(packetIndex);
  packet.swapUserData(userData);

  std::string wire;
  if (!packet.serialize(wire)) return;

  const auto sendTo = [&](uint32_t destinationIp) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(parameters_.port());
    address.sin_addr.s_addr = htonl(destinationIp);
    if (sendto(sendSocket_.fd, wire.data(), static_cast<int>(wire.size()), 0, reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) < 0) {
      std::cerr << "discovery::Peer: sendto failed: " << lastSocketError() << "\n";
    }
  };

  if (parameters_.canUseBroadcast()) sendTo(INADDR_BROADCAST);
  if (parameters_.canUseMulticast()) sendTo(parameters_.multicastGroupAddress());
}

void PeerEnv::evictExpiredPeers(int64_t nowMs) {
  std::lock_guard<std::mutex> lock(mutex_);
  discoveredPeers_.remove_if([&](const DiscoveredPeer& peer) {
    return nowMs - peer.lastUpdated() > parameters_.discoveredPeerTtlMs();
  });
}

void PeerEnv::handleReceivedPacket(int64_t nowMs, const IpPort& from, const char* packetData, size_t length) {
  Packet packet;
  if (!packet.parse(std::string(packetData, length))) return;

  const bool isRelevant =
      (parameters_.applicationId() == packet.applicationId()) && (parameters_.discoverSelf() || packet.peerId() != peerId_);
  if (!isRelevant) return;

  std::lock_guard<std::mutex> lock(mutex_);

  auto peerIt = std::find_if(discoveredPeers_.begin(), discoveredPeers_.end(), [&](const DiscoveredPeer& peer) {
    return isSame(parameters_.samePeerMode(), peer.ipPort(), from);
  });

  switch (packet.packetType()) {
    case kPacketIAmHere:
      if (peerIt == discoveredPeers_.end()) {
        DiscoveredPeer& peer = discoveredPeers_.emplace_back();
        peer.setIpPort(from);
        peer.initUserData(packet.userData(), packet.snapshotIndex());
        peer.setLastUpdated(nowMs);
      } else {
        peerIt->tryUpdateUserData(packet.userData(), packet.snapshotIndex());
        peerIt->setLastUpdated(nowMs);
      }
      break;

    case kPacketIAmOutOfHere:
      if (peerIt != discoveredPeers_.end()) {
        discoveredPeers_.erase(peerIt);
      }
      break;

    default:
      break;
  }
}

}  // namespace impl

// ============================================================================
// Peer public API
// ============================================================================

Peer::Peer() = default;

/// @brief Joins background threads before the wrapper is destroyed.
Peer::~Peer() { stopImpl(true); }

bool Peer::start(const PeerParameters& parameters, const std::string& userData) {
  stopImpl(true);

  auto env = std::make_shared<impl::PeerEnv>();
  if (!env->start(parameters, userData)) {
    return false;
  }
  env_ = env;

  // Threads capture env by value so they keep it alive beyond Peer's lifetime.
  sending_thread_ = std::make_unique<std::thread>([env] { env->sendingThreadFunc(); });
  if (parameters.canDiscover()) {
    receiving_thread_ = std::make_unique<std::thread>([env] { env->receivingThreadFunc(); });
  }

  return true;
}

void Peer::setUserData(const std::string& userData) {
  if (env_) env_->setUserData(userData);
}

std::list<DiscoveredPeer> Peer::listDiscovered() const {
  if (env_) return env_->listDiscovered();
  return {};
}

void Peer::stop() { stopImpl(false); }
void Peer::stopAndWait() { stopImpl(true); }

void Peer::stopImpl(bool waitForThreads) {
  if (!env_) return;

  env_->requestExit();
  env_.reset();

  const auto joinOrDetach = [waitForThreads](std::unique_ptr<std::thread>& thread) {
    if (thread && thread->joinable()) {
      if (waitForThreads)
        thread->join();
      else
        thread->detach();
    }
    thread.reset();
  };

  joinOrDetach(sending_thread_);
  joinOrDetach(receiving_thread_);
}

// ---- Free functions --------------------------------------------------------

bool isSame(PeerParameters::SamePeerMode mode, const IpPort& lhs, const IpPort& rhs) {
  switch (mode) {
    case PeerParameters::SamePeerMode::kIp:
      return lhs.ip() == rhs.ip();
    case PeerParameters::SamePeerMode::kIpAndPort:
      return lhs.ip() == rhs.ip() && lhs.port() == rhs.port();
  }
  return false;
}

bool isSame(PeerParameters::SamePeerMode mode, const std::list<DiscoveredPeer>& lhs,
            const std::list<DiscoveredPeer>& rhs) {
  const auto containsPeer = [&](const DiscoveredPeer& peer, const std::list<DiscoveredPeer>& peers) {
    return std::any_of(peers.begin(), peers.end(), [&](const DiscoveredPeer& candidatePeer) {
      return isSame(mode, peer.ipPort(), candidatePeer.ipPort());
    });
  };

  for (const auto& peer : lhs)
    if (!containsPeer(peer, rhs)) return false;
  for (const auto& peer : rhs)
    if (!containsPeer(peer, lhs)) return false;
  return true;
}

}  // namespace discovery
