#include "ut_metadata_protocol.hpp"
#include <algorithm>
#include <cstring>

namespace dht_crawler {

UTMetadataProtocol::UTMetadataProtocol(const MetadataConfig& config) : config_(config) {
}

bool UTMetadataProtocol::requestMetadata(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    std::string request_id = peer_ip + ":" + std::to_string(peer_port) + ":" + info_hash;
    
    // Check if already requesting
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        return false; // Already requesting
    }
    
    // Create metadata request
    MetadataRequest request;
    request.peer_ip = peer_ip;
    request.peer_port = peer_port;
    request.info_hash = info_hash;
    request.status = RequestStatus::REQUESTING;
    request.created_at = std::chrono::steady_clock::now();
    request.expires_at = request.created_at + std::chrono::milliseconds(config_.request_timeout);
    request.piece_count = 0;
    request.received_pieces = 0;
    request.pieces.clear();
    
    active_requests_[request_id] = request;
    
    return true;
}

bool UTMetadataProtocol::processPiece(const std::string& peer_ip, int peer_port, 
                                     const std::string& info_hash, int piece_index, 
                                     const std::vector<uint8_t>& piece_data) {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    std::string request_id = peer_ip + ":" + std::to_string(peer_port) + ":" + info_hash;
    
    auto it = active_requests_.find(request_id);
    if (it == active_requests_.end()) {
        return false;
    }
    
    MetadataRequest& request = it->second;
    
    // Check if piece index is valid
    if (piece_index < 0 || piece_index >= request.piece_count) {
        return false;
    }
    
    // Check if piece already received
    if (request.pieces.find(piece_index) != request.pieces.end()) {
        return false; // Duplicate piece
    }
    
    // Store piece data
    request.pieces[piece_index] = piece_data;
    request.received_pieces++;
    
    // Check if all pieces received
    if (request.received_pieces >= request.piece_count) {
        request.status = RequestStatus::COMPLETED;
        
        // Move to completed requests
        completed_requests_[request_id] = request;
        active_requests_.erase(it);
    }
    
    return true;
}

std::vector<uint8_t> UTMetadataProtocol::assembleMetadata(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::string request_id = peer_ip + ":" + std::to_string(peer_port) + ":" + info_hash;
    
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    auto it = completed_requests_.find(request_id);
    if (it == completed_requests_.end()) {
        return {};
    }
    
    const MetadataRequest& request = it->second;
    
    // Assemble pieces in order
    std::vector<uint8_t> metadata;
    for (int i = 0; i < request.piece_count; i++) {
        auto piece_it = request.pieces.find(i);
        if (piece_it != request.pieces.end()) {
            metadata.insert(metadata.end(), piece_it->second.begin(), piece_it->second.end());
        }
    }
    
    return metadata;
}

bool UTMetadataProtocol::isComplete(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::string request_id = peer_ip + ":" + std::to_string(peer_port) + ":" + info_hash;
    
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    auto it = completed_requests_.find(request_id);
    return it != completed_requests_.end();
}

std::vector<uint8_t> UTMetadataProtocol::getMetadata(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    return assembleMetadata(peer_ip, peer_port, info_hash);
}

UTMetadataProtocol::RequestStatus UTMetadataProtocol::getRequestStatus(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::string request_id = peer_ip + ":" + std::to_string(peer_port) + ":" + info_hash;
    
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        return it->second.status;
    }
    
    auto completed_it = completed_requests_.find(request_id);
    if (completed_it != completed_requests_.end()) {
        return completed_it->second.status;
    }
    
    return RequestStatus::NOT_FOUND;
}

UTMetadataProtocol::MetadataRequest UTMetadataProtocol::getRequestInfo(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::string request_id = peer_ip + ":" + std::to_string(peer_port) + ":" + info_hash;
    
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        return it->second;
    }
    
    auto completed_it = completed_requests_.find(request_id);
    if (completed_it != completed_requests_.end()) {
        return completed_it->second;
    }
    
    return MetadataRequest{}; // Return empty request if not found
}

std::vector<std::string> UTMetadataProtocol::getActiveRequests() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    std::vector<std::string> requests;
    for (const auto& pair : active_requests_) {
        requests.push_back(pair.first);
    }
    
    return requests;
}

std::vector<std::string> UTMetadataProtocol::getCompletedRequests() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    std::vector<std::string> requests;
    for (const auto& pair : completed_requests_) {
        requests.push_back(pair.first);
    }
    
    return requests;
}

int UTMetadataProtocol::getActiveRequestCount() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    return static_cast<int>(active_requests_.size());
}

int UTMetadataProtocol::getCompletedRequestCount() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    return static_cast<int>(completed_requests_.size());
}

int UTMetadataProtocol::getTotalRequestCount() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    return static_cast<int>(active_requests_.size() + completed_requests_.size());
}

double UTMetadataProtocol::getCompletionRatio(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::string request_id = peer_ip + ":" + std::to_string(peer_port) + ":" + info_hash;
    
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        if (it->second.piece_count > 0) {
            return static_cast<double>(it->second.received_pieces) / it->second.piece_count;
        }
    }
    
    auto completed_it = completed_requests_.find(request_id);
    if (completed_it != completed_requests_.end()) {
        return 1.0; // Completed
    }
    
    return 0.0;
}

void UTMetadataProtocol::cleanupExpiredRequests() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = active_requests_.begin(); it != active_requests_.end();) {
        if (now > it->second.expires_at) {
            // Move to expired requests
            it->second.status = RequestStatus::EXPIRED;
            expired_requests_[it->first] = it->second;
            it = active_requests_.erase(it);
        } else {
            ++it;
        }
    }
}

void UTMetadataProtocol::clearExpiredRequests() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    expired_requests_.clear();
}

void UTMetadataProtocol::clearAllRequests() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    active_requests_.clear();
    completed_requests_.clear();
    expired_requests_.clear();
}

void UTMetadataProtocol::updateConfig(const MetadataConfig& config) {
    config_ = config;
}

UTMetadataProtocol::MetadataConfig UTMetadataProtocol::getConfig() const {
    return config_;
}

std::map<std::string, std::string> UTMetadataProtocol::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["active_requests"] = std::to_string(getActiveRequestCount());
    status["completed_requests"] = std::to_string(getCompletedRequestCount());
    status["total_requests"] = std::to_string(getTotalRequestCount());
    status["request_timeout"] = std::to_string(config_.request_timeout);
    
    return status;
}

} // namespace dht_crawler
