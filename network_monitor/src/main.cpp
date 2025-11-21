#include "network_monitor.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <iomanip>

void printUsage(const char* program_name) {
    std::cout << "Network Performance Monitor - All Phases Complete" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -l, --list              List available network interfaces" << std::endl;
    std::cout << "  -i, --interface <name>  Monitor specific interface (single reading)" << std::endl;
    std::cout << "  -m, --monitor <name>    Continuously monitor interface" << std::endl;
    std::cout << "  -t, --interval <sec>    Set monitoring interval (default: 1 second)" << std::endl;
    std::cout << "  -p, --ping <host>       Measure latency to host (ICMP ping)" << std::endl;
    std::cout << "  --timeout <ms>          Set timeout for ping in milliseconds (default: 1000)" << std::endl;
    std::cout << "  --packetloss <host>     Detect packet loss and jitter (default: 10 packets)" << std::endl;
    std::cout << "  --count <num>           Number of packets for packet loss test (default: 10)" << std::endl;
    std::cout << "  -c, --connections       Display active network connections" << std::endl;
    std::cout << "  --log <filename>        Log data to CSV file (use with other commands)" << std::endl;
    std::cout << "  -h, --help              Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " --list" << std::endl;
    std::cout << "  " << program_name << " --interface eth0" << std::endl;
    std::cout << "  " << program_name << " --monitor wlan0 --interval 2" << std::endl;
    std::cout << "  " << program_name << " --ping 8.8.8.8" << std::endl;
    std::cout << "  " << program_name << " --packetloss 8.8.8.8 --count 20" << std::endl;
    std::cout << "  " << program_name << " --connections" << std::endl;
    std::cout << "  " << program_name << " --interface eth0 --log bandwidth.csv" << std::endl;
    std::cout << "  " << program_name << " --ping 8.8.8.8 --log latency.csv" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: Bandwidth monitoring and connection stats do not require root privileges." << std::endl;
    std::cout << "      Latency and packet loss measurement require root privileges." << std::endl;
}

