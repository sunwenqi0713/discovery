#include "discovery/discovery_peer.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <thread>

#include "discovery/discovery_protocol.h"

// Platform socket API includes and type aliases.
#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketType = SOCKET;
using AddressLenType = int;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using SocketType = int;
using AddressLenType = socklen_t;
constexpr SocketType kInvalidSocket = -1;
#endif

namespace {

void InitSockets() {
#if defined(_WIN32)
  static std::once_flag init_flag;
  std::call_once(init_flag, []() {
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
  });
#endif
}

void SetSocketTimeout(SocketType sock, int param, int timeout_ms) {
#if defined(_WIN32)
  setsockopt(sock, SOL_SOCKET, param, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
  timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = 1000 * (timeout_ms % 1000);
  setsockopt(sock, SOL_SOCKET, param, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#endif
}

void CloseSocket(SocketType sock) {
#if defined(_WIN32)
  closesocket(sock);
#else
  close(sock);
#endif
}

bool IsRightTime(int64_t last_action_time, int64_t now_time, int64_t timeout, int64_t& time_to_wait_out) {
  if (last_action_time == 0) {
    time_to_wait_out = timeout;
    return true;
  }

  int64_t time_passed = now_time - last_action_time;
  if (time_passed >= timeout) {
    time_to_wait_out = timeout - (time_passed - timeout);
    return true;
  }

  time_to_wait_out = timeout - time_passed;
  return false;
}

uint32_t MakeRandomId() {
  static std::mutex gen_mutex;
  static std::mt19937 gen(std::random_device{}());
  static std::uniform_int_distribution<uint32_t> dis;
  std::lock_guard<std::mutex> lock(gen_mutex);
  return dis(gen);
}

}  // namespace

namespace discovery {
namespace impl {

int64_t NowTime() {
  using namespace std::chrono;
  auto now = steady_clock::now();
  return duration_cast<milliseconds>(now.time_since_epoch()).count();
}

void SleepFor(std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); }

class PeerEnv : public PeerEnvInterface, public std::enable_shared_from_this<PeerEnv> {
 public:
  PeerEnv() = default;

  ~PeerEnv() override {
    if (binding_sock_ != kInvalidSocket) {
      CloseSocket(binding_sock_);
    }
    if (sock_ != kInvalidSocket) {
      CloseSocket(sock_);
    }
  }

  PeerEnv(const PeerEnv&) = delete;             // Non-copyable.
  PeerEnv& operator=(const PeerEnv&) = delete;  // Non-copyable.

  bool Start(const PeerParameters& parameters, const std::string& user_data) {
    parameters_ = parameters;
    user_data_ = user_data;

    if (!parameters_.can_use_broadcast() && !parameters_.can_use_multicast()) {
      std::cerr << "discovery::Peer can't use broadcast and can't use multicast." << std::endl;
      return false;
    }

    if (!parameters_.can_discover() && !parameters_.can_be_discovered()) {
      std::cerr << "discovery::Peer can't discover and can't be discovered." << std::endl;
      return false;
    }

    InitSockets();

    peer_id_ = MakeRandomId();

    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == kInvalidSocket) {
      std::cerr << "discovery::Peer can't create socket." << std::endl;
      return false;
    }

    {
      int value = 1;
      if (setsockopt(sock_, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&value), sizeof(value)) < 0) {
        std::cerr << "discovery::Peer failed to enable broadcast on socket." << std::endl;
      }
    }

    if (parameters_.can_discover()) {
      binding_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
      if (binding_sock_ == kInvalidSocket) {
        std::cerr << "discovery::Peer can't create binding socket." << std::endl;

        CloseSocket(sock_);
        sock_ = kInvalidSocket;
        return false;
      }

      {
        int reuse_addr = 1;
        if (setsockopt(binding_sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse_addr),
                       sizeof(reuse_addr)) < 0) {
          std::cerr << "discovery::Peer failed to set SO_REUSEADDR." << std::endl;
        }
#ifdef SO_REUSEPORT
        int reuse_port = 1;
        if (setsockopt(binding_sock_, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&reuse_port),
                       sizeof(reuse_port)) < 0) {
          std::cerr << "discovery::Peer failed to set SO_REUSEPORT." << std::endl;
        }
#endif
      }

      if (parameters_.can_use_multicast()) {
        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = htonl(parameters_.multicast_group_address());
        mreq.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(binding_sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq),
                       sizeof(mreq)) < 0) {
          CloseSocket(binding_sock_);
          binding_sock_ = kInvalidSocket;
          CloseSocket(sock_);
          sock_ = kInvalidSocket;
          std::cerr << "discovery::Peer failed to join multicast group." << std::endl;
          return false;
        }
      }

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(parameters_.port());
      addr.sin_addr.s_addr = htonl(INADDR_ANY);

      if (bind(binding_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in)) < 0) {
        CloseSocket(binding_sock_);
        binding_sock_ = kInvalidSocket;

        CloseSocket(sock_);
        sock_ = kInvalidSocket;

        std::cerr << "discovery::Peer can't bind socket." << std::endl;
        return false;
      }

      // TODO(sunwenqi): Replace timeout-based unblocking with a pipe/eventfd
      // so that shutdown latency is not bounded by the 1-second timeout.
      SetSocketTimeout(binding_sock_, SO_RCVTIMEO, 1000);
    }

    return true;
  }

  void SetUserData(const std::string& user_data) override {
    std::lock_guard<std::mutex> lock(mutex_);
    user_data_ = user_data;
  }

  std::list<DiscoveredPeer> ListDiscovered() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return discovered_peers_;
  }

  void Exit() override {
    std::lock_guard<std::mutex> lock(mutex_);
    exit_ = true;
  }

  void SendingThreadFunc() {
    int64_t last_send_time_ms = 0;
    int64_t last_delete_idle_ms = 0;

    while (true) {
      bool should_exit = false;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        should_exit = exit_;
      }

      if (should_exit) {
        sendPacket(kPacketIAmOutOfHere);
        return;
      }

      int64_t cur_time_ms = NowTime();
      int64_t to_sleep_ms = std::numeric_limits<int64_t>::max();

      if (parameters_.can_be_discovered()) {
        int64_t next_send_wait = 0;
        if (IsRightTime(last_send_time_ms, cur_time_ms, parameters_.send_timeout_ms(), next_send_wait)) {
          sendPacket(kPacketIAmHere);
          last_send_time_ms = cur_time_ms;
        }
        to_sleep_ms = next_send_wait;
      }

      if (parameters_.can_discover()) {
        int64_t next_idle_wait = 0;
        if (IsRightTime(last_delete_idle_ms, cur_time_ms, parameters_.discovered_peer_ttl_ms(), next_idle_wait)) {
          deleteIdle(cur_time_ms);
          last_delete_idle_ms = cur_time_ms;
        }
        to_sleep_ms = std::min(to_sleep_ms, next_idle_wait);
      }

      if (to_sleep_ms > 0 && to_sleep_ms != std::numeric_limits<int64_t>::max()) {
        SleepFor(std::chrono::milliseconds(to_sleep_ms));
      }
    }
  }

  void ReceivingThreadFunc() {
    std::string buffer(kMaxPacketSize, '\0');

    while (true) {
      sockaddr_in from_addr{};
      AddressLenType addr_length = sizeof(sockaddr_in);

      auto length = recvfrom(binding_sock_, &buffer[0], static_cast<int>(kMaxPacketSize), 0,
                             reinterpret_cast<sockaddr*>(&from_addr), &addr_length);

      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (exit_) {
          return;
        }
      }

      if (length <= 0) {
        continue;
      }

      IpPort from;
      from.set_port(ntohs(from_addr.sin_port));
      from.set_ip(ntohl(from_addr.sin_addr.s_addr));

      std::string received(buffer.data(), static_cast<size_t>(length));
      processReceivedBuffer(NowTime(), from, received);
    }
  }

 private:
  void processReceivedBuffer(int64_t cur_time_ms, const IpPort& from, const std::string& buffer) {
    Packet packet;
    if (!packet.Parse(buffer)) {
      return;
    }

    bool accept_packet = (parameters_.application_id() == packet.application_id()) &&
                         (parameters_.discover_self() || packet.peer_id() != peer_id_);

    if (accept_packet) {
      std::lock_guard<std::mutex> lock(mutex_);

      auto find_it =
          std::find_if(discovered_peers_.begin(), discovered_peers_.end(), [this, &from](const DiscoveredPeer& peer) {
            return Same(parameters_.same_peer_mode(), peer.ip_port(), from);
          });

      if (packet.packet_type() == kPacketIAmHere) {
        if (find_it == discovered_peers_.end()) {
          discovered_peers_.emplace_back();
          discovered_peers_.back().set_ip_port(from);
          discovered_peers_.back().SetUserData(packet.user_data(), packet.snapshot_index());
          discovered_peers_.back().set_last_updated(cur_time_ms);
        } else {
          if (find_it->last_received_packet() < packet.snapshot_index()) {
            find_it->SetUserData(packet.user_data(), packet.snapshot_index());
          }
          find_it->set_last_updated(cur_time_ms);
        }
      } else if (packet.packet_type() == kPacketIAmOutOfHere) {
        if (find_it != discovered_peers_.end()) {
          discovered_peers_.erase(find_it);
        }
      }
    }
  }

  void deleteIdle(int64_t cur_time_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    discovered_peers_.remove_if([this, cur_time_ms](const DiscoveredPeer& peer) {
      return cur_time_ms - peer.last_updated() > parameters_.discovered_peer_ttl_ms();
    });
  }

  void sendPacket(PacketType packet_type) {
    std::string user_data;
    uint64_t packet_idx;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      user_data = user_data_;
      packet_idx = packet_index_++;
    }

    Packet packet;
    packet.set_packet_type(packet_type);
    packet.set_application_id(parameters_.application_id());
    packet.set_peer_id(peer_id_);
    packet.set_snapshot_index(packet_idx);
    packet.SwapUserData(user_data);

    std::string packet_data;
    if (!packet.Serialize(packet_data)) {
      return;
    }

    if (parameters_.can_use_broadcast()) {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(parameters_.port());
      addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
      if (sendto(sock_, packet_data.data(), static_cast<int>(packet_data.size()), 0,
                 reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in)) < 0) {
        std::cerr << "discovery::Peer failed to send broadcast packet." << std::endl;
      }
    }

    if (parameters_.can_use_multicast()) {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(parameters_.port());
      addr.sin_addr.s_addr = htonl(parameters_.multicast_group_address());
      if (sendto(sock_, packet_data.data(), static_cast<int>(packet_data.size()), 0,
                 reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in)) < 0) {
        std::cerr << "discovery::Peer failed to send multicast packet." << std::endl;
      }
    }
  }

  PeerParameters parameters_;
  uint32_t peer_id_ = 0;
  SocketType binding_sock_ = kInvalidSocket;
  SocketType sock_ = kInvalidSocket;
  uint64_t packet_index_ = 0;

  mutable std::mutex mutex_;
  bool exit_ = false;
  std::string user_data_;
  std::list<DiscoveredPeer> discovered_peers_;
};

}  // namespace impl

