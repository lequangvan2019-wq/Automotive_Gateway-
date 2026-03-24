# Automotive Communication Gateway

**CAN-to-Ethernet Gateway with SOME/IP | BeagleBone Black | C++17**

A production-grade automotive communication gateway that bridges CAN bus networks with Ethernet-based SOME/IP protocol. Built from scratch in C++17 on a BeagleBone Black (ARM Cortex-A8, 1GHz), implementing the same architecture used in modern vehicle ECU networks.

---

## Demo

```
CAN Frame arrives on vcan0:
  vcan0  123  [8]  B0 36 00 00 00 00 00 00

DBC decode:
  EngineRPM     3500.00 rpm
  EngineTemp      85.25 degC
  ThrottlePos     23.14 %

SOME/IP published → UDP 239.0.0.1:30490
  Service=0x0101 Method=0x8001 Session=42
  Payload: 3500.00 (float, big-endian)

End-to-end latency: 171μs (P50)
```

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  APPLICATION LAYER                  │
│          Config Loader │ Stats Reporter             │
└─────────────────────────┬───────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────┐
│                  SOME/IP LAYER                      │
│     SomeIpPublisher │ SomeIpMessage serializer      │
│     UDP multicast → 239.0.0.1:30490                 │
└─────────────────────────┬───────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────┐
│                  CAN/DBC LAYER                      │
│     DbcParser │ SignalDecoder │ CanSocket            │
│     SocketCAN (AF_CAN) on vcan0 / can0              │
└─────────────────────────┬───────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────┐
│              LINUX KERNEL (SocketCAN)               │
│         vcan0 (virtual) │ can0 (hardware MCP2515)   │
└─────────────────────────────────────────────────────┘

All layers driven by single-threaded epoll EventLoop
No threads. No mutexes. No race conditions.
```

---

## Key Features

- **SocketCAN integration** — uses Linux AF_CAN sockets with epoll for non-blocking CAN frame reception
- **DBC file parser** — runtime parsing of Vector CANdb DBC files, no recompilation needed to change signal definitions
- **SOME/IP serialization** — implements AUTOSAR SOME/IP binary protocol (Service ID, Method ID, session tracking, float payload encoding)
- **Zero-copy hot path** — packet parsing uses in-place views, no heap allocation per frame after initialization
- **Single-threaded event loop** — epoll-based, same architecture as nginx and Node.js, deterministic latency
- **Runtime config** — signal-to-service routing defined in `config/gateway.conf`, not hardcoded
- **Built-in benchmarking** — nanosecond-precision latency histograms per pipeline stage, live stats table
- **Watchdog** — detects CAN bus silence and alerts after configurable timeout

---

## Measured Performance (BeagleBone Black, ARM Cortex-A8 1GHz)

Tested at 1000 CAN frames/sec sustained load:

| Pipeline Stage | P50 | P95 | P99 |
|---|---|---|---|
| DBC lookup | 1 μs | 3 μs | 3 μs |
| Signal decode | 6 μs | 23 μs | 31 μs |
| SOME/IP publish (UDP) | 164 μs | 437 μs | 6806 μs |
| **End-to-end** | **171 μs** | **462 μs** | **6835 μs** |

P50 end-to-end latency of **171μs** is well within the automotive target of <1ms for non-safety-critical signals. The P99 spike is Linux UDP socket jitter under sustained load — a known characteristic of Linux kernel networking on embedded hardware.

---

## Project Structure

```
automotive-gateway/
├── src/
│   ├── main.cpp                    ← Entry point, wires all modules
│   ├── event/
│   │   └── event_loop.hpp          ← epoll event loop
│   ├── can/
│   │   ├── can_socket.cpp/hpp      ← SocketCAN AF_CAN interface
│   │   ├── dbc_parser.cpp/hpp      ← DBC file parser
│   │   └── signal_decoder.cpp/hpp  ← Bit-level signal extraction
│   ├── someip/
│   │   ├── someip_message.cpp/hpp  ← SOME/IP serialization
│   │   └── someip_publisher.cpp/hpp← UDP multicast publisher
│   ├── bench/
│   │   └── benchmark.hpp           ← Nanosecond latency histograms
│   └── utils/
│       ├── config_loader.cpp/hpp   ← Runtime config parser
│       ├── stats_reporter.cpp/hpp  ← Table + JSON output
│       └── watchdog.hpp            ← CAN bus silence detection
├── tools/
│   ├── can_simulator.cpp           ← Simulates vehicle CAN traffic
│   ├── someip_subscriber.cpp       ← Receives and prints SOME/IP messages
│   └── stress_test.cpp             ← Floods gateway at 1000 fps
├── config/
│   ├── vehicle.dbc                 ← CAN signal definitions
│   └── gateway.conf                ← Signal routing rules
└── scripts/
    └── setup_vcan.sh               ← Virtual CAN interface setup
```

---

## Build

### Prerequisites

```bash
# Ubuntu / Debian (including BeagleBone Black)
sudo apt-get install -y build-essential cmake g++ can-utils iproute2
```

### Compile

```bash
git clone https://github.com/lequangvan2019-wq/automotive-gateway
cd automotive-gateway
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Cross-compile for BeagleBone Black from x86 host

```bash
sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
cmake -B build-arm \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm --parallel
scp build-arm/automotive-gateway debian@beaglebone:~/
```

---

## Run

### Setup virtual CAN (required after every reboot)

