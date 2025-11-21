#include "network_monitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <cstring>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>

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

// Get bandwidth values for a specific interface (helper for logging)
bool NetworkMonitor::getBandwidth(const std::string& interface, double& download_bps, double& upload_bps) {
    InterfaceStats stats1, stats2;
    
    // First reading
    if (!readInterfaceStats(interface, stats1)) {
        return false;
    }
    
    // Wait 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Second reading
    if (!readInterfaceStats(interface, stats2)) {
        return false;
    }
    
    // Calculate bandwidth
    calculateBandwidth(stats1, stats2, download_bps, upload_bps);
    return true;
}

// Monitor bandwidth continuously
void NetworkMonitor::monitorBandwidthContinuous(const std::string& interface, int interval_seconds,
                                                const std::string& log_file) {
    InterfaceStats prev_stats, current_stats;
    bool log_enabled = !log_file.empty();
    bool log_notice_shown = false;
    
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
        
        // Log results to CSV if enabled
        if (log_enabled) {
            if (logBandwidthToCSV(log_file, interface, download_bps, upload_bps) && !log_notice_shown) {
                std::cout << "Logging continuous measurements to: " << log_file << std::endl;
                log_notice_shown = true;
            }
        }
        
        // Update previous stats
        prev_stats = current_stats;
    }
}

// ICMP Helper Functions for Phase 2

// Calculate ICMP checksum
unsigned short NetworkMonitor::calculateChecksum(unsigned short* buffer, int length) {
    unsigned long sum = 0;
    unsigned short* ptr = buffer;
    
    // Sum all 16-bit words
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    
    // Add any remaining byte
    if (length > 0) {
        sum += *(unsigned char*)ptr;
    }
    
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (unsigned short)(~sum);
}

// Resolve hostname to IP address
bool NetworkMonitor::resolveHostname(const std::string& hostname, struct sockaddr_in* addr) {
    struct hostent* host_entry;
    
    // Try to convert as IP address first
    if (inet_addr(hostname.c_str()) != INADDR_NONE) {
        addr->sin_addr.s_addr = inet_addr(hostname.c_str());
        addr->sin_family = AF_INET;
        return true;
    }
    
    // Try DNS resolution
    host_entry = gethostbyname(hostname.c_str());
    if (host_entry == nullptr) {
        return false;
    }
    
    addr->sin_family = AF_INET;
    addr->sin_addr = *(struct in_addr*)host_entry->h_addr_list[0];
    return true;
}

// Create raw socket for ICMP
int NetworkMonitor::createRawSocket() {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        return -1;
    }
    
    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    return sock;
}

// Phase 2: ICMP-based latency measurement
LatencyResult NetworkMonitor::measureLatency(const std::string& host, int timeout_ms) {
    LatencyResult result;
    result.host = host;
    result.success = false;
    result.rtt_ms = 0.0;
    
    // Resolve hostname to IP address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    
    if (!resolveHostname(host, &dest_addr)) {
        std::cerr << "Error: Could not resolve hostname: " << host << std::endl;
        return result;
    }
    
    // Create raw socket
    int sock = createRawSocket();
    if (sock < 0) {
        std::cerr << "Error: Could not create raw socket. Root privileges required." << std::endl;
        return result;
    }
    
    // Build ICMP packet
    struct icmphdr icmp_hdr;
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.code = 0;
    icmp_hdr.un.echo.id = getpid() & 0xFFFF;  // Use process ID as identifier
    icmp_hdr.un.echo.sequence = 1;
    icmp_hdr.checksum = 0;
    
    // Calculate checksum
    icmp_hdr.checksum = calculateChecksum((unsigned short*)&icmp_hdr, sizeof(icmp_hdr));
    
    // Record send time
    auto send_time = std::chrono::steady_clock::now();
    
    // Send ICMP echo request
    ssize_t sent = sendto(sock, &icmp_hdr, sizeof(icmp_hdr), 0,
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (sent < 0) {
        std::cerr << "Error: Failed to send ICMP packet: " << strerror(errno) << std::endl;
        close(sock);
        return result;
    }
    
    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // Receive ICMP echo reply
    char recv_buffer[1024];
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    
    ssize_t received = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0,
                                (struct sockaddr*)&recv_addr, &addr_len);
    
    // Record receive time
    auto recv_time = std::chrono::steady_clock::now();
    
    close(sock);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cerr << "Error: Timeout waiting for ICMP reply from " << host << std::endl;
        } else {
            std::cerr << "Error: Failed to receive ICMP reply: " << strerror(errno) << std::endl;
        }
        return result;
    }
    
    // Parse IP header to get to ICMP header
    struct iphdr* ip_hdr = (struct iphdr*)recv_buffer;
    int ip_header_len = ip_hdr->ihl * 4;
    
    if (received < static_cast<ssize_t>(ip_header_len + sizeof(struct icmphdr))) {
        std::cerr << "Error: Received packet too short" << std::endl;
        return result;
    }
    
    struct icmphdr* recv_icmp = (struct icmphdr*)(recv_buffer + ip_header_len);
    
    // Verify it's an echo reply and matches our request
    if (recv_icmp->type == ICMP_ECHOREPLY && 
        recv_icmp->un.echo.id == (getpid() & 0xFFFF)) {
        // Calculate RTT
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(recv_time - send_time);
        result.rtt_ms = duration.count() / 1000.0;
        result.success = true;
    } else {
        std::cerr << "Error: Received unexpected ICMP packet type: " << (int)recv_icmp->type << std::endl;
    }
    
    return result;
}

