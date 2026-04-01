#pragma once

#include <cstdint>
#include <string>

namespace discovery {

namespace impl {

// A lightweight cursor over a byte buffer used for binary serialization and
// parsing. Wraps either a mutable string (write mode) or a const byte span
// (read-only mode) to avoid unnecessary copies or const_cast.
class BufferView {
 public:
  // Write mode: appends serialized bytes to buffer.
  explicit BufferView(std::string* buffer) : write_buffer_(buffer), read_data_(nullptr), read_size_(0) {}

  // Read-only mode: reads from an existing byte range without copying.
  BufferView(const char* data, size_t size) : write_buffer_(nullptr), read_data_(data), read_size_(size) {}

  void push_back(char c) { write_buffer_->push_back(c); }

  void InsertBack(const std::string& s, size_t size) {
    write_buffer_->insert(write_buffer_->end(), s.begin(), s.begin() + size);
  }

  size_t parsed() const { return parsed_; }

  size_t LeftUnparsed() const {
    return (write_buffer_ ? write_buffer_->size() : read_size_) - parsed_;
  }

  bool CanRead(size_t num_bytes) const {
    return parsed_ + num_bytes <= (write_buffer_ ? write_buffer_->size() : read_size_);
  }

  char Read() {
    char c = write_buffer_ ? (*write_buffer_)[parsed_] : read_data_[parsed_];
    ++parsed_;
    return c;
  }

 private:
  std::string* write_buffer_ = nullptr;
  const char* read_data_ = nullptr;
  size_t read_size_ = 0;
  size_t parsed_ = 0;
};

enum class SerializeDirection {
  kSerialize,
  kParse,
};

constexpr SerializeDirection kSerialize = SerializeDirection::kSerialize;
constexpr SerializeDirection kParse = SerializeDirection::kParse;

// Serializes or parses an unsigned integer in big-endian byte order.
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

}  // namespace impl

// Maximum number of bytes allowed in the user_data payload.
constexpr size_t kMaxUserDataSize = 4096;

// Maximum UDP datagram size used for the receive buffer.
constexpr size_t kMaxPacketSize = 65536;

enum class PacketType : uint8_t { kIAmHere = 0, kIAmOutOfHere = 1, kUnknown = 255 };

constexpr PacketType kPacketIAmHere = PacketType::kIAmHere;
constexpr PacketType kPacketIAmOutOfHere = PacketType::kIAmOutOfHere;
constexpr PacketType kPacketTypeUnknown = PacketType::kUnknown;

namespace impl {
PacketType GetPacketType(uint8_t packet_type);
}  // namespace impl

// Represents a single discovery protocol packet.
//
// Supports binary serialization (Serialize) and deserialization (Parse).
// The wire format uses magic bytes "SO7V" followed by a version byte and
// fixed-size header fields, with a variable-length user data payload.
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

  // Serializes this packet into buffer_out. Returns false if user_data
  // exceeds kMaxUserDataSize or another serialization error occurs.
  bool Serialize(std::string& buffer_out);

  // Parses buffer into this packet. Returns false if the buffer does not
  // contain a valid packet (wrong magic, unknown version, truncated data).
  bool Parse(const std::string& buffer);

 private:
  bool SerializeBody(impl::SerializeDirection direction, impl::BufferView* buffer_view);

  uint8_t packet_type_ = 0;
  uint32_t application_id_ = 0;
  uint32_t peer_id_ = 0;
  uint64_t snapshot_index_ = 0;
  std::string user_data_;
};

}  // namespace discovery
