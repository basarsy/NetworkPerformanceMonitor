#include "network_monitor.h"
#include <iostream>
#include <string>
#include <cstdlib>

void printUsage(const char* program_name) {
    std::cout << "Network Performance Monitor - Phase 1: Bandwidth Monitoring" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -l, --list              List available network interfaces" << std::endl;
    std::cout << "  -i, --interface <name>  Monitor specific interface (single reading)" << std::endl;
    std::cout << "  -m, --monitor <name>    Continuously monitor interface" << std::endl;
    std::cout << "  -t, --interval <sec>    Set monitoring interval (default: 1 second)" << std::endl;
    std::cout << "  -h, --help              Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " --list" << std::endl;
    std::cout << "  " << program_name << " --interface eth0" << std::endl;
    std::cout << "  " << program_name << " --monitor wlan0 --interval 2" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: This program reads from /proc/net/dev and does not require" << std::endl;
    std::cout << "      root privileges for bandwidth monitoring." << std::endl;
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
    int interval = 1;
    
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
    }
    else if (mode == "continuous") {
        monitor.monitorBandwidthContinuous(interface, interval);
    }
    else {
        std::cerr << "Error: No valid mode specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}

