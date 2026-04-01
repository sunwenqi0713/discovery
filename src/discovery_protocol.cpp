#include "discovery/discovery_protocol.h"

namespace discovery {
namespace impl {

bool SerializeString(SerializeDirection direction, std::string* value, size_t value_size, BufferView* buffer_view) {
  if (direction == SerializeDirection::kSerialize) {
    buffer_view->InsertBack(*value, value_size);
  } else {
    if (!buffer_view->CanRead(value_size)) {
      return false;
    }
    value->resize(value_size);
    for (size_t i = 0; i < value_size; ++i) {
      (*value)[i] = buffer_view->Read();
    }
  }
  return true;
}

PacketType GetPacketType(uint8_t packet_type) {
  if (packet_type == static_cast<uint8_t>(kPacketIAmHere)) {
    return kPacketIAmHere;
  } else if (packet_type == static_cast<uint8_t>(kPacketIAmOutOfHere)) {
    return kPacketIAmOutOfHere;
  }
  return kPacketTypeUnknown;
}

}  // namespace impl

bool Packet::Serialize(std::string& buffer_out) {
  if (user_data_.size() > kMaxUserDataSize) {
    return false;
  }
  impl::BufferView buffer_view(&buffer_out);
  return SerializeBody(impl::kSerialize, &buffer_view);
}

bool Packet::Parse(const std::string& buffer) {
  impl::BufferView buffer_view(buffer.data(), buffer.size());

  // Verify the 4-byte magic signature that identifies this protocol.
  const char kMagic[] = {'D', 'S', 'C', 'V'};
  for (char expected : kMagic) {
    uint8_t byte = 0;
    if (!impl::SerializeUnsignedIntegerBigEndian(impl::kParse, &byte, &buffer_view)) {
      return false;
    }
    if (byte != static_cast<uint8_t>(expected)) {
      return false;
    }
  }

  // Reject packets from unknown or future protocol versions.
  constexpr uint8_t kCurrentVersion = 1;
  uint8_t version = 0;
  if (!impl::SerializeUnsignedIntegerBigEndian(impl::kParse, &version, &buffer_view)) {
    return false;
  }
  if (version != kCurrentVersion) {
    return false;
  }

  return SerializeBody(impl::kParse, &buffer_view);
}

bool Packet::SerializeBody(impl::SerializeDirection direction, impl::BufferView* buffer_view) {
  if (direction == impl::kSerialize) {
    buffer_view->push_back('D');
    buffer_view->push_back('S');
    buffer_view->push_back('C');
    buffer_view->push_back('V');
    uint8_t version = 1;
    impl::SerializeUnsignedIntegerBigEndian(impl::kSerialize, &version, buffer_view);
  }

  // Three reserved bytes follow the version field (reserved for future use).
  uint8_t reserved = 0;
  for (int i = 0; i < 3; ++i) {
    if (!impl::SerializeUnsignedIntegerBigEndian(direction, &reserved, buffer_view)) {
      return false;
    }
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &packet_type_, buffer_view)) {
    return false;
  }
  if (direction == impl::kParse) {
    if (impl::GetPacketType(packet_type_) == kPacketTypeUnknown) {
      return false;
    }
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &application_id_, buffer_view)) {
    return false;
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &peer_id_, buffer_view)) {
    return false;
  }

  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &snapshot_index_, buffer_view)) {
    return false;
  }

  auto user_data_size = static_cast<uint16_t>(user_data_.size());
  if (!impl::SerializeUnsignedIntegerBigEndian(direction, &user_data_size, buffer_view)) {
    return false;
  }

  if (direction == impl::kParse) {
    if (user_data_size > kMaxUserDataSize) {
      return false;
    }
    // Ensure the remaining bytes match the declared payload length exactly.
    if (buffer_view->LeftUnparsed() != user_data_size) {
      return false;
    }
  }

  if (!impl::SerializeString(direction, &user_data_, user_data_size, buffer_view)) {
    return false;
  }

  return true;
}

}  // namespace discovery
