#pragma once
#include <cstdint>
#include <vector>
#include <string>

// UDS Service IDs
constexpr uint8_t UDS_SESSION_CONTROL    = 0x10;
constexpr uint8_t UDS_READ_DTC           = 0x19;
constexpr uint8_t UDS_READ_DATA_BY_ID    = 0x22;
constexpr uint8_t UDS_ECU_RESET          = 0x11;
constexpr uint8_t UDS_NEGATIVE_RESPONSE  = 0x7F;

// UDS Negative Response Codes
constexpr uint8_t UDS_NRC_SERVICE_NOT_SUPPORTED = 0x11;
constexpr uint8_t UDS_NRC_REQUEST_OUT_OF_RANGE  = 0x31;

// A simulated DTC (Diagnostic Trouble Code)
struct Dtc {
    uint32_t    code;    // e.g. 0x012345
    uint8_t     status;  // DTC status byte
    std::string description;
};

class UdsHandler {
public:
    UdsHandler();

    // Process a UDS request and return the response bytes
    std::vector<uint8_t> handle(const uint8_t* request, size_t len);

private:
    // Simulated DTC list
    std::vector<Dtc> dtcs_;

    // Simulated data identifiers
    // DID 0xF190 = VIN, DID 0xF18C = ECU serial number
    std::vector<uint8_t> handle_session_control(const uint8_t* req,
                                                  size_t len);
    std::vector<uint8_t> handle_read_dtc(const uint8_t* req, size_t len);
    std::vector<uint8_t> handle_read_data_by_id(const uint8_t* req,
                                                  size_t len);
    std::vector<uint8_t> handle_ecu_reset(const uint8_t* req, size_t len);
    std::vector<uint8_t> negative_response(uint8_t service_id,
                                            uint8_t nrc);
};
