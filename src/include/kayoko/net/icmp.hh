#ifndef KAYOKO_NET_ICMP_HH
#define KAYOKO_NET_ICMP_HH

#include <cstdint>

namespace kayoko {
namespace net {

/**
 * ICMP Header Structure (RFC 792)
 * Note: We use uint8_t and uint16_t to ensure exact sizing.
 */
struct icmp_hdr {
    uint8_t  type;      // ICMP message type
    uint8_t  code;      // ICMP message sub-code
    uint16_t checksum;  // 16-bit ones' complement checksum
    uint16_t id;        // Identifier (usually PID)
    uint16_t seq;       // Sequence number
};

// Common ICMP Types
inline constexpr uint8_t echo_reply   = 0;
inline constexpr uint8_t echo_request = 8;

} // namespace net
} // namespace kayoko

#endif // KAYOKO_NET_ICMP_HH
