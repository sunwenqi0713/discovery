#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "discovery/discovery_peer.h"

namespace {

volatile std::sig_atomic_t g_running = 1;

/// @brief Stops the sample loop when a termination signal is received.
void onSignal(int) { g_running = 0; }

/// @brief Prints command-line usage for the sample program.
/// @param argv Process argument vector.
void printUsage(char* argv[]) {
  std::cout << "Usage: " << argv[0] << " application_id port\n"
            << "  application_id - integer id of application to discover\n"
            << "  port           - port used by application\n";
}

/// @brief Prints the currently discovered peers.
/// @param peers Snapshot of discovered peers.
void printPeers(const std::list<discovery::DiscoveredPeer>& peers) {
  std::cout << "Discovered peers: " << peers.size() << "\n";
  for (const auto& p : peers) {
    std::cout << " - " << discovery::ipToString(p.ipPort().ip()) << ", " << p.userData() << "\n";
  }
}

/// @brief Reports whether any discovered peer payload has changed.
/// @param peers Current peer snapshot.
/// @param known_user_data Previously observed peer payloads.
/// @return `true` if at least one payload differs.
bool hasUserDataChanged(const std::list<discovery::DiscoveredPeer>& peers,
                        const std::map<discovery::IpPort, std::string>& known_user_data) {
  for (const auto& peer : peers) {
    const auto it = known_user_data.find(peer.ipPort());
    if (it == known_user_data.end() || it->second != peer.userData()) return true;
  }
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    std::cerr << "expecting application_id and port\n";
    printUsage(argv);
    return 1;
  }
  if (argc <= 2) {
    std::cerr << "expecting port\n";
    printUsage(argv);
    return 1;
  }

  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);

  discovery::PeerParameters parameters;
  parameters.setCanDiscover(true);
  parameters.setCanBeDiscovered(false);
  parameters.setPort(static_cast<uint16_t>(std::atoi(argv[2])));
  parameters.setApplicationId(static_cast<uint32_t>(std::atoi(argv[1])));

  discovery::Peer peer;
  if (!peer.start(parameters, "")) return 1;

  std::list<discovery::DiscoveredPeer> known_peers;
  std::map<discovery::IpPort, std::string> known_user_data;

  while (g_running) {
    const auto current_peers = peer.listDiscovered();

    const bool changed = !discovery::isSame(parameters.samePeerMode(), known_peers, current_peers) ||
                         hasUserDataChanged(current_peers, known_user_data);

    if (changed) {
      known_peers = current_peers;
      known_user_data.clear();
      for (const auto& discovered_peer : known_peers) {
        known_user_data.emplace(discovered_peer.ipPort(), discovered_peer.userData());
      }
      printPeers(known_peers);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  peer.stopAndWait();
  return 0;
}
