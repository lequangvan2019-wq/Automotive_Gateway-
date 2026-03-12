#pragma once
#include "dbc_parser.hpp"
#include "../../include/gateway/types.hpp"
#include <linux/can.h>
#include <vector>

class SignalDecoder {
public:
    // Decode all signals in a CAN frame using the message definition
    // Returns one DecodedSignal per signal in the message
    std::vector<DecodedSignal> decode(const can_frame& frame,
                                      const MessageDef& msg) const;

private:
    // Extract raw integer bits from CAN data bytes
    // Handles little-endian (Intel) byte order
    // Big-endian (Motorola) support added in Phase 6
    uint64_t extract_bits(const uint8_t* data,
                           uint8_t start_bit,
                           uint8_t length) const;
};
