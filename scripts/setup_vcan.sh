#!/bin/bash
echo "Setting up virtual CAN interface..."
# Load kernel modules
sudo modprobe vcan sudo modprobe can sudo modprobe can_raw
# Create virtual CAN interface
sudo ip link add dev vcan0 type vcan sudo ip link set up vcan0
# Verify
ip link show vcan0
echo "vcan0 is ready"

