#include "direct_peer_connector.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace dht_crawler {

DirectPeerConnector::DirectPeerConnector(const ConnectionConfig& config) : config_(config) {
}

DirectPeerConnector::~DirectPeerConnector() {
    disconnectAll();
}

bool DirectPeerConnector::connect(const std::string& ip_address, int port) {
    std::string connection_id = ip_address + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // Check if already connected
    auto it = connections_.find(connection_id);
    if (it != connections_.end() && it->second.is_connected) {
        return true;
    }
    
    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd);
        return false;
    }
    
    // Set non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sockfd);
        return false;
    }
    
    // Set up address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip_address.c_str(), &addr.sin_addr) <= 0) {
        close(sockfd);
        return false;
    }
    
    // Connect
    int result = ::connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0 && errno != EINPROGRESS) {
        close(sockfd);
        return false;
    }
    
    // Create connection info
    ConnectionInfo conn_info;
    conn_info.socket_fd = sockfd;
    conn_info.ip_address = ip_address;
    conn_info.port = port;
    conn_info.is_connected = (result == 0);
    conn_info.created_at = std::chrono::steady_clock::now();
    conn_info.last_activity = conn_info.created_at;
    
    connections_[connection_id] = conn_info;
    
    return true;
}

bool DirectPeerConnector::disconnect(const std::string& ip_address, int port) {
    std::string connection_id = ip_address + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        if (it->second.socket_fd >= 0) {
            close(it->second.socket_fd);
        }
        connections_.erase(it);
        return true;
    }
    
    return false;
}

void DirectPeerConnector::disconnectAll() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    for (auto& pair : connections_) {
        if (pair.second.socket_fd >= 0) {
            close(pair.second.socket_fd);
        }
    }
    
    connections_.clear();
}

bool DirectPeerConnector::sendData(const std::string& ip_address, int port, const std::vector<uint8_t>& data) {
    std::string connection_id = ip_address + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it == connections_.end() || !it->second.is_connected) {
        return false;
    }
    
    int sockfd = it->second.socket_fd;
    ssize_t bytes_sent = send(sockfd, data.data(), data.size(), 0);
    
    if (bytes_sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, but connection is still valid
            return false;
        } else {
            // Connection error
            it->second.is_connected = false;
            return false;
        }
    }
    
    it->second.last_activity = std::chrono::steady_clock::now();
    return bytes_sent == static_cast<ssize_t>(data.size());
}

std::vector<uint8_t> DirectPeerConnector::receiveData(const std::string& ip_address, int port, size_t max_size) {
    std::string connection_id = ip_address + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it == connections_.end() || !it->second.is_connected) {
        return {};
    }
    
    int sockfd = it->second.socket_fd;
    std::vector<uint8_t> buffer(max_size);
    
    ssize_t bytes_received = recv(sockfd, buffer.data(), max_size, 0);
    
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available
            return {};
        } else {
            // Connection error
            it->second.is_connected = false;
            return {};
        }
    }
    
    if (bytes_received == 0) {
        // Connection closed
        it->second.is_connected = false;
        return {};
    }
    
    buffer.resize(bytes_received);
    it->second.last_activity = std::chrono::steady_clock::now();
    
    return buffer;
}

bool DirectPeerConnector::isConnected(const std::string& ip_address, int port) {
    std::string connection_id = ip_address + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    return it != connections_.end() && it->second.is_connected;
}

std::vector<std::string> DirectPeerConnector::getConnectedPeers() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    std::vector<std::string> peers;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected) {
            peers.push_back(pair.first);
        }
    }
    
    return peers;
}

int DirectPeerConnector::getConnectionCount() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    int count = 0;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected) {
            count++;
        }
    }
    
    return count;
}

DirectPeerConnector::ConnectionInfo DirectPeerConnector::getConnectionInfo(const std::string& ip_address, int port) {
    std::string connection_id = ip_address + ":" + std::to_string(port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second;
    }
    
    return ConnectionInfo{}; // Return empty info if not found
}

void DirectPeerConnector::cleanupInactiveConnections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = connections_.begin(); it != connections_.end();) {
        if (now - it->second.last_activity > std::chrono::milliseconds(config_.connection_timeout)) {
            if (it->second.socket_fd >= 0) {
                close(it->second.socket_fd);
            }
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

void DirectPeerConnector::updateConfig(const ConnectionConfig& config) {
    config_ = config;
}

DirectPeerConnector::ConnectionConfig DirectPeerConnector::getConfig() const {
    return config_;
}

std::map<std::string, std::string> DirectPeerConnector::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["total_connections"] = std::to_string(connections_.size());
    status["active_connections"] = std::to_string(getConnectionCount());
    status["connection_timeout"] = std::to_string(config_.connection_timeout);
    
    return status;
}

} // namespace dht_crawler