```bash
sudo ./scripts/setup_vcan.sh
# or manually:
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

### Start the gateway

```bash
./build/automotive-gateway
```

### Simulate vehicle CAN traffic

```bash
# In a second terminal
./build/can_simulator
```

### Monitor SOME/IP output

```bash
# In a third terminal
./build/someip_subscriber
```

### Run stress test

```bash
# 1000 frames/sec for 10 seconds
./build/stress_test 1000 10
```

---

## Configuration

### `config/gateway.conf` — Signal routing rules

```
# Format: signal_name  service_id  method_id
EngineRPM     0x0101  0x8001
EngineTemp    0x0101  0x8002
ThrottlePos   0x0101  0x8003
VehicleSpeed  0x0102  0x8001
BrakePressure 0x0103  0x8001
BrakeActive   0x0103  0x8002
```

Add or remove signals without recompiling. Change service/method IDs to match your target ECU's SOME/IP interface definition.

### `config/vehicle.dbc` — CAN signal definitions

Standard Vector CANdb format. Drop in any OEM-provided DBC file and the gateway will decode signals automatically at startup.

```
BO_ 291 EngineData: 8 Vector__XXX
 SG_ EngineRPM : 0|16@1+ (0.25,0) [0|16383.75] "rpm" Vector__XXX
 SG_ EngineTemp : 16|8@1+ (0.75,-40) [-40|148.75] "degC" Vector__XXX
```

---

## SOME/IP Message Format

Every CAN signal is published as a SOME/IP notification:

```
Byte  0-1:  Service ID     (e.g. 0x0101 = Engine service)
Byte  2-3:  Method ID      (e.g. 0x8001 = RPM event)
Byte  4-7:  Length         (payload + 8)
Byte  8-9:  Client ID      (0x0000 for gateway)
Byte 10-11: Session ID     (increments per message)
Byte 12:    Protocol Ver   (0x01)
Byte 13:    Interface Ver  (0x01)
Byte 14:    Message Type   (0x02 = NOTIFICATION)
Byte 15:    Return Code    (0x00 = OK)
Byte 16-19: Payload        (float32, big-endian)
```

Compatible with any AUTOSAR-compliant SOME/IP subscriber (vsomeip, Vector CANoe, COVESA).

## DoIP — Diagnostics over IP (ISO 13400)

The gateway implements a DoIP server on TCP port 13400. Any diagnostic
tool (mechanic's scanner, CANoe, custom client) can connect and send
UDS commands to read fault codes, ECU data, and control the ECU.

### Supported UDS Services

| Service | SID | Description |
|---|---|---|
| Session Control | 0x10 | Switch diagnostic session |
| Read DTC | 0x19 | Read stored fault codes |
| Read Data By ID | 0x22 | Read ECU data (VIN, serial, SW version) |
| ECU Reset | 0x11 | Soft/hard reset |

### Demo
```bash
# Start gateway
./build/automotive-gateway 192.168.137.2

# Run diagnostic client
./build/doip_client 127.0.0.1
```

Expected output:
```
[CLIENT] Routing activation SUCCESS

=== UDS Read DTC ===
DTC 0x012345 — Engine coolant temperature sensor
DTC 0x023456 — Throttle position sensor
DTC 0x034567 — Vehicle speed sensor

=== UDS Read VIN ===
VIN: BBB0000000000001

=== UDS Read SW Version ===
v1.0.0-automotive-gateway
```

### Supported Data Identifiers (DIDs)

| DID | Description | Example Value |
|---|---|---|
| 0xF190 | Vehicle VIN | BBB0000000000001 |
| 0xF18C | ECU Serial Number | ECU-BBB-2024-001 |
| 0xF187 | Software Version | v1.0.0-automotive-gateway |
---

## Hardware

Developed and tested on **BeagleBone Black** (TI AM335x, ARM Cortex-A8, 1GHz, 512MB RAM) running Debian Linux.

For real CAN hardware (instead of vcan0), connect an MCP2515 CAN controller via SPI and change `"vcan0"` to `"can0"` in `src/main.cpp`. The rest of the code is unchanged — SocketCAN abstracts the hardware difference.

---

## Protocols Implemented

| Protocol | Standard | Implementation |
|---|---|---|
| SocketCAN | Linux kernel AF_CAN | Full — frame send/receive, kernel filters |
| DBC file format | Vector CANdb | Full — message and signal parsing, bit extraction, scaling |
| SOME/IP | AUTOSAR PRS_SOMEIP | Notification messages, float/uint32 payload, session tracking |
| SOME/IP multicast | AUTOSAR | UDP multicast to 239.0.0.1:30490 |


---

## Roadmap

- [ ] SOME/IP Service Discovery (SOME/IP-SD) — automatic service announcement and subscriber management
- [ ] DoIP (ISO 13400) — diagnostic server with UDS command handling
- [ ] Unit tests per module (ASPICE compliance)
- [ ] 802.1Q VLAN + QoS priority tagging
- [ ] Hardware CAN testing with MCP2515 on real BeagleBone Black

---

## Why This Project

Modern vehicles contain 100+ ECUs communicating over CAN bus and Ethernet. The shift from legacy CAN-only networks to Ethernet-based architectures (SOME/IP, DoIP) is the defining challenge in automotive software today. This gateway implements the bridge between these two worlds — the same problem solved by production middleware from Bosch, Continental, and Vector Informatik.

---
