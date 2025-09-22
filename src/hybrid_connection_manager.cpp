#include "hybrid_connection_manager.hpp"
#include <algorithm>

namespace dht_crawler {

HybridConnectionManager::HybridConnectionManager(const HybridConfig& config) : config_(config) {
}

bool HybridConnectionManager::connectPeer(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::string connection_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // Check if already connected
    auto it = connections_.find(connection_id);
    if (it != connections_.end() && it->second.is_connected) {
        return true;
    }
    
    // Determine connection strategy
    ConnectionStrategy strategy = determineStrategy(peer_ip, peer_port);
    
    // Create connection info
    ConnectionInfo conn_info;
    conn_info.peer_ip = peer_ip;
    conn_info.peer_port = peer_port;
    conn_info.info_hash = info_hash;
    conn_info.strategy = strategy;
    conn_info.connection_type = getConnectionType(strategy);
    conn_info.is_connected = false;
    conn_info.created_at = std::chrono::steady_clock::now();
    conn_info.last_activity = conn_info.created_at;
    conn_info.performance_score = 0.0;
    
    // Attempt connection based on strategy
    bool success = false;
    if (strategy == ConnectionStrategy::DIRECT_FIRST) {
        success = attemptDirectConnection(conn_info);
        if (!success) {
            success = attemptLibtorrentConnection(conn_info);
        }
    } else if (strategy == ConnectionStrategy::LIBTORRENT_FIRST) {
        success = attemptLibtorrentConnection(conn_info);
        if (!success) {
            success = attemptDirectConnection(conn_info);
        }
    } else if (strategy == ConnectionStrategy::DIRECT_ONLY) {
        success = attemptDirectConnection(conn_info);
    } else if (strategy == ConnectionStrategy::LIBTORRENT_ONLY) {
        success = attemptLibtorrentConnection(conn_info);
    }
    
    conn_info.is_connected = success;
    connections_[connection_id] = conn_info;
    
    return success;
}

bool HybridConnectionManager::disconnectPeer(const std::string& peer_ip, int peer_port) {
    std::string connection_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        // Disconnect based on connection type
        if (it->second.connection_type == ConnectionType::DIRECT) {
            disconnectDirectConnection(it->second);
        } else if (it->second.connection_type == ConnectionType::LIBTORRENT) {
            disconnectLibtorrentConnection(it->second);
        }
        
        connections_.erase(it);
        return true;
    }
    
    return false;
}

HybridConnectionManager::ConnectionType HybridConnectionManager::getConnectionType(const std::string& peer_ip, int peer_port) {
    std::string connection_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second.connection_type;
    }
    
    return ConnectionType::UNKNOWN;
}

HybridConnectionManager::PerformanceMetrics HybridConnectionManager::getPerformanceMetrics(const std::string& peer_ip, int peer_port) {
    std::string connection_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second.performance_metrics;
    }
    
    return PerformanceMetrics{}; // Return empty metrics if not found
}

void HybridConnectionManager::setStrategy(const std::string& peer_ip, int peer_port, ConnectionStrategy strategy) {
    std::string connection_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        it->second.strategy = strategy;
    }
}

HybridConnectionManager::ConnectionStrategy HybridConnectionManager::determineStrategy(const std::string& peer_ip, int peer_port) {
    // Simple strategy determination based on config
    if (config_.prefer_direct_connections) {
        return ConnectionStrategy::DIRECT_FIRST;
    } else {
        return ConnectionStrategy::LIBTORRENT_FIRST;
    }
}

HybridConnectionManager::ConnectionType HybridConnectionManager::getConnectionType(ConnectionStrategy strategy) {
    switch (strategy) {
        case ConnectionStrategy::DIRECT_FIRST:
        case ConnectionStrategy::DIRECT_ONLY:
            return ConnectionType::DIRECT;
        case ConnectionStrategy::LIBTORRENT_FIRST:
        case ConnectionStrategy::LIBTORRENT_ONLY:
            return ConnectionType::LIBTORRENT;
        default:
            return ConnectionType::UNKNOWN;
    }
}