Peer::Peer() = default;

Peer::~Peer() { StopImpl(false); }

bool Peer::Start(const PeerParameters& parameters, const std::string& user_data) {
  StopImpl(false);

  auto env = std::make_shared<impl::PeerEnv>();
  if (!env->Start(parameters, user_data)) {
    return false;
  }

  env_ = env;

  // Capture env by value so the threads keep it alive beyond Peer's lifetime.
  sending_thread_ = std::make_unique<std::thread>([env]() { env->SendingThreadFunc(); });

  if (parameters.can_discover()) {
    receiving_thread_ = std::make_unique<std::thread>([env]() { env->ReceivingThreadFunc(); });
  }

  return true;
}

void Peer::SetUserData(const std::string& user_data) {
  if (env_) {
    env_->SetUserData(user_data);
  }
}

std::list<DiscoveredPeer> Peer::ListDiscovered() const {
  if (env_) {
    return env_->ListDiscovered();
  }
  return {};
}

void Peer::Stop() { StopImpl(false); }

void Peer::StopAndWaitForThreads() { StopImpl(true); }

void Peer::Stop(bool wait_for_threads) { StopImpl(wait_for_threads); }

void Peer::StopImpl(bool wait_for_threads) {
  if (!env_) {
    return;
  }

  env_->Exit();
  env_.reset();

  if (sending_thread_ && sending_thread_->joinable()) {
    if (wait_for_threads) {
      sending_thread_->join();
    } else {
      sending_thread_->detach();
    }
  }
  if (receiving_thread_ && receiving_thread_->joinable()) {
    if (wait_for_threads) {
      receiving_thread_->join();
    } else {
      receiving_thread_->detach();
    }
  }

  sending_thread_.reset();
  receiving_thread_.reset();
}

