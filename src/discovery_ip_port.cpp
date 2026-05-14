#include "discovery/discovery_ip_port.h"

#include <sstream>

namespace discovery {

std::string ipToString(uint32_t ip) {
  std::ostringstream ss;
  ss << ((ip >> 24) & 0xff) << "." << ((ip >> 16) & 0xff) << "." << ((ip >> 8) & 0xff) << "." << (ip & 0xff);
  return ss.str();
}

std::string ipPortToString(const IpPort& ip_port) {
  std::ostringstream ss;
  ss << ipToString(ip_port.ip()) << ":" << ip_port.port();
  return ss.str();
}

}  // namespace discovery
