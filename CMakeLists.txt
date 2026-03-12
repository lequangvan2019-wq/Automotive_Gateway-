cmake_minimum_required(VERSION 3.13)
project(automotive_gateway CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -O2)

add_executable(automotive-gateway
    src/main.cpp
    src/can/can_socket.cpp
    src/can/dbc_parser.cpp
    src/can/signal_decoder.cpp
    src/someip/someip_message.cpp
    src/someip/someip_publisher.cpp
    src/someip/someip_sd.cpp
    src/someip/someip_sd_service.cpp
    src/doip/doip_message.cpp
    src/doip/doip_server.cpp
    src/doip/uds_handler.cpp
    src/utils/config_loader.cpp
    src/utils/stats_reporter.cpp
    src/utils/watchdog.cpp
    src/bench/benchmark.cpp
)
target_include_directories(automotive-gateway PRIVATE src include)

add_executable(someip_subscriber     tools/someip_subscriber.cpp)
add_executable(someip_sd_subscriber  tools/someip_sd_subscriber.cpp)
add_executable(can_simulator         tools/can_simulator.cpp)
add_executable(stress_test           tools/stress_test.cpp)
add_executable(doip_client           tools/doip_client.cpp)
