#include "network_monitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <cstring>

NetworkMonitor::NetworkMonitor() {
    detectInterfaces();
}

NetworkMonitor::~NetworkMonitor() {
    // Cleanup if needed
}

// Detect available network interfaces
bool NetworkMonitor::detectInterfaces() {
    available_interfaces_.clear();
    
    std::ifstream proc_file("/proc/net/dev");
    if (!proc_file.is_open()) {
        std::cerr << "Error: Unable to open /proc/net/dev" << std::endl;
        return false;
    }
    
    std::string line;
    // Skip first two header lines
    std::getline(proc_file, line);
    std::getline(proc_file, line);
    
    while (std::getline(proc_file, line)) {
        std::istringstream iss(line);
        std::string interface;
        iss >> interface;
        
        // Remove the colon from interface name
        if (!interface.empty() && interface.back() == ':') {
            interface.pop_back();
        }
        
        // Skip loopback interface
        if (interface != "lo") {
            available_interfaces_.push_back(interface);
        }
    }
    
    proc_file.close();
    return !available_interfaces_.empty();
}

// Get list of available interfaces
std::vector<std::string> NetworkMonitor::getAvailableInterfaces() const {
    return available_interfaces_;
}

// Parse /proc/net/dev and read all interface statistics
bool NetworkMonitor::parseProcNetDev(std::map<std::string, InterfaceStats>& stats) {
    std::ifstream proc_file("/proc/net/dev");
    if (!proc_file.is_open()) {
        return false;
    }
    
    std::string line;
    // Skip first two header lines
    std::getline(proc_file, line);
    std::getline(proc_file, line);
    
    auto current_time = std::chrono::steady_clock::now();
    
    while (std::getline(proc_file, line)) {
        std::istringstream iss(line);
        InterfaceStats stat;
        
        iss >> stat.interface_name;
        
        // Remove colon from interface name
        if (!stat.interface_name.empty() && stat.interface_name.back() == ':') {
            stat.interface_name.pop_back();
        }
        
        // Read statistics
        // Format: bytes packets errs drop fifo frame compressed multicast
        iss >> stat.bytes_received >> stat.packets_received;
        
        // Skip errs, drop, fifo, frame, compressed, multicast for RX
        unsigned long long temp;
        for (int i = 0; i < 6; i++) {
            iss >> temp;
        }
        
        // Read TX stats
        iss >> stat.bytes_sent >> stat.packets_sent;
        
        stat.timestamp = current_time;
        stats[stat.interface_name] = stat;
    }
    
    proc_file.close();
    return true;
}

// Read statistics for a specific interface
bool NetworkMonitor::readInterfaceStats(const std::string& interface, InterfaceStats& stats) {
    std::map<std::string, InterfaceStats> all_stats;
    if (!parseProcNetDev(all_stats)) {
        return false;
    }
    
    auto it = all_stats.find(interface);
    if (it == all_stats.end()) {
        return false;
    }
    
    stats = it->second;
    return true;
}

// Calculate time difference in seconds
double NetworkMonitor::calculateTimeDiff(const std::chrono::steady_clock::time_point& start,
                                        const std::chrono::steady_clock::time_point& end) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return duration.count() / 1000.0;
}

// Calculate bandwidth based on two consecutive measurements
void NetworkMonitor::calculateBandwidth(const InterfaceStats& prev, const InterfaceStats& current,
                                       double& download_bps, double& upload_bps) {
    // Calculate time difference in seconds
    double time_diff = calculateTimeDiff(prev.timestamp, current.timestamp);
    
    if (time_diff <= 0) {
        download_bps = 0.0;
        upload_bps = 0.0;
        return;
    }
    
    // Calculate bytes transferred
    unsigned long long bytes_recv_diff = current.bytes_received - prev.bytes_received;
    unsigned long long bytes_sent_diff = current.bytes_sent - prev.bytes_sent;
    
    // Convert to bits per second (multiply by 8)
    download_bps = (bytes_recv_diff * 8.0) / time_diff;
    upload_bps = (bytes_sent_diff * 8.0) / time_diff;
}

