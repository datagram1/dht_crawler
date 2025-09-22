#include "bittorrent_protocol.hpp"
#include <cstring>
#include <random>

namespace dht_crawler {

BitTorrentProtocol::BitTorrentProtocol(const ProtocolConfig& config) : config_(config) {
    // Initialize random number generator
    std::random_device rd;
    rng_.seed(rd());
}

bool BitTorrentProtocol::performHandshake(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(handshakes_mutex_);
    
    std::string handshake_id = peer_ip + ":" + std::to_string(peer_port);
    
    // Create handshake message
    HandshakeMessage handshake;
    handshake.protocol_length = 19;
    handshake.protocol = "BitTorrent protocol";
    handshake.reserved = {0, 0, 0, 0, 0, 0, 0, 0};
    handshake.info_hash = info_hash;
    handshake.peer_id = generatePeerId();
    
    // Store handshake info
    HandshakeInfo handshake_info;
    handshake_info.peer_ip = peer_ip;
    handshake_info.peer_port = peer_port;
    handshake_info.info_hash = info_hash;
    handshake_info.peer_id = handshake.peer_id;
    handshake_info.status = HandshakeStatus::INITIATED;
    handshake_info.created_at = std::chrono::steady_clock::now();
    
    active_handshakes_[handshake_id] = handshake_info;
    
    return true;
}

BitTorrentProtocol::Message BitTorrentProtocol::parseMessage(const std::vector<uint8_t>& data) {
    Message message;
    
    if (data.size() < 4) {
        message.type = MessageType::INVALID;
        return message;
    }
    
    // Parse message length
    message.length = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    
    if (message.length == 0) {
        message.type = MessageType::KEEP_ALIVE;
        return message;
    }
    
    if (data.size() < 4 + message.length) {
        message.type = MessageType::INVALID;
        return message;
    }
    
    // Parse message type
    if (message.length > 0) {
        message.type = static_cast<MessageType>(data[4]);
    }
    
    // Parse payload
    if (message.length > 1) {
        message.payload.assign(data.begin() + 5, data.begin() + 4 + message.length);
    }
    
    return message;
}

bool BitTorrentProtocol::sendMessage(const std::string& peer_ip, int peer_port, const Message& message) {
    std::vector<uint8_t> data;
    
    // Add message length
    data.push_back((message.length >> 24) & 0xFF);
    data.push_back((message.length >> 16) & 0xFF);
    data.push_back((message.length >> 8) & 0xFF);
    data.push_back(message.length & 0xFF);
    
    // Add message type
    if (message.length > 0) {
        data.push_back(static_cast<uint8_t>(message.type));
    }
    
    // Add payload
    data.insert(data.end(), message.payload.begin(), message.payload.end());
    
    // Send data (this would typically use a connection manager)
    return true; // Simplified for now
}

bool BitTorrentProtocol::enableExtension(const std::string& peer_ip, int peer_port, const std::string& extension_name) {
    std::lock_guard<std::mutex> lock(extensions_mutex_);
    
    std::string peer_id = peer_ip + ":" + std::to_string(peer_port);
    
    auto it = peer_extensions_.find(peer_id);
    if (it != peer_extensions_.end()) {
        it->second.enabled_extensions.insert(extension_name);
        return true;
    }
    
    // Create new extension info
    ExtensionInfo ext_info;
    ext_info.peer_ip = peer_ip;
    ext_info.peer_port = peer_port;
    ext_info.enabled_extensions.insert(extension_name);
    ext_info.created_at = std::chrono::steady_clock::now();
    
    peer_extensions_[peer_id] = ext_info;
    
    return true;
}

bool BitTorrentProtocol::sendExtensionMessage(const std::string& peer_ip, int peer_port, 
                                             const std::string& extension_name, 
                                             const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(extensions_mutex_);
    
    std::string peer_id = peer_ip + ":" + std::to_string(peer_port);
    
    auto it = peer_extensions_.find(peer_id);
    if (it == peer_extensions_.end()) {
        return false;
    }
    
    // Check if extension is enabled
    if (it->second.enabled_extensions.find(extension_name) == it->second.enabled_extensions.end()) {
        return false;
    }
    
    // Create extension message
    Message message;
    message.type = MessageType::EXTENDED;
    message.length = 2 + payload.size(); // Extension ID + payload
    message.payload.push_back(0); // Extension ID (simplified)
    message.payload.insert(message.payload.end(), payload.begin(), payload.end());
    
    return sendMessage(peer_ip, peer_port, message);
}

std::string BitTorrentProtocol::generatePeerId() {
    std::string peer_id = "-DC0001-"; // DHT Crawler identifier
    
    // Add random characters
    std::uniform_int_distribution<char> dist('0', '9');
    for (int i = 0; i < 12; i++) {
        peer_id += dist(rng_);
    }
    
    return peer_id;
}

BitTorrentProtocol::HandshakeInfo BitTorrentProtocol::getHandshakeInfo(const std::string& peer_ip, int peer_port) {
    std::string handshake_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(handshakes_mutex_);
    
    auto it = active_handshakes_.find(handshake_id);
    if (it != active_handshakes_.end()) {
        return it->second;
    }
    
    return HandshakeInfo{}; // Return empty info if not found
}

std::vector<std::string> BitTorrentProtocol::getActiveHandshakes() {
    std::lock_guard<std::mutex> lock(handshakes_mutex_);
    
    std::vector<std::string> handshakes;
    for (const auto& pair : active_handshakes_) {
        handshakes.push_back(pair.first);
    }
    
    return handshakes;
}

std::vector<std::string> BitTorrentProtocol::getEnabledExtensions(const std::string& peer_ip, int peer_port) {
    std::string peer_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(extensions_mutex_);
    
    auto it = peer_extensions_.find(peer_id);
    if (it != peer_extensions_.end()) {
        return std::vector<std::string>(it->second.enabled_extensions.begin(), 
                                       it->second.enabled_extensions.end());
    }
    
    return {};
}

bool BitTorrentProtocol::isExtensionEnabled(const std::string& peer_ip, int peer_port, const std::string& extension_name) {
    std::string peer_id = peer_ip + ":" + std::to_string(peer_port);
    
    std::lock_guard<std::mutex> lock(extensions_mutex_);
    
    auto it = peer_extensions_.find(peer_id);
    if (it != peer_extensions_.end()) {
        return it->second.enabled_extensions.find(extension_name) != it->second.enabled_extensions.end();
    }
    
    return false;
}

int BitTorrentProtocol::getActiveHandshakeCount() {
    std::lock_guard<std::mutex> lock(handshakes_mutex_);
    return static_cast<int>(active_handshakes_.size());
}

int BitTorrentProtocol::getExtensionCount() {
    std::lock_guard<std::mutex> lock(extensions_mutex_);
    return static_cast<int>(peer_extensions_.size());
}

void BitTorrentProtocol::cleanupExpiredHandshakes() {
    std::lock_guard<std::mutex> lock(handshakes_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = active_handshakes_.begin(); it != active_handshakes_.end();) {
        if (now - it->second.created_at > std::chrono::milliseconds(config_.handshake_timeout)) {
            it = active_handshakes_.erase(it);
        } else {
            ++it;
        }
    }
}

void BitTorrentProtocol::cleanupExpiredExtensions() {
    std::lock_guard<std::mutex> lock(extensions_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = peer_extensions_.begin(); it != peer_extensions_.end();) {
        if (now - it->second.created_at > std::chrono::milliseconds(config_.extension_timeout)) {
            it = peer_extensions_.erase(it);
        } else {
            ++it;
        }
    }
}

void BitTorrentProtocol::updateConfig(const ProtocolConfig& config) {
    config_ = config;
}

BitTorrentProtocol::ProtocolConfig BitTorrentProtocol::getConfig() const {
    return config_;
}

std::map<std::string, std::string> BitTorrentProtocol::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["active_handshakes"] = std::to_string(getActiveHandshakeCount());
    status["extensions"] = std::to_string(getExtensionCount());
    status["handshake_timeout"] = std::to_string(config_.handshake_timeout);
    status["extension_timeout"] = std::to_string(config_.extension_timeout);
    
    return status;
}

} // namespace dht_crawler