bool HybridConnectionManager::attemptDirectConnection(ConnectionInfo& conn_info) {
    // Simulate direct connection attempt
    // In a real implementation, this would use the DirectPeerConnector
    
    conn_info.connection_type = ConnectionType::DIRECT;
    conn_info.connection_attempts++;
    
    // Simulate success/failure based on some criteria
    bool success = (conn_info.connection_attempts <= config_.max_connection_attempts);
    
    if (success) {
        conn_info.connected_at = std::chrono::steady_clock::now();
        conn_info.performance_score = 0.8; // Direct connections get good score
    }
    
    return success;
}

bool HybridConnectionManager::attemptLibtorrentConnection(ConnectionInfo& conn_info) {
    // Simulate libtorrent connection attempt
    // In a real implementation, this would use libtorrent
    
    conn_info.connection_type = ConnectionType::LIBTORRENT;
    conn_info.connection_attempts++;
    
    // Simulate success/failure based on some criteria
    bool success = (conn_info.connection_attempts <= config_.max_connection_attempts);
    
    if (success) {
        conn_info.connected_at = std::chrono::steady_clock::now();
        conn_info.performance_score = 0.6; // Libtorrent connections get moderate score
    }
    
    return success;
}

void HybridConnectionManager::disconnectDirectConnection(ConnectionInfo& conn_info) {
    // Simulate direct connection disconnection
    conn_info.is_connected = false;
    conn_info.disconnected_at = std::chrono::steady_clock::now();
}

void HybridConnectionManager::disconnectLibtorrentConnection(ConnectionInfo& conn_info) {
    // Simulate libtorrent connection disconnection
    conn_info.is_connected = false;
    conn_info.disconnected_at = std::chrono::steady_clock::now();
}

std::vector<std::string> HybridConnectionManager::getConnectedPeers() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    std::vector<std::string> peers;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected) {
            peers.push_back(pair.first);
        }
    }
    
    return peers;
}

std::vector<std::string> HybridConnectionManager::getDirectConnections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    std::vector<std::string> peers;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected && pair.second.connection_type == ConnectionType::DIRECT) {
            peers.push_back(pair.first);
        }
    }
    
    return peers;
}

std::vector<std::string> HybridConnectionManager::getLibtorrentConnections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    std::vector<std::string> peers;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected && pair.second.connection_type == ConnectionType::LIBTORRENT) {
            peers.push_back(pair.first);
        }
    }
    
    return peers;
}

int HybridConnectionManager::getConnectionCount() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    int count = 0;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected) {
            count++;
        }
    }
    
    return count;
}

int HybridConnectionManager::getDirectConnectionCount() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    int count = 0;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected && pair.second.connection_type == ConnectionType::DIRECT) {
            count++;
        }
    }
    
    return count;
}

int HybridConnectionManager::getLibtorrentConnectionCount() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    int count = 0;
    for (const auto& pair : connections_) {
        if (pair.second.is_connected && pair.second.connection_type == ConnectionType::LIBTORRENT) {
            count++;
        }
    }
    
    return count;
}

HybridConnectionManager::ConnectionInfo HybridConnectionManager::getConnectionInfo(const std::string& peer_ip, int peer_port) {
    std::string connection_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second;
    }
    
    return ConnectionInfo{}; // Return empty info if not found
}

void HybridConnectionManager::cleanupInactiveConnections() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = connections_.begin(); it != connections_.end();) {
        if (now - it->second.last_activity > std::chrono::milliseconds(config_.connection_timeout)) {
            if (it->second.is_connected) {
                if (it->second.connection_type == ConnectionType::DIRECT) {
                    disconnectDirectConnection(it->second);
                } else if (it->second.connection_type == ConnectionType::LIBTORRENT) {
                    disconnectLibtorrentConnection(it->second);
                }
            }
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

void HybridConnectionManager::updateConfig(const HybridConfig& config) {
    config_ = config;
}

HybridConnectionManager::HybridConfig HybridConnectionManager::getConfig() const {
    return config_;
}

std::map<std::string, std::string> HybridConnectionManager::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["total_connections"] = std::to_string(connections_.size());
    status["active_connections"] = std::to_string(getConnectionCount());
    status["direct_connections"] = std::to_string(getDirectConnectionCount());
    status["libtorrent_connections"] = std::to_string(getLibtorrentConnectionCount());
    status["connection_timeout"] = std::to_string(config_.connection_timeout);
    
    return status;
}

} // namespace dht_crawler
