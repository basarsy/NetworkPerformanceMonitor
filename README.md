# Network Performance Monitor - Implementation

C++ implementation of the Network Performance Monitor for monitoring bandwidth, latency, jitter, and packet loss.

## Overview

The monitor collects real-time network telemetry directly from Linux system interfaces (no third‑party dependencies). It supports:

- **Bandwidth analytics** from `/proc/net/dev`, with single-shot or continuous sampling.
- **Latency and jitter** measurement using raw-socket ICMP echo requests.
- **Packet loss statistics**, including min/max/avg RTT and jitter calculations.
- **Connection insights** by parsing `/proc/net/tcp` and `/proc/net/udp`.
- **CSV logging** for every metric so the data can be graphed or fed into reports later.

All features are exposed through a single CLI tool, making it easy to run quick diagnostics or automate longer experiments.

## Build Instructions

```bash
cd network_monitor
make
```

This will create `bin/netmonitor` executable.

## Usage Examples

### Basic Commands

**List available network interfaces:**
```bash
./bin/netmonitor --list
```

**Show help message:**
```bash
./bin/netmonitor --help
```

### Bandwidth Monitoring

**Single measurement (optionally log to CSV):**
```bash
./bin/netmonitor --interface wlp0s20f3 --log bandwidth.csv
```

**Continuous monitoring with optional logging (Ctrl+C to stop):**
```bash
./bin/netmonitor --monitor wlp0s20f3 --interval 2 --log continuous_bandwidth.csv
```

### Latency Measurement (Requires Root)

**Single ping to IP address:**
```bash
sudo ./bin/netmonitor --ping 8.8.8.8
```

**Ping hostname with custom timeout (logs if desired):**
```bash
sudo ./bin/netmonitor --ping google.com --timeout 2000 --log latency.csv
```

### Packet Loss and Jitter Detection (Requires Root)

**Packet loss test with custom packet count (logs to CSV if set):**
```bash
sudo ./bin/netmonitor --packetloss 8.8.8.8 --count 20 --log packetloss.csv
```

### Connection Statistics

**Display active network connections:**
```bash
./bin/netmonitor --connections
```

**Log connection statistics to CSV:**
```bash
./bin/netmonitor --connections --log connections.csv
```

### Notes

- **Root privileges required:** Latency measurement (`--ping`) and packet loss detection (`--packetloss`) require root privileges because they use raw sockets. Use `sudo` for these commands.

- **Interface names:** Replace `wlp0s20f3` with your actual network interface name. Use `--list` to find available interfaces.

- **CSV files:** CSV files are created automatically. If a file exists, new data is appended. Headers are added only for new files.

- **Continuous monitoring:** Press `Ctrl+C` to stop continuous bandwidth monitoring.

## Requirements

- C++ compiler with C++11 support (g++)
- Linux operating system
- Standard C++ libraries + POSIX system libraries (no external dependencies to install)
- Root privileges for latency and packet loss measurement (raw sockets)

**Libraries Used:**
- Standard C++: `<iostream>`, `<fstream>`, `<thread>`, `<string>`, `<algorithm>`, `<cmath>`, etc.
- POSIX/Linux system: `<sys/socket.h>`, `<netinet/ip.h>`, `<netinet/ip_icmp.h>`, etc.
- All libraries are built into Linux systems - no installation required

## Project Structure

```
network_monitor/
├── include/          # Header files
├── src/             # Source files
├── build/           # Compiled object files (generated)
├── bin/             # Executable (generated)
├── Makefile         # Build configuration
└── README.md        
```

