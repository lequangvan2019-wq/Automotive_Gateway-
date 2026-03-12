#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

// One signal inside a CAN message
// Example: EngineRPM at bit 0, length 16, scale 0.25, offset 0
struct SignalDef {
    std::string name;
    uint8_t     start_bit;   // bit position in the CAN frame
    uint8_t     length;      // number of bits
    bool        is_signed;   // signed or unsigned value
    bool        is_little_endian; // true = Intel, false = Motorola
    double      scale;       // physical = raw * scale + offset
    double      offset;
    double      min_val;
    double      max_val;
    std::string unit;
};

// One CAN message containing multiple signals
struct MessageDef {
    uint32_t             can_id;
    std::string          name;
    uint8_t              length;   // expected DLC in bytes
    std::vector<SignalDef> signals;
};

// The full parsed DBC file
class DbcParser {
public:
    // Parse a DBC file — returns false if file cannot be opened
    bool parse(const std::string& filepath);

    // Look up a message definition by CAN ID
    // Returns nullptr if CAN ID not found
    const MessageDef* find_message(uint32_t can_id) const;

    // How many messages were parsed
    size_t message_count() const { return messages_.size(); }

private:
    // Map from CAN ID to message definition
    std::unordered_map<uint32_t, MessageDef> messages_;

    // Parse one BO_ line (message header)
    std::optional<MessageDef> parse_message_line(const std::string& line);

    // Parse one SG_ line (signal definition)
    std::optional<SignalDef> parse_signal_line(const std::string& line);
};
