#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <string>
#include <map>
#include <vector>
#include <chrono>

// Structure to hold network interface statistics
struct InterfaceStats {
    std::string interface_name;
    unsigned long long bytes_received;
    unsigned long long bytes_sent;
    unsigned long long packets_received;
    unsigned long long packets_sent;
    std::chrono::steady_clock::time_point timestamp;
};

// Structure to hold latency measurement results
struct LatencyResult {
    double rtt_ms;          // Round-trip time in milliseconds
    bool success;           // Whether the ping was successful
    std::string host;       // Target host
};

// Structure to hold packet loss statistics
struct PacketLossStats {
    int packets_sent;
    int packets_received;
    double loss_percentage;
    double min_rtt;
    double max_rtt;
    double avg_rtt;
    double jitter;          // Standard deviation of RTT
};

// Main Network Monitor class
class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();
    
    // Bandwidth monitoring
    bool detectInterfaces();
    std::vector<std::string> getAvailableInterfaces() const;
    bool readInterfaceStats(const std::string& interface, InterfaceStats& stats);
    void calculateBandwidth(const InterfaceStats& prev, const InterfaceStats& current,
                           double& download_bps, double& upload_bps);
    
    // Display functions
    void displayBandwidth(const std::string& interface);
    bool getBandwidth(const std::string& interface, double& download_bps, double& upload_bps);
    void monitorBandwidthContinuous(const std::string& interface, int interval_seconds = 1,
                                    const std::string& log_file = "");
    
    // Latency measurement (to be implemented in Phase 2)
    LatencyResult measureLatency(const std::string& host, int timeout_ms = 1000);
    
    // Packet loss detection (to be implemented in Phase 3)
    PacketLossStats detectPacketLoss(const std::string& host, int count = 10);
    
    // Connection statistics (Phase 3)
    void displayActiveConnections();
    bool getConnectionStats(int& tcp_total, int& tcp_established, int& udp_total);
    
    // Data logging (Phase 4)
    void logToCSV(const std::string& filename);
    bool logBandwidthToCSV(const std::string& filename, const std::string& interface, 
                          double download_bps, double upload_bps);
    bool logLatencyToCSV(const std::string& filename, const LatencyResult& result);
    bool logPacketLossToCSV(const std::string& filename, const std::string& host, 
                           const PacketLossStats& stats);
    bool logConnectionsToCSV(const std::string& filename, int tcp_total, int tcp_established, 
                            int udp_total);

private:
    std::vector<std::string> available_interfaces_;
    std::map<std::string, InterfaceStats> last_stats_;
    
    // Helper functions
    bool parseProcNetDev(std::map<std::string, InterfaceStats>& stats);
    double calculateTimeDiff(const std::chrono::steady_clock::time_point& start,
                            const std::chrono::steady_clock::time_point& end);
    
    // ICMP helper functions for Phase 2
    unsigned short calculateChecksum(unsigned short* buffer, int length);
    bool resolveHostname(const std::string& hostname, struct sockaddr_in* addr);
    int createRawSocket();
};

#endif // NETWORK_MONITOR_H

