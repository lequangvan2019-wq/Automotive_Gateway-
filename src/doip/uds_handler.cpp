#include "uds_handler.hpp"
#include <iostream>
#include <cstring>

UdsHandler::UdsHandler() {
    // Simulate some DTCs that would appear in a real ECU
    dtcs_.push_back({0x012345, 0x08, "Engine coolant temperature sensor"});
    dtcs_.push_back({0x023456, 0x09, "Throttle position sensor"});
    dtcs_.push_back({0x034567, 0x00, "Vehicle speed sensor (inactive)"});
}

std::vector<uint8_t> UdsHandler::handle(const uint8_t* req, size_t len) {
    if (len < 1) return negative_response(0x00, UDS_NRC_SERVICE_NOT_SUPPORTED);

    uint8_t service_id = req[0];
    std::cout << "[UDS] Service=0x" << std::hex << (int)service_id
              << std::dec << "\n";

    switch (service_id) {
    case UDS_SESSION_CONTROL:
        return handle_session_control(req, len);
    case UDS_READ_DTC:
        return handle_read_dtc(req, len);
    case UDS_READ_DATA_BY_ID:
        return handle_read_data_by_id(req, len);
    case UDS_ECU_RESET:
        return handle_ecu_reset(req, len);
    default:
        std::cout << "[UDS] Unknown service 0x"
                  << std::hex << (int)service_id << std::dec << "\n";
        return negative_response(service_id,
                                  UDS_NRC_SERVICE_NOT_SUPPORTED);
    }
}

std::vector<uint8_t> UdsHandler::handle_session_control(
    const uint8_t* req, size_t len)
{
    if (len < 2) return negative_response(UDS_SESSION_CONTROL,
                                           UDS_NRC_REQUEST_OUT_OF_RANGE);
    uint8_t session_type = req[1];
    std::cout << "[UDS] SessionControl type=0x"
              << std::hex << (int)session_type << std::dec << "\n";

    // Positive response: 0x50 + session type
    return {0x50, session_type};
}

std::vector<uint8_t> UdsHandler::handle_read_dtc(
    const uint8_t* req, size_t len)
{
    if (len < 2) return negative_response(UDS_READ_DTC,
                                           UDS_NRC_REQUEST_OUT_OF_RANGE);

    uint8_t sub_func = req[1];
    std::cout << "[UDS] ReadDTC subFunction=0x"
              << std::hex << (int)sub_func << std::dec
              << " — " << dtcs_.size() << " DTCs stored\n";

    // Response: 0x59 (positive response SID) + subFunction
    // + DTC count + DTC records
    std::vector<uint8_t> response;
    response.push_back(0x59);           // positive response
    response.push_back(sub_func);
    response.push_back(0xFF);           // DTC status availability mask
    response.push_back(0x09);           // DTC format: ISO 14229-1
    response.push_back(0x00);           // DTC count high
    response.push_back(static_cast<uint8_t>(dtcs_.size())); // DTC count low

    // Append each DTC: 3 bytes code + 1 byte status
    for (const auto& dtc : dtcs_) {
        response.push_back((dtc.code >> 16) & 0xFF);
        response.push_back((dtc.code >>  8) & 0xFF);
        response.push_back( dtc.code        & 0xFF);
        response.push_back(dtc.status);
        std::cout << "[UDS]   DTC 0x" << std::hex << dtc.code
                  << " status=0x" << (int)dtc.status
                  << " (" << dtc.description << ")\n";
    }

    return response;
}

std::vector<uint8_t> UdsHandler::handle_read_data_by_id(
    const uint8_t* req, size_t len)
{
    if (len < 3) return negative_response(UDS_READ_DATA_BY_ID,
                                           UDS_NRC_REQUEST_OUT_OF_RANGE);

    uint16_t did = (req[1] << 8) | req[2];
    std::cout << "[UDS] ReadDataByID DID=0x"
              << std::hex << did << std::dec << "\n";

    std::vector<uint8_t> response;
    response.push_back(0x62);            // positive response SID
    response.push_back((did >> 8) & 0xFF);
    response.push_back( did       & 0xFF);

    switch (did) {
    case 0xF190: {
        // VIN (Vehicle Identification Number) — 17 chars
        std::string vin = "BBB0000000000001";  // BeagleBone demo VIN
        response.insert(response.end(), vin.begin(), vin.end());
        std::cout << "[UDS]   VIN: " << vin << "\n";
        break;
    }
    case 0xF18C: {
        // ECU Serial Number
        std::string serial = "ECU-BBB-2024-001";
        response.insert(response.end(), serial.begin(), serial.end());
        std::cout << "[UDS]   ECU Serial: " << serial << "\n";
        break;
    }
    case 0xF187: {
        // Software version
        std::string version = "v1.0.0-automotive-gateway";
        response.insert(response.end(), version.begin(), version.end());
        std::cout << "[UDS]   SW Version: " << version << "\n";
        break;
    }
    default:
        std::cout << "[UDS]   Unknown DID 0x"
                  << std::hex << did << std::dec << "\n";
        return negative_response(UDS_READ_DATA_BY_ID,
                                  UDS_NRC_REQUEST_OUT_OF_RANGE);
    }

    return response;
}

std::vector<uint8_t> UdsHandler::handle_ecu_reset(
    const uint8_t* req, size_t len)
{
    if (len < 2) return negative_response(UDS_ECU_RESET,
                                           UDS_NRC_REQUEST_OUT_OF_RANGE);
    uint8_t reset_type = req[1];
    std::cout << "[UDS] ECUReset type=0x"
              << std::hex << (int)reset_type << std::dec
              << " (simulated — not actually resetting)\n";

    // Positive response: 0x51 + reset type
    return {0x51, reset_type};
}

std::vector<uint8_t> UdsHandler::negative_response(uint8_t service_id,
                                                     uint8_t nrc)
{
    std::cout << "[UDS] Negative response service=0x"
              << std::hex << (int)service_id
              << " NRC=0x" << (int)nrc << std::dec << "\n";
    return {UDS_NEGATIVE_RESPONSE, service_id, nrc};
}