// Display bandwidth for a specific interface (single measurement)
void NetworkMonitor::displayBandwidth(const std::string& interface) {
    InterfaceStats stats1, stats2;
    
    // First reading
    if (!readInterfaceStats(interface, stats1)) {
        std::cerr << "Error reading interface: " << interface << std::endl;
        return;
    }
    
    // Wait 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Second reading
    if (!readInterfaceStats(interface, stats2)) {
        std::cerr << "Error reading interface: " << interface << std::endl;
        return;
    }
    
    // Calculate bandwidth
    double download_bps, upload_bps;
    calculateBandwidth(stats1, stats2, download_bps, upload_bps);
    
    // Display results
    std::cout << "Interface: " << interface << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    // Convert to appropriate units (Kbps, Mbps)
    if (download_bps > 1000000) {
        std::cout << "  Download: " << (download_bps / 1000000.0) << " Mbps" << std::endl;
    } else if (download_bps > 1000) {
        std::cout << "  Download: " << (download_bps / 1000.0) << " Kbps" << std::endl;
    } else {
        std::cout << "  Download: " << download_bps << " bps" << std::endl;
    }
    
    if (upload_bps > 1000000) {
        std::cout << "  Upload:   " << (upload_bps / 1000000.0) << " Mbps" << std::endl;
    } else if (upload_bps > 1000) {
        std::cout << "  Upload:   " << (upload_bps / 1000.0) << " Kbps" << std::endl;
    } else {
        std::cout << "  Upload:   " << upload_bps << " bps" << std::endl;
    }
}

// Monitor bandwidth continuously
void NetworkMonitor::monitorBandwidthContinuous(const std::string& interface, int interval_seconds) {
    InterfaceStats prev_stats, current_stats;
    
    std::cout << "Starting continuous bandwidth monitoring for interface: " << interface << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl << std::endl;
    
    // Get initial reading
    if (!readInterfaceStats(interface, prev_stats)) {
        std::cerr << "Error: Unable to read interface " << interface << std::endl;
        return;
    }
    
    while (true) {
        // Wait for specified interval
        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
        
        // Read current stats
        if (!readInterfaceStats(interface, current_stats)) {
            std::cerr << "Error reading interface stats" << std::endl;
            break;
        }
        
        // Calculate bandwidth
        double download_bps, upload_bps;
        calculateBandwidth(prev_stats, current_stats, download_bps, upload_bps);
        
        // Get current time for display
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::string time_str = std::ctime(&time_t_now);
        time_str.pop_back(); // Remove newline
        
        // Display results
        std::cout << "[" << time_str << "] " << interface << " - ";
        std::cout << std::fixed << std::setprecision(2);
        
        // Download
        if (download_bps > 1000000) {
            std::cout << "↓ " << (download_bps / 1000000.0) << " Mbps";
        } else if (download_bps > 1000) {
            std::cout << "↓ " << (download_bps / 1000.0) << " Kbps";
        } else {
            std::cout << "↓ " << download_bps << " bps";
        }
        
        std::cout << " | ";
        
        // Upload
        if (upload_bps > 1000000) {
            std::cout << "↑ " << (upload_bps / 1000000.0) << " Mbps";
        } else if (upload_bps > 1000) {
            std::cout << "↑ " << (upload_bps / 1000.0) << " Kbps";
        } else {
            std::cout << "↑ " << upload_bps << " bps";
        }
        
        std::cout << std::endl;
        
        // Update previous stats
        prev_stats = current_stats;
    }
}

// Placeholder implementations for future phases
LatencyResult NetworkMonitor::measureLatency(const std::string& host, int timeout_ms) {
    // To be implemented in Phase 2
    LatencyResult result;
    result.host = host;
    result.success = false;
    result.rtt_ms = 0.0;
    std::cout << "Latency measurement will be implemented in Phase 2" << std::endl;
    return result;
}

PacketLossStats NetworkMonitor::detectPacketLoss(const std::string& host, int count) {
    // To be implemented in Phase 3
    PacketLossStats stats = {0};
    std::cout << "Packet loss detection will be implemented in Phase 3" << std::endl;
    return stats;
}

void NetworkMonitor::displayActiveConnections() {
    // To be implemented in Phase 3
    std::cout << "Connection statistics will be implemented in Phase 3" << std::endl;
}

void NetworkMonitor::logToCSV(const std::string& filename) {
    // To be implemented in Phase 4
    std::cout << "CSV logging will be implemented in Phase 4" << std::endl;
}

