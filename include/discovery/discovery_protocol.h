#pragma once

#include <cstdint>
#include <string>

#include "discovery_protocol_version.h"

namespace discovery {

namespace impl {

class BufferView {
 public:
  BufferView() = default;
  explicit BufferView(std::string* buffer) : buffer_(buffer) {}

  std::string* buffer() { return buffer_; }

  void push_back(char c) { buffer_->push_back(c); }

  void InsertBack(const std::string& s, size_t size) { buffer_->insert(buffer_->end(), s.begin(), s.begin() + size); }

  size_t parsed() const { return parsed_; }

  size_t LeftUnparsed() const { return buffer_->size() - parsed_; }

  bool CanRead(size_t num_bytes) const { return (parsed_ + num_bytes <= buffer_->size()); }

  char Read() {
    char c = buffer_->at(parsed_);
    ++parsed_;
    return c;
  }

 private:
  std::string* buffer_ = nullptr;
  size_t parsed_ = 0;
};

enum class SerializeDirection {
  kSerialize,
  kParse,
};

// Convenience aliases for backward compatibility
constexpr SerializeDirection kSerialize = SerializeDirection::kSerialize;
constexpr SerializeDirection kParse = SerializeDirection::kParse;

template <typename ValueType>
bool SerializeUnsignedIntegerBigEndian(SerializeDirection direction, ValueType* value, BufferView* buffer_view) {
  constexpr size_t n = sizeof(ValueType);

  if (direction == SerializeDirection::kSerialize) {
    for (size_t i = 0; i < n; ++i) {
      auto c = static_cast<uint8_t>((*value) >> ((n - i - 1) * 8)) & 0xff;
      buffer_view->push_back(static_cast<char>(c));
    }
  } else {
    *value = 0;
    if (buffer_view->CanRead(n)) {
      for (size_t i = 0; i < n; ++i) {
        auto v = static_cast<ValueType>(static_cast<uint8_t>(buffer_view->Read()));
        *value |= (v << ((n - i - 1) * 8));
      }
    } else {
      return false;
    }
  }

  return true;
}

bool SerializeString(SerializeDirection direction, std::string* value, size_t value_size, BufferView* buffer_view);

ProtocolVersion GetProtocolVersion(uint8_t version);

}  // namespace impl

constexpr size_t kMaxUserDataSizeV0 = 32768;
constexpr size_t kMaxPaddingSizeV0 = 32768;
constexpr size_t kMaxUserDataSizeV1 = 4096;
// Used for receiving buffer.
constexpr size_t kMaxPacketSize = 65536;

enum class PacketType : uint8_t { kIAmHere = 0, kIAmOutOfHere = 1, kUnknown = 255 };

// Convenience aliases for backward compatibility
constexpr PacketType kPacketIAmHere = PacketType::kIAmHere;
constexpr PacketType kPacketIAmOutOfHere = PacketType::kIAmOutOfHere;
constexpr PacketType kPacketTypeUnknown = PacketType::kUnknown;

namespace impl {
PacketType GetPacketType(uint8_t packet_type);
}  // namespace impl

class Packet {
 public:
  Packet() = default;

  PacketType packet_type() const { return static_cast<PacketType>(packet_type_); }
  void set_packet_type(PacketType packet_type) { packet_type_ = static_cast<uint8_t>(packet_type); }

  uint32_t application_id() const { return application_id_; }
  void set_application_id(uint32_t application_id) { application_id_ = application_id; }

  uint32_t peer_id() const { return peer_id_; }
  void set_peer_id(uint32_t peer_id) { peer_id_ = peer_id; }

  uint64_t snapshot_index() const { return snapshot_index_; }
  void set_snapshot_index(uint64_t snapshot_index) { snapshot_index_ = snapshot_index; }

  const std::string& user_data() const { return user_data_; }
  void set_user_data(const std::string& user_data) { user_data_ = user_data; }
  void SwapUserData(std::string& user_data) { std::swap(user_data_, user_data); }

  // Writes the packet to the buffer for sending. Uses provided protocol_version
  // to construct data on wire. This function should return false in the case
  // when it is not possible to convert the current packet representation to
  // the wire representation of the given version. The caller can reserve memory
  // in the buffer_out and no memory will be allocated in this function.
  bool Serialize(ProtocolVersion protocol_version, std::string& buffer_out);

  // Parses the provided buffer and returns the detected protocol version. If
  // parsing fails then kProtocolVersionUnknown is returned.
  ProtocolVersion Parse(const std::string& buffer);

 private:
  bool Serialize(ProtocolVersion protocol_version, impl::SerializeDirection direction, impl::BufferView* buffer_view);

  uint8_t packet_type_ = 0;
  uint32_t application_id_ = 0;
  uint32_t peer_id_ = 0;
  uint64_t snapshot_index_ = 0;
  std::string user_data_;
};

}  // namespace discovery
