#include "signal_decoder.hpp"
#include <cmath>
#include <iostream>

std::vector<DecodedSignal> SignalDecoder::decode(
    const can_frame& frame, const MessageDef& msg) const
{
    std::vector<DecodedSignal> results;
    results.reserve(msg.signals.size());

    for (const auto& sig : msg.signals) {
        // Step 1 — extract raw bits from CAN data
        uint64_t raw = extract_bits(frame.data, sig.start_bit, sig.length);

        // Step 2 — handle signed values (two's complement)
        int64_t raw_signed = static_cast<int64_t>(raw);
        if (sig.is_signed && sig.length < 64) {
            // Check if sign bit is set
            uint64_t sign_bit = 1ULL << (sig.length - 1);
            if (raw & sign_bit) {
                // Sign-extend to 64 bits
                raw_signed = static_cast<int64_t>(
                    raw | (~0ULL << sig.length));
            }
        }

        // Step 3 — apply scale and offset
        // physical_value = raw * scale + offset
        double physical = static_cast<double>(raw_signed)
                          * sig.scale + sig.offset;

        // Step 4 — clamp to defined range
        physical = std::max(sig.min_val, std::min(sig.max_val, physical));

        results.push_back({sig.name, physical, sig.unit});
    }

    return results;
}

uint64_t SignalDecoder::extract_bits(const uint8_t* data,
                                      uint8_t start_bit,
                                      uint8_t length) const
{
    // Intel byte order (little-endian):
    // start_bit is the LSB position, counting from bit 0 of byte 0
    uint64_t result = 0;

    for (uint8_t i = 0; i < length; i++) {
        uint8_t bit_pos  = start_bit + i;
        uint8_t byte_idx = bit_pos / 8;
        uint8_t bit_idx  = bit_pos % 8;

        if (byte_idx >= 8) break;  // safety: CAN frame is max 8 bytes

        if (data[byte_idx] & (1 << bit_idx))
            result |= (1ULL << i);
    }

    return result;
}
