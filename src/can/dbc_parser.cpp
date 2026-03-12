#include "dbc_parser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

bool DbcParser::parse(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[DBC] Cannot open: " << filepath << "\n";
        return false;
    }

    std::string line;
    uint32_t current_id = 0;
    bool in_message = false;

    while (std::getline(file, line)) {
        // Strip carriage return (Windows line endings)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Check for blank line BEFORE trimming
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            in_message = false;
            continue;
        }

        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);

        if (trimmed.size() >= 4 && trimmed.substr(0, 4) == "BO_ ") {
            auto msg = parse_message_line(trimmed);
            if (msg) {
                current_id = msg->can_id;
                messages_[current_id] = std::move(*msg);
                in_message = true;
                std::cout << "[DBC] Found message ID=" << current_id
                          << " name=" << messages_[current_id].name << "\n";
            }
        } else if (trimmed.size() >= 4 && trimmed.substr(0, 4) == "SG_ "
                   && in_message) {
            auto sig = parse_signal_line(trimmed);
            if (sig) {
                std::cout << "[DBC]   Signal: " << sig->name << "\n";
                messages_[current_id].signals.push_back(std::move(*sig));
            }
        }
    }

    std::cout << "[DBC] Parsed " << messages_.size() << " messages\n";
    for (const auto& [id, msg] : messages_) {
        std::cout << "[DBC]   0x" << std::hex << id << std::dec
                  << " " << msg.name
                  << " (" << msg.signals.size() << " signals)\n";
    }
    return true;
}

const MessageDef* DbcParser::find_message(uint32_t can_id) const {
    auto it = messages_.find(can_id);
    if (it == messages_.end()) return nullptr;
    return &it->second;
}

std::optional<MessageDef> DbcParser::parse_message_line(
    const std::string& line)
{
    // Format: BO_ 291 EngineData: 8 Vector__XXX
    std::istringstream ss(line);
    std::string tag, name, sender;
    uint32_t id;
    int length;

    ss >> tag >> id >> name >> length >> sender;
    if (ss.fail() || tag != "BO_") return std::nullopt;

    // Remove trailing colon from name
    if (!name.empty() && name.back() == ':')
        name.pop_back();

    MessageDef msg;
    msg.can_id = id;
    msg.name   = name;
    msg.length = static_cast<uint8_t>(length);
    return msg;
}

std::optional<SignalDef> DbcParser::parse_signal_line(
    const std::string& line)
{
    // Format: SG_ EngineRPM : 0|16@1+ (0.25,0) [0|16383.75] "rpm" Vector
    std::istringstream ss(line);
    std::string tag, name, colon, bit_info, scale_info, range_info, unit;

    ss >> tag >> name >> colon >> bit_info >> scale_info >> range_info >> unit;
    if (ss.fail() || tag != "SG_") return std::nullopt;

    // Remove surrounding quotes from unit
    if (unit.size() >= 2 && unit.front() == '"' && unit.back() == '"')
        unit = unit.substr(1, unit.size() - 2);

    // --- Parse bit_info: "0|16@1+" ---
    size_t pipe_pos = bit_info.find('|');
    size_t at_pos   = bit_info.find('@');
    if (pipe_pos == std::string::npos || at_pos == std::string::npos) {
        std::cerr << "[DBC] Bad bit_info: " << bit_info << "\n";
        return std::nullopt;
    }

    SignalDef sig;
    sig.name = name;

    try {
        sig.start_bit = static_cast<uint8_t>(
                            std::stoi(bit_info.substr(0, pipe_pos)));
        sig.length    = static_cast<uint8_t>(
                            std::stoi(bit_info.substr(
                                pipe_pos + 1, at_pos - pipe_pos - 1)));
    } catch (...) {
        std::cerr << "[DBC] Bad bit values in: " << bit_info << "\n";
        return std::nullopt;
    }

    std::string order_sign = bit_info.substr(at_pos + 1);
    sig.is_little_endian = (!order_sign.empty() && order_sign[0] == '1');
    sig.is_signed        = (order_sign.size() > 1 && order_sign[1] == '-');

    // --- Parse scale_info: "(0.25,0)" ---
    std::string sc = scale_info;
    if (!sc.empty() && sc.front() == '(') sc = sc.substr(1);
    if (!sc.empty() && sc.back()  == ')') sc.pop_back();
    size_t comma = sc.find(',');
    if (comma == std::string::npos) {
        std::cerr << "[DBC] Bad scale_info: " << scale_info << "\n";
        return std::nullopt;
    }

    try {
        sig.scale  = std::stod(sc.substr(0, comma));
        sig.offset = std::stod(sc.substr(comma + 1));
    } catch (...) {
        std::cerr << "[DBC] Bad scale values in: " << scale_info << "\n";
        return std::nullopt;
    }

    // --- Parse range_info: "[0|16383.75]" ---
    std::string rng = range_info;
    if (!rng.empty() && rng.front() == '[') rng = rng.substr(1);
    if (!rng.empty() && rng.back()  == ']') rng.pop_back();
    size_t rng_pipe = rng.find('|');
    if (rng_pipe == std::string::npos) {
        std::cerr << "[DBC] Bad range_info: " << range_info << "\n";
        return std::nullopt;
    }

    try {
        sig.min_val = std::stod(rng.substr(0, rng_pipe));
        sig.max_val = std::stod(rng.substr(rng_pipe + 1));
    } catch (...) {
        std::cerr << "[DBC] Bad range values in: " << range_info << "\n";
        return std::nullopt;
    }

    sig.unit = unit;
    return sig;
}