bool Same(PeerParameters::SamePeerMode mode, const IpPort& lhv, const IpPort& rhv) {
  switch (mode) {
    case PeerParameters::SamePeerMode::kIp:
      return lhv.ip() == rhv.ip();

    case PeerParameters::SamePeerMode::kIpAndPort:
      return (lhv.ip() == rhv.ip()) && (lhv.port() == rhv.port());
  }

  return false;
}

bool Same(PeerParameters::SamePeerMode mode, const std::list<DiscoveredPeer>& lhv,
          const std::list<DiscoveredPeer>& rhv) {
  for (const auto& lhv_peer : lhv) {
    auto in_rhv = std::find_if(rhv.begin(), rhv.end(), [mode, &lhv_peer](const DiscoveredPeer& rhv_peer) {
      return Same(mode, lhv_peer.ip_port(), rhv_peer.ip_port());
    });

    if (in_rhv == rhv.end()) {
      return false;
    }
  }

  for (const auto& rhv_peer : rhv) {
    auto in_lhv = std::find_if(lhv.begin(), lhv.end(), [mode, &rhv_peer](const DiscoveredPeer& lhv_peer) {
      return Same(mode, rhv_peer.ip_port(), lhv_peer.ip_port());
    });

    if (in_lhv == lhv.end()) {
      return false;
    }
  }

  return true;
}

}  // namespace discovery
