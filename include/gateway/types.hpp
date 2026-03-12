#pragma once
#include <cstdint> 
#include <string> 
#include <functional>
// A decoded CAN signal — name + physical value + unit
struct DecodedSignal {
	 std::string name;
	 double value;
	 std::string unit;
};
// Callback types used across layers
using OnSignalDecoded = std::function<void(const DecodedSignal&)>;
