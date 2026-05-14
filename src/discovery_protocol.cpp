#include "discovery/discovery_protocol.h"

namespace discovery {

bool Packet::serialize(std::string& buffer_out) {
  if (user_data_.size() > kMaxUserDataSize) return false;

  impl::Writer writer(buffer_out);
  for (char magic_byte : impl::kMagic) writer.writeByte(static_cast<uint8_t>(magic_byte));
  writer.writeUInt(impl::kVersion);
  writeBody(writer);
  return true;
}

void Packet::writeBody(impl::Writer& writer) {
  writer.writeUInt<uint8_t>(0);  // reserved[0]
  writer.writeUInt<uint8_t>(0);  // reserved[1]
  writer.writeUInt<uint8_t>(0);  // reserved[2]

  writer.writeUInt(packet_type_);
  writer.writeUInt(application_id_);
  writer.writeUInt(peer_id_);
  writer.writeUInt(snapshot_index_);

  const auto user_data_size = static_cast<uint16_t>(user_data_.size());
  writer.writeUInt(user_data_size);
  writer.writeBytes(user_data_, user_data_size);
}

bool Packet::parse(const std::string& buffer) {
  impl::Reader reader(buffer.data(), buffer.size());

  for (char expected : impl::kMagic) {
    uint8_t actual = 0;
    if (!reader.readUInt(actual) || actual != static_cast<uint8_t>(expected)) return false;
  }

  uint8_t version = 0;
  if (!reader.readUInt(version) || version != impl::kVersion) return false;

  return parseBody(reader);
}

bool Packet::parseBody(impl::Reader& reader) {
  uint8_t reserved = 0;
  for (int i = 0; i < 3; ++i) {
    if (!reader.readUInt(reserved)) return false;
  }

  if (!reader.readUInt(packet_type_)) return false;
  if (packet_type_ != static_cast<uint8_t>(kPacketIAmHere) &&
      packet_type_ != static_cast<uint8_t>(kPacketIAmOutOfHere)) {
    return false;
  }

  if (!reader.readUInt(application_id_)) return false;
  if (!reader.readUInt(peer_id_)) return false;
  if (!reader.readUInt(snapshot_index_)) return false;

  uint16_t user_data_size = 0;
  if (!reader.readUInt(user_data_size)) return false;
  if (user_data_size > kMaxUserDataSize) return false;
  if (reader.remaining() != user_data_size) return false;

  return reader.readBytes(user_data_, user_data_size);
}

}  // namespace discovery
