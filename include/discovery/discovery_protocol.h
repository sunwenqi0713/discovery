#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace discovery {

/// @brief Maximum number of bytes allowed in the user data payload.
constexpr size_t kMaxUserDataSize = 4096;

/// @brief Maximum UDP datagram size used for the receive buffer.
constexpr size_t kMaxPacketSize = 65536;

/// @brief Packet types used by the discovery protocol.
enum class PacketType : uint8_t {
  kIAmHere = 0,       ///< Periodic presence announcement.
  kIAmOutOfHere = 1,  ///< Departure announcement.
  kUnknown = 255,     ///< Invalid or unrecognized packet type.
};

constexpr PacketType kPacketIAmHere = PacketType::kIAmHere;
constexpr PacketType kPacketIAmOutOfHere = PacketType::kIAmOutOfHere;
constexpr PacketType kPacketTypeUnknown = PacketType::kUnknown;

namespace impl {

/// @brief Wire-format constants shared between `serialize()` and `parse()`.
constexpr char kMagic[4] = {'D', 'S', 'C', 'V'};
constexpr uint8_t kVersion = 1;

/// @brief Append-only cursor used during binary serialization.
class Writer {
 public:
  explicit Writer(std::string& buffer) : buffer_(buffer) {}

  /// @brief Appends a single byte.
  /// @param byte Byte to append.
  void writeByte(uint8_t byte) { buffer_.push_back(static_cast<char>(byte)); }

  /// @brief Appends an unsigned integer in big-endian byte order.
  /// @tparam T Unsigned integer type.
  /// @param value Value to append.
  template <typename T>
  void writeUInt(T value) {
    constexpr size_t byte_count = sizeof(T);
    for (size_t i = 0; i < byte_count; ++i) {
      writeByte(static_cast<uint8_t>(value >> ((byte_count - i - 1) * 8)));
    }
  }

  /// @brief Appends a fixed number of bytes from a string.
  /// @param bytes Source string.
  /// @param count Number of bytes to append.
  void writeBytes(const std::string& bytes, size_t count) {
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(count));
  }

 private:
  std::string& buffer_;
};

/// @brief Read-only cursor used during binary deserialization.
class Reader {
 public:
  Reader(const char* data, size_t size) : data_(data), size_(size) {}

  /// @brief Reports whether the cursor can read the requested byte count.
  /// @param byte_count Number of bytes to test.
  /// @return `true` if `byte_count` bytes are available.
  bool canRead(size_t byte_count) const { return pos_ + byte_count <= size_; }

  /// @brief Returns the remaining unread byte count.
  size_t remaining() const { return size_ - pos_; }

  /// @brief Reads an unsigned integer in big-endian byte order.
  /// @tparam T Unsigned integer type.
  /// @param value Output value.
  /// @return `true` on success, otherwise `false`.
  template <typename T>
  bool readUInt(T& value) {
    constexpr size_t byte_count = sizeof(T);
    if (!canRead(byte_count)) return false;
    value = 0;
    for (size_t i = 0; i < byte_count; ++i) {
      value |= static_cast<T>(static_cast<uint8_t>(data_[pos_++])) << ((byte_count - i - 1) * 8);
    }
    return true;
  }

  /// @brief Reads a fixed number of bytes into a string.
  /// @param out Output string.
  /// @param count Number of bytes to read.
  /// @return `true` on success, otherwise `false`.
  bool readBytes(std::string& out, size_t count) {
    if (!canRead(count)) return false;
    out.assign(data_ + pos_, count);
    pos_ += count;
    return true;
  }

 private:
  const char* data_;
  size_t size_;
  size_t pos_ = 0;
};

}  // namespace impl

/// @brief Represents a single discovery protocol packet.
/// @details Wire format: 4-byte magic `"DSCV"`, 1-byte version, then the body
/// fields. Supports binary serialization and deserialization.
class Packet {
 public:
  Packet() = default;

  /// @brief Returns the packet type.
  PacketType packetType() const { return static_cast<PacketType>(packet_type_); }
  /// @brief Sets the packet type.
  /// @param packet_type Packet type value.
  void setPacketType(PacketType packet_type) { packet_type_ = static_cast<uint8_t>(packet_type); }

  /// @brief Returns the application identifier.
  uint32_t applicationId() const { return application_id_; }
  /// @brief Sets the application identifier.
  /// @param application_id Application identifier.
  void setApplicationId(uint32_t application_id) { application_id_ = application_id; }

  /// @brief Returns the peer identifier.
  uint32_t peerId() const { return peer_id_; }
  /// @brief Sets the peer identifier.
  /// @param peer_id Peer identifier.
  void setPeerId(uint32_t peer_id) { peer_id_ = peer_id; }

  /// @brief Returns the packet snapshot index.
  uint64_t snapshotIndex() const { return snapshot_index_; }
  /// @brief Sets the packet snapshot index.
  /// @param snapshot_index Monotonic packet sequence value.
  void setSnapshotIndex(uint64_t snapshot_index) { snapshot_index_ = snapshot_index; }

  /// @brief Returns the user-defined payload.
  const std::string& userData() const { return user_data_; }
  /// @brief Sets the user-defined payload.
  /// @param user_data User-defined payload.
  void setUserData(const std::string& user_data) { user_data_ = user_data; }
  /// @brief Swaps the user-defined payload with an external string.
  /// @param new_user_data String to exchange with the internal payload.
  void swapUserData(std::string& new_user_data) { std::swap(user_data_, new_user_data); }

  /// @brief Serializes the packet into a binary buffer.
  /// @param buffer_out Output buffer.
  /// @return `true` on success, otherwise `false`.
  bool serialize(std::string& buffer_out);

  /// @brief Parses a binary buffer into this packet.
  /// @param buffer Input buffer.
  /// @return `true` on success, otherwise `false`.
  bool parse(const std::string& buffer);

 private:
  void writeBody(impl::Writer& writer);
  bool parseBody(impl::Reader& reader);

  uint8_t packet_type_ = 0;
  uint32_t application_id_ = 0;
  uint32_t peer_id_ = 0;
  uint64_t snapshot_index_ = 0;
  std::string user_data_;
};

}  // namespace discovery
