# Network Performance Monitor - Implementation

C++ implementation of the Network Performance Monitor for monitoring bandwidth, latency, jitter, and packet loss.

## Current Status

**Phase 1: COMPLETED** ✓
- Bandwidth monitoring functionality implemented
- Interface detection
- Continuous monitoring support

**Phase 2: IN PROGRESS** (Weeks 3-4)
- ICMP-based latency measurement
- Raw socket programming

**Phase 3: PLANNED** (Weeks 5-6)
- Packet loss detection
- Jitter calculation
- Connection statistics

**Phase 4: PLANNED** (Weeks 7-8)
- Data logging (CSV export)
- Testing and optimization

## Build Instructions

```bash
cd network_monitor
make
```

This will create `bin/netmonitor` executable.

## Usage

List available network interfaces:
```bash
./bin/netmonitor --list
```

Single bandwidth measurement:
```bash
./bin/netmonitor --interface eth0
```

Continuous monitoring:
```bash
./bin/netmonitor --monitor wlan0 --interval 2
```

## Requirements

- C++ compiler with C++11 support (g++)
- Linux operating system
- Standard C++ libraries only (no external dependencies)

## Project Structure

```
network_monitor/
├── include/          # Header files
├── src/             # Source files
├── build/           # Compiled object files (generated)
├── bin/             # Executable (generated)
├── Makefile         # Build configuration
└── README.md        # This file
```