// Phase 3: Packet loss detection with jitter calculation
PacketLossStats NetworkMonitor::detectPacketLoss(const std::string& host, int count) {
    PacketLossStats stats = {0, 0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    // Resolve hostname to IP address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    
    if (!resolveHostname(host, &dest_addr)) {
        std::cerr << "Error: Could not resolve hostname: " << host << std::endl;
        return stats;
    }
    
    // Create raw socket
    int sock = createRawSocket();
    if (sock < 0) {
        std::cerr << "Error: Could not create raw socket. Root privileges required." << std::endl;
        return stats;
    }
    
    // Set receive timeout (1 second per packet)
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    std::vector<double> rtt_values;
    int pid = getpid() & 0xFFFF;
    stats.packets_sent = count;
    
    std::cout << "Pinging " << host << " with " << count << " packets..." << std::endl;
    
    // Send multiple ICMP packets and collect RTT values
    for (int seq = 1; seq <= count; seq++) {
        // Build ICMP packet
        struct icmphdr icmp_hdr;
        memset(&icmp_hdr, 0, sizeof(icmp_hdr));
        icmp_hdr.type = ICMP_ECHO;
        icmp_hdr.code = 0;
        icmp_hdr.un.echo.id = pid;
        icmp_hdr.un.echo.sequence = seq;
        icmp_hdr.checksum = 0;
        
        // Calculate checksum
        icmp_hdr.checksum = calculateChecksum((unsigned short*)&icmp_hdr, sizeof(icmp_hdr));
        
        // Record send time
        auto send_time = std::chrono::steady_clock::now();
        
        // Send ICMP echo request
        ssize_t sent = sendto(sock, &icmp_hdr, sizeof(icmp_hdr), 0,
                              (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        if (sent < 0) {
            std::cerr << "Warning: Failed to send packet " << seq << std::endl;
            continue;
        }
        
        // Try to receive reply
        char recv_buffer[1024];
        struct sockaddr_in recv_addr;
        socklen_t addr_len = sizeof(recv_addr);
        
        ssize_t received = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0,
                                    (struct sockaddr*)&recv_addr, &addr_len);
        
        // Record receive time
        auto recv_time = std::chrono::steady_clock::now();
        
        if (received > 0) {
            // Parse IP header
            struct iphdr* ip_hdr = (struct iphdr*)recv_buffer;
            int ip_header_len = ip_hdr->ihl * 4;
            
            if (received >= static_cast<ssize_t>(ip_header_len + sizeof(struct icmphdr))) {
                struct icmphdr* recv_icmp = (struct icmphdr*)(recv_buffer + ip_header_len);
                
                // Verify it's an echo reply and matches our request
                if (recv_icmp->type == ICMP_ECHOREPLY && 
                    recv_icmp->un.echo.id == pid &&
                    recv_icmp->un.echo.sequence == seq) {
                    // Calculate RTT
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(recv_time - send_time);
                    double rtt_ms = duration.count() / 1000.0;
                    rtt_values.push_back(rtt_ms);
                    stats.packets_received++;
                    
                    std::cout << "  Packet " << seq << ": " << std::fixed << std::setprecision(2) 
                              << rtt_ms << " ms" << std::endl;
                }
            }
        }
        
        // Small delay between packets
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    close(sock);
    
    // Calculate statistics
    if (stats.packets_received > 0) {
        // Calculate packet loss percentage
        stats.loss_percentage = ((count - stats.packets_received) * 100.0) / count;
        
        // Calculate min, max, and average RTT
        stats.min_rtt = *std::min_element(rtt_values.begin(), rtt_values.end());
        stats.max_rtt = *std::max_element(rtt_values.begin(), rtt_values.end());
        
        double sum = 0.0;
        for (double rtt : rtt_values) {
            sum += rtt;
        }
        stats.avg_rtt = sum / rtt_values.size();
        
        // Calculate jitter (standard deviation of RTT)
        if (rtt_values.size() > 1) {
            double variance = 0.0;
            for (double rtt : rtt_values) {
                double diff = rtt - stats.avg_rtt;
                variance += diff * diff;
            }
            variance /= rtt_values.size();
            stats.jitter = std::sqrt(variance);
        } else {
            stats.jitter = 0.0;
        }
    } else {
        stats.loss_percentage = 100.0;
    }
    
    return stats;
}

// Phase 3: Display active network connections
void NetworkMonitor::displayActiveConnections() {
    std::cout << "Active Network Connections" << std::endl;
    std::cout << "==========================" << std::endl << std::endl;
    
    int tcp_count = 0;
    int udp_count = 0;
    int tcp_established = 0;
    int udp_established = 0;
    
    // Parse TCP connections
    std::ifstream tcp_file("/proc/net/tcp");
    if (tcp_file.is_open()) {
        std::string line;
        std::getline(tcp_file, line); // Skip header
        
        while (std::getline(tcp_file, line)) {
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            std::string sl, local_address, rem_address, st, tx_queue, rx_queue, tr, tm_when, retrnsmt, uid, timeout, inode;
            
            iss >> sl >> local_address >> rem_address >> st >> tx_queue >> rx_queue 
                >> tr >> tm_when >> retrnsmt >> uid >> timeout >> inode;
            
            if (!st.empty()) {
                int state = std::stoi(st, nullptr, 16);
                tcp_count++;
                
                // State 01 = ESTABLISHED
                if (state == 0x01) {
                    tcp_established++;
                }
            }
        }
        tcp_file.close();
    }
    
    // Parse UDP connections
    std::ifstream udp_file("/proc/net/udp");
    if (udp_file.is_open()) {
        std::string line;
        std::getline(udp_file, line); // Skip header
        
        while (std::getline(udp_file, line)) {
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            std::string sl, local_address, rem_address, st, tx_queue, rx_queue, tr, tm_when, retrnsmt, uid, timeout, inode;
            
            iss >> sl >> local_address >> rem_address >> st >> tx_queue >> rx_queue 
                >> tr >> tm_when >> retrnsmt >> uid >> timeout >> inode;
            
            if (!st.empty()) {
                udp_count++;
                udp_established++;
            }
        }
        udp_file.close();
    }
    
    // Display statistics
    std::cout << "TCP Connections:" << std::endl;
    std::cout << "  Total: " << tcp_count << std::endl;
    std::cout << "  Established: " << tcp_established << std::endl;
    std::cout << std::endl;
    
    std::cout << "UDP Connections:" << std::endl;
    std::cout << "  Total: " << udp_count << std::endl;
    std::cout << std::endl;
    
    std::cout << "Total Active Connections: " << (tcp_count + udp_count) << std::endl;
}

// Get connection statistics (helper for logging)
bool NetworkMonitor::getConnectionStats(int& tcp_total, int& tcp_established, int& udp_total) {
    tcp_total = 0;
    tcp_established = 0;
    udp_total = 0;
    
    // Parse TCP connections
    std::ifstream tcp_file("/proc/net/tcp");
    if (tcp_file.is_open()) {
        std::string line;
        std::getline(tcp_file, line); // Skip header
        
        while (std::getline(tcp_file, line)) {
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            std::string sl, local_address, rem_address, st, tx_queue, rx_queue, tr, tm_when, retrnsmt, uid, timeout, inode;
            
            iss >> sl >> local_address >> rem_address >> st >> tx_queue >> rx_queue 
                >> tr >> tm_when >> retrnsmt >> uid >> timeout >> inode;
            
            if (!st.empty()) {
                int state = std::stoi(st, nullptr, 16);
                tcp_total++;
                
                if (state == 0x01) {
                    tcp_established++;
                }
            }
        }
        tcp_file.close();
    }
    
    // Parse UDP connections
    std::ifstream udp_file("/proc/net/udp");
    if (udp_file.is_open()) {
        std::string line;
        std::getline(udp_file, line); // Skip header
        
        while (std::getline(udp_file, line)) {
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            std::string sl, local_address, rem_address, st, tx_queue, rx_queue, tr, tm_when, retrnsmt, uid, timeout, inode;
            
            iss >> sl >> local_address >> rem_address >> st >> tx_queue >> rx_queue 
                >> tr >> tm_when >> retrnsmt >> uid >> timeout >> inode;
            
            if (!st.empty()) {
                udp_total++;
            }
        }
        udp_file.close();
    }
    
    return true;
}

// Phase 4: CSV Data Logging

// Get current timestamp as string
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Log bandwidth data to CSV
bool NetworkMonitor::logBandwidthToCSV(const std::string& filename, const std::string& interface,
                                       double download_bps, double upload_bps) {
    std::ofstream csv_file;
    
    // Check if file exists to determine if we need headers
    bool file_exists = std::ifstream(filename).good();
    
    csv_file.open(filename, std::ios::app);
    if (!csv_file.is_open()) {
        std::cerr << "Error: Could not open CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write header if new file
    if (!file_exists) {
        csv_file << "Timestamp,Interface,Download_bps,Upload_bps,Download_Mbps,Upload_Mbps\n";
    }
    
    // Write data
    csv_file << getCurrentTimestamp() << ","
             << interface << ","
             << std::fixed << std::setprecision(2)
             << download_bps << ","
             << upload_bps << ","
             << (download_bps / 1000000.0) << ","
             << (upload_bps / 1000000.0) << "\n";
    
    csv_file.close();
    return true;
}

// Log latency measurement to CSV
bool NetworkMonitor::logLatencyToCSV(const std::string& filename, const LatencyResult& result) {
    std::ofstream csv_file;
    
    bool file_exists = std::ifstream(filename).good();
    
    csv_file.open(filename, std::ios::app);
    if (!csv_file.is_open()) {
        std::cerr << "Error: Could not open CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write header if new file
    if (!file_exists) {
        csv_file << "Timestamp,Host,RTT_ms,Success\n";
    }
    
    // Write data
    csv_file << getCurrentTimestamp() << ","
             << result.host << ","
             << std::fixed << std::setprecision(2)
             << result.rtt_ms << ","
             << (result.success ? "Yes" : "No") << "\n";
    
    csv_file.close();
    return true;
}

// Log packet loss statistics to CSV
bool NetworkMonitor::logPacketLossToCSV(const std::string& filename, const std::string& host,
                                       const PacketLossStats& stats) {
    std::ofstream csv_file;
    
    bool file_exists = std::ifstream(filename).good();
    
    csv_file.open(filename, std::ios::app);
    if (!csv_file.is_open()) {
        std::cerr << "Error: Could not open CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write header if new file
    if (!file_exists) {
        csv_file << "Timestamp,Host,Packets_Sent,Packets_Received,Loss_Percentage,"
                 << "Min_RTT_ms,Max_RTT_ms,Avg_RTT_ms,Jitter_ms\n";
    }
    
    // Write data
    csv_file << getCurrentTimestamp() << ","
             << host << ","
             << stats.packets_sent << ","
             << stats.packets_received << ","
             << std::fixed << std::setprecision(2)
             << stats.loss_percentage << ","
             << stats.min_rtt << ","
             << stats.max_rtt << ","
             << stats.avg_rtt << ","
             << stats.jitter << "\n";
    
    csv_file.close();
    return true;
}

// Log connection statistics to CSV
bool NetworkMonitor::logConnectionsToCSV(const std::string& filename, int tcp_total, 
                                         int tcp_established, int udp_total) {
    std::ofstream csv_file;
    
    bool file_exists = std::ifstream(filename).good();
    
    csv_file.open(filename, std::ios::app);
    if (!csv_file.is_open()) {
        std::cerr << "Error: Could not open CSV file: " << filename << std::endl;
        return false;
    }
    
    // Write header if new file
    if (!file_exists) {
        csv_file << "Timestamp,TCP_Total,TCP_Established,UDP_Total,Total_Connections\n";
    }
    
    // Write data
    csv_file << getCurrentTimestamp() << ","
             << tcp_total << ","
             << tcp_established << ","
             << udp_total << ","
             << (tcp_total + udp_total) << "\n";
    
    csv_file.close();
    return true;
}

// General CSV logging function (logs current bandwidth for default interface)
void NetworkMonitor::logToCSV(const std::string& filename) {
    if (available_interfaces_.empty()) {
        std::cerr << "Error: No interfaces available for logging" << std::endl;
        return;
    }
    
    // Use first available interface
    std::string interface = available_interfaces_[0];
    InterfaceStats stats1, stats2;
    
    if (!readInterfaceStats(interface, stats1)) {
        std::cerr << "Error: Could not read interface stats" << std::endl;
        return;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    if (!readInterfaceStats(interface, stats2)) {
        std::cerr << "Error: Could not read interface stats" << std::endl;
        return;
    }
    
    double download_bps, upload_bps;
    calculateBandwidth(stats1, stats2, download_bps, upload_bps);
    
    if (logBandwidthToCSV(filename, interface, download_bps, upload_bps)) {
        std::cout << "Bandwidth data logged to: " << filename << std::endl;
    }
}

