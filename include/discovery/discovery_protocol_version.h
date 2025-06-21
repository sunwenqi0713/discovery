#pragma once

#include <cstdint>

namespace discovery {

enum class ProtocolVersion : uint8_t { kVersion0 = 0, kVersion1 = 1, kCurrent = kVersion1, kUnknown = 255 };

// Convenience aliases for backward compatibility
constexpr ProtocolVersion kProtocolVersion0 = ProtocolVersion::kVersion0;
constexpr ProtocolVersion kProtocolVersion1 = ProtocolVersion::kVersion1;
constexpr ProtocolVersion kProtocolVersionCurrent = ProtocolVersion::kCurrent;
constexpr ProtocolVersion kProtocolVersionUnknown = ProtocolVersion::kUnknown;

}  // namespace discovery
