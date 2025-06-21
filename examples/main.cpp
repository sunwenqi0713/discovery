#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "discovery/discovery_peer.h"

namespace {

void Usage(int argc, char* argv[]) {
  std::cout << "Usage: " << argv[0] << " application_id port" << std::endl;
  std::cout << "  application_id - integer id of application to discover" << std::endl;
  std::cout << "  port - port used by application" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc <= 1) {
    std::cerr << "expecting application_id and port" << std::endl;
    Usage(argc, argv);
    return 1;
  }

  if (argc <= 2) {
    std::cerr << "expecting port" << std::endl;
    Usage(argc, argv);
    return 1;
  }

  auto port = static_cast<uint16_t>(std::atoi(argv[2]));
  auto application_id = static_cast<uint32_t>(std::atoi(argv[1]));

  discovery::PeerParameters parameters;
  parameters.set_can_discover(true);
  parameters.set_can_be_discovered(false);

  parameters.set_port(port);
  parameters.set_application_id(application_id);

  discovery::Peer peer;

  if (!peer.Start(parameters, "")) {
    return 1;
  }

  std::list<discovery::DiscoveredPeer> discovered_peers;
  std::map<discovery::IpPort, std::string> last_seen_user_datas;

  while (true) {
    auto new_discovered_peers = peer.ListDiscovered();
    if (!discovery::Same(parameters.same_peer_mode(), discovered_peers, new_discovered_peers)) {
      discovered_peers = new_discovered_peers;

      last_seen_user_datas.clear();
      for (const auto& p : discovered_peers) {
        last_seen_user_datas.insert(std::make_pair(p.ip_port(), p.user_data()));
      }

      std::cout << "Discovered peers: " << discovered_peers.size() << std::endl;
      for (const auto& p : discovered_peers) {
        std::cout << " - " << discovery::IpToString(p.ip_port().ip()) << ", " << p.user_data() << std::endl;
      }
    } else {
      bool same_user_datas = true;
      for (const auto& p : new_discovered_peers) {
        auto find_it = last_seen_user_datas.find(p.ip_port());
        if (find_it != last_seen_user_datas.end()) {
          if (find_it->second != p.user_data()) {
            same_user_datas = false;
            break;
          }
        } else {
          same_user_datas = false;
          break;
        }
      }

      if (!same_user_datas) {
        discovered_peers = new_discovered_peers;

        last_seen_user_datas.clear();
        for (const auto& p : discovered_peers) {
          last_seen_user_datas.insert(std::make_pair(p.ip_port(), p.user_data()));
        }

        std::cout << "Discovered peers: " << discovered_peers.size() << std::endl;
        for (const auto& p : discovered_peers) {
          std::cout << " - " << discovery::IpToString(p.ip_port().ip()) << ", " << p.user_data() << std::endl;
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  peer.Stop(true);

  return 0;
}
