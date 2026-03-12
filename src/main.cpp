#include "doip/doip_server.hpp"
#include "event/event_loop.hpp"
#include "can/can_socket.hpp"
#include "can/dbc_parser.hpp"
#include "can/signal_decoder.hpp"
#include "someip/someip_publisher.hpp"
#include "someip/someip_sd_service.hpp"
#include "utils/config_loader.hpp"
#include "utils/can_printer.hpp"
#include "utils/watchdog.hpp"
#include "bench/benchmark.hpp"
#include "utils/stats_reporter.hpp"
#include <iostream>
#include <iomanip>
#include <csignal>

constexpr auto     SOMEIP_MULTICAST = "239.0.0.1";
constexpr uint16_t SOMEIP_PORT      = 30490;
constexpr uint16_t SOMEIP_DATA_PORT = 30501;
constexpr size_t   STATS_EVERY_N    = 10;
constexpr size_t   SD_OFFER_EVERY_N = 100;  // offer every 100 frames (~10s)

static EventLoop*          g_loop  = nullptr;
static BenchmarkCollector* g_bench = nullptr;

void handle_sigint(int) {
    std::cout << "\n[MAIN] Shutting down...\n";
    if (g_bench) {
        StatsReporter reporter(*g_bench);
        reporter.print_table();
        std::cout << reporter.to_json();
    }
    if (g_loop) g_loop->stop();
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);

    // Get local IP from command line or use default
    // Usage: ./automotive-gateway 192.168.1.100
    std::string local_ip = "127.0.0.1";
    if (argc > 1) local_ip = argv[1];

    try {
        // --- Load config ---
        ConfigLoader config;
        if (!config.load("config/gateway.conf")) {
            std::cerr << "[FATAL] Could not load gateway.conf\n";
            return 1;
        }

        // --- Load DBC ---
        DbcParser dbc;
        if (!dbc.parse("config/vehicle.dbc")) {
            std::cerr << "[FATAL] Could not load vehicle.dbc\n";
            return 1;
        }

        // --- Core objects ---
        SignalDecoder      decoder;
        SomeIpPublisher    publisher(SOMEIP_MULTICAST, SOMEIP_PORT);
        BenchmarkCollector bench;
        Watchdog           watchdog(2000000);
        g_bench = &bench;

        // --- Register routes ---
        for (const auto& route : config.routes())
            publisher.register_signal(route.signal_name,
                                      route.service_id,
                                      route.method_id);

        EventLoop loop;
        CanSocket can("vcan0");

        // --- SOME/IP Service Discovery ---
        SomeIpSdService sd;
        sd.register_with_loop(loop);

        // Offer all three services
        sd.offer_service(0x0101, local_ip, SOMEIP_DATA_PORT);
        sd.offer_service(0x0102, local_ip, SOMEIP_DATA_PORT);
        sd.offer_service(0x0103, local_ip, SOMEIP_DATA_PORT);

        // Send initial offer immediately
        sd.send_offers();

        sd.on_subscribe = [](const Subscriber& sub) {
            std::cout << "[SD] New subscriber for Service=0x"
                      << std::hex << sub.service_id
                      << " from " << inet_ntoa(sub.addr.sin_addr)
                      << std::dec << "\n";
        };

        size_t frame_count      = 0;
        size_t last_offer_frame = 0;

        // --- CAN callback ---
        can.register_with_loop(loop, [&](const can_frame& frame) {
            watchdog.feed();

            uint64_t t0     = now_ns();
            uint32_t can_id = frame.can_id & CAN_EFF_MASK;

            const MessageDef* msg = dbc.find_message(can_id);
            if (!msg) return;

            uint64_t t1 = now_ns();
            bench.record("dbc_lookup", (t1 - t0) / 1000);

            auto signals = decoder.decode(frame, *msg);
            uint64_t t2  = now_ns();
            bench.record("signal_decode", (t2 - t1) / 1000);

            for (const auto& sig : signals)
                publisher.publish(sig);

            uint64_t t3 = now_ns();
            bench.record("someip_publish", (t3 - t2) / 1000);
            bench.record("end_to_end",     (t3 - t0) / 1000);

            frame_count++;

            // Watchdog check
            if (frame_count % 50 == 0)
                watchdog.check();

            // Periodic SD offer
            if (frame_count - last_offer_frame >= SD_OFFER_EVERY_N) {
                sd.send_offers();
                last_offer_frame = frame_count;
            }

            // Stats table
            if (frame_count % STATS_EVERY_N == 0) {
                std::cout << "[GW] " << frame_count
                          << " frames processed\n";
                StatsReporter reporter(bench);
                reporter.print_table();
            }
        });

// --- DoIP Diagnostic Server ---
        DoIpServer doip(13400);
        doip.register_with_loop(loop);
        std::cout << "[MAIN] Gateway ready\n";
        std::cout << "[MAIN] CAN:     vcan0\n";
        std::cout << "[MAIN] SOME/IP: "
                  << SOMEIP_MULTICAST << ":" << SOMEIP_PORT << "\n";
        std::cout << "[MAIN] SD:      "
                  << SOMEIP_SD_MULTICAST << ":" << SOMEIP_SD_PORT << "\n";
        std::cout << "[MAIN] Local IP: " << local_ip << "\n";
        std::cout << "[MAIN] Usage: ./automotive-gateway <your-ip>\n";

        g_loop = &loop;
        loop.run();

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