int main(int argc, char* argv[]) {
    NetworkMonitor monitor;
    
    // No arguments - show help
    if (argc == 1) {
        printUsage(argv[0]);
        return 0;
    }
    
    // Parse command line arguments
    std::string mode = "";
    std::string interface = "";
    std::string ping_host = "";
    std::string packetloss_host = "";
    std::string log_file = "";
    int interval = 1;
    int timeout_ms = 1000;
    int packet_count = 10;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-l" || arg == "--list") {
            mode = "list";
        }
        else if (arg == "-i" || arg == "--interface") {
            if (i + 1 < argc) {
                mode = "single";
                interface = argv[++i];
            } else {
                std::cerr << "Error: --interface requires an interface name" << std::endl;
                return 1;
            }
        }
        else if (arg == "-m" || arg == "--monitor") {
            if (i + 1 < argc) {
                mode = "continuous";
                interface = argv[++i];
            } else {
                std::cerr << "Error: --monitor requires an interface name" << std::endl;
                return 1;
            }
        }
        else if (arg == "-t" || arg == "--interval") {
            if (i + 1 < argc) {
                interval = std::atoi(argv[++i]);
                if (interval <= 0) {
                    std::cerr << "Error: interval must be a positive integer" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --interval requires a number" << std::endl;
                return 1;
            }
        }
        else if (arg == "-p" || arg == "--ping") {
            if (i + 1 < argc) {
                mode = "ping";
                ping_host = argv[++i];
            } else {
                std::cerr << "Error: --ping requires a hostname or IP address" << std::endl;
                return 1;
            }
        }
        else if (arg == "--timeout") {
            if (i + 1 < argc) {
                timeout_ms = std::atoi(argv[++i]);
                if (timeout_ms <= 0) {
                    std::cerr << "Error: timeout must be a positive integer" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --timeout requires a number" << std::endl;
                return 1;
            }
        }
        else if (arg == "--packetloss") {
            if (i + 1 < argc) {
                mode = "packetloss";
                packetloss_host = argv[++i];
            } else {
                std::cerr << "Error: --packetloss requires a hostname or IP address" << std::endl;
                return 1;
            }
        }
        else if (arg == "--count") {
            if (i + 1 < argc) {
                packet_count = std::atoi(argv[++i]);
                if (packet_count <= 0) {
                    std::cerr << "Error: count must be a positive integer" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --count requires a number" << std::endl;
                return 1;
            }
        }
        else if (arg == "-c" || arg == "--connections") {
            mode = "connections";
        }
        else if (arg == "--log") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            } else {
                std::cerr << "Error: --log requires a filename" << std::endl;
                return 1;
            }
        }
        else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Execute based on mode
    if (mode == "list") {
        std::cout << "Available network interfaces:" << std::endl;
        auto interfaces = monitor.getAvailableInterfaces();
        if (interfaces.empty()) {
            std::cout << "  No interfaces found (excluding loopback)" << std::endl;
        } else {
            for (const auto& iface : interfaces) {
                std::cout << "  - " << iface << std::endl;
            }
        }
    }
    else if (mode == "single") {
        std::cout << "Measuring bandwidth for interface: " << interface << std::endl;
        std::cout << "Please wait..." << std::endl << std::endl;
        monitor.displayBandwidth(interface);
        
        // Log to CSV if requested
        if (!log_file.empty()) {
            double download_bps, upload_bps;
            if (monitor.getBandwidth(interface, download_bps, upload_bps)) {
                monitor.logBandwidthToCSV(log_file, interface, download_bps, upload_bps);
                std::cout << "Data logged to: " << log_file << std::endl;
            }
        }
    }
    else if (mode == "continuous") {
        monitor.monitorBandwidthContinuous(interface, interval, log_file);
    }
    else if (mode == "ping") {
        std::cout << "Pinging " << ping_host << "..." << std::endl;
        LatencyResult result = monitor.measureLatency(ping_host, timeout_ms);
        
        if (result.success) {
            std::cout << "Reply from " << result.host << ": time=" 
                      << std::fixed << std::setprecision(2) << result.rtt_ms 
                      << " ms" << std::endl;
            
            // Log to CSV if requested
            if (!log_file.empty()) {
                monitor.logLatencyToCSV(log_file, result);
                std::cout << "Data logged to: " << log_file << std::endl;
            }
        } else {
            std::cout << "Request timed out or failed." << std::endl;
            // Log failed attempt if requested
            if (!log_file.empty()) {
                monitor.logLatencyToCSV(log_file, result);
            }
            return 1;
        }
    }
    else if (mode == "packetloss") {
        PacketLossStats stats = monitor.detectPacketLoss(packetloss_host, packet_count);
        
        std::cout << std::endl << "Packet Loss Statistics:" << std::endl;
        std::cout << "=========================" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Packets sent:     " << stats.packets_sent << std::endl;
        std::cout << "Packets received: " << stats.packets_received << std::endl;
        std::cout << "Packet loss:      " << stats.loss_percentage << "%" << std::endl;
        
        if (stats.packets_received > 0) {
            std::cout << "Min RTT:          " << stats.min_rtt << " ms" << std::endl;
            std::cout << "Max RTT:          " << stats.max_rtt << " ms" << std::endl;
            std::cout << "Avg RTT:          " << stats.avg_rtt << " ms" << std::endl;
            std::cout << "Jitter:           " << stats.jitter << " ms" << std::endl;
        }
        
        // Log to CSV if requested
        if (!log_file.empty()) {
            monitor.logPacketLossToCSV(log_file, packetloss_host, stats);
            std::cout << "Data logged to: " << log_file << std::endl;
        }
    }
    else if (mode == "connections") {
        monitor.displayActiveConnections();
        
        // Log to CSV if requested
        if (!log_file.empty()) {
            int tcp_total, tcp_established, udp_total;
            if (monitor.getConnectionStats(tcp_total, tcp_established, udp_total)) {
                monitor.logConnectionsToCSV(log_file, tcp_total, tcp_established, udp_total);
                std::cout << "Data logged to: " << log_file << std::endl;
            }
        }
    }
    else {
        std::cerr << "Error: No valid mode specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}

