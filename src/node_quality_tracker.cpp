#include "node_quality_tracker.hpp"
#include <algorithm>
#include <cmath>

namespace dht_crawler {

NodeQualityTracker::NodeQualityTracker(const QualityConfig& config) : config_(config) {
}

void NodeQualityTracker::updateNodeStatus(const std::string& node_id, NodeStatus status) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = node_qualities_.find(node_id);
    if (it != node_qualities_.end()) {
        it->second.status = status;
        it->second.last_updated = std::chrono::steady_clock::now();
    } else {
        NodeQuality quality;
        quality.node_id = node_id;
        quality.status = status;
        quality.quality_score = getInitialQualityScore(status);
        quality.response_times.clear();
        quality.successful_requests = 0;
        quality.failed_requests = 0;
        quality.last_updated = std::chrono::steady_clock::now();
        quality.created_at = std::chrono::steady_clock::now();
        
        node_qualities_[node_id] = quality;
    }
}

NodeQualityTracker::NodeStatus NodeQualityTracker::getNodeStatus(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = node_qualities_.find(node_id);
    if (it != node_qualities_.end()) {
        return it->second.status;
    }
    
    return NodeStatus::UNKNOWN;
}

void NodeQualityTracker::updateResponseTime(const std::string& node_id, std::chrono::milliseconds response_time) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = node_qualities_.find(node_id);
    if (it != node_qualities_.end()) {
        NodeQuality& quality = it->second;
        
        // Add response time to history
        quality.response_times.push_back(response_time);
        
        // Keep only recent response times
        if (quality.response_times.size() > config_.max_response_history) {
            quality.response_times.erase(quality.response_times.begin());
        }
        
        // Update quality score based on response time
        updateQualityScore(quality);
        
        quality.last_updated = std::chrono::steady_clock::now();
    }
}

void NodeQualityTracker::blacklistNode(const std::string& node_id, const std::string& reason) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = node_qualities_.find(node_id);
    if (it != node_qualities_.end()) {
        it->second.status = NodeStatus::BAD;
        it->second.blacklist_reason = reason;
        it->second.blacklisted_at = std::chrono::steady_clock::now();
        it->second.quality_score = 0.0;
    } else {
        NodeQuality quality;
        quality.node_id = node_id;
        quality.status = NodeStatus::BAD;
        quality.quality_score = 0.0;
        quality.blacklist_reason = reason;
        quality.blacklisted_at = std::chrono::steady_clock::now();
        quality.created_at = std::chrono::steady_clock::now();
        quality.last_updated = std::chrono::steady_clock::now();
        
        node_qualities_[node_id] = quality;
    }
}

double NodeQualityTracker::getQualityScore(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = node_qualities_.find(node_id);
    if (it != node_qualities_.end()) {
        return it->second.quality_score;
    }
    
    return 0.0;
}

NodeQualityTracker::NodeQuality NodeQualityTracker::getNodeQuality(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = node_qualities_.find(node_id);
    if (it != node_qualities_.end()) {
        return it->second;
    }
    
    return NodeQuality{}; // Return empty quality if not found
}

std::vector<std::string> NodeQualityTracker::getNodesByStatus(NodeStatus status) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<std::string> nodes;
    for (const auto& pair : node_qualities_) {
        if (pair.second.status == status) {
            nodes.push_back(pair.first);
        }
    }
    
    return nodes;
}

std::vector<std::string> NodeQualityTracker::getGoodNodes() {
    return getNodesByStatus(NodeStatus::GOOD);
}

std::vector<std::string> NodeQualityTracker::getBadNodes() {
    return getNodesByStatus(NodeStatus::BAD);
}

std::vector<std::string> NodeQualityTracker::getUnknownNodes() {
    return getNodesByStatus(NodeStatus::UNKNOWN);
}

std::vector<std::string> NodeQualityTracker::getBlacklistedNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<std::string> nodes;
    for (const auto& pair : node_qualities_) {
        if (!pair.second.blacklist_reason.empty()) {
            nodes.push_back(pair.first);
        }
    }
    
    return nodes;
}

int NodeQualityTracker::getNodeCount() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    return static_cast<int>(node_qualities_.size());
}

int NodeQualityTracker::getNodeCountByStatus(NodeStatus status) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    int count = 0;
    for (const auto& pair : node_qualities_) {
        if (pair.second.status == status) {
            count++;
        }
    }
    
    return count;
}

int NodeQualityTracker::getGoodNodeCount() {
    return getNodeCountByStatus(NodeStatus::GOOD);
}

int NodeQualityTracker::getBadNodeCount() {
    return getNodeCountByStatus(NodeStatus::BAD);
}

int NodeQualityTracker::getUnknownNodeCount() {
    return getNodeCountByStatus(NodeStatus::UNKNOWN);
}

int NodeQualityTracker::getBlacklistedNodeCount() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    int count = 0;
    for (const auto& pair : node_qualities_) {
        if (!pair.second.blacklist_reason.empty()) {
            count++;
        }
    }
    
    return count;
}

double NodeQualityTracker::getAverageQualityScore() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    if (node_qualities_.empty()) {
        return 0.0;
    }
    
    double total_score = 0.0;
    for (const auto& pair : node_qualities_) {
        total_score += pair.second.quality_score;
    }
    
    return total_score / node_qualities_.size();
}

double NodeQualityTracker::getAverageResponseTime(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto it = node_qualities_.find(node_id);
    if (it != node_qualities_.end() && !it->second.response_times.empty()) {
        double total_time = 0.0;
        for (const auto& time : it->second.response_times) {
            total_time += time.count();
        }
        return total_time / it->second.response_times.size();
    }
    
    return 0.0;
}

double NodeQualityTracker::getInitialQualityScore(NodeStatus status) {
    switch (status) {
        case NodeStatus::GOOD:
            return 0.8;
        case NodeStatus::UNKNOWN:
            return 0.5;
        case NodeStatus::BAD:
            return 0.2;
        default:
            return 0.5;
    }
}

void NodeQualityTracker::updateQualityScore(NodeQuality& quality) {
    double score = 0.0;
    
    // Base score from status
    score += getInitialQualityScore(quality.status) * 0.3;
    
    // Response time score
    if (!quality.response_times.empty()) {
        double avg_response_time = 0.0;
        for (const auto& time : quality.response_times) {
            avg_response_time += time.count();
        }
        avg_response_time /= quality.response_times.size();
        
        // Convert response time to score (lower is better)
        double response_score = std::max(0.0, 1.0 - (avg_response_time / config_.max_response_time));
        score += response_score * 0.3;
    }
    
    // Success rate score
    int total_requests = quality.successful_requests + quality.failed_requests;
    if (total_requests > 0) {
        double success_rate = static_cast<double>(quality.successful_requests) / total_requests;
        score += success_rate * 0.4;
    }
    
    quality.quality_score = std::min(score, 1.0);
}

void NodeQualityTracker::clearExpiredNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = node_qualities_.begin(); it != node_qualities_.end();) {
        if (now - it->second.last_updated > std::chrono::milliseconds(config_.node_timeout)) {
            it = node_qualities_.erase(it);
        } else {
            ++it;
        }
    }
}

void NodeQualityTracker::clearAllNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    node_qualities_.clear();
}

void NodeQualityTracker::updateConfig(const QualityConfig& config) {
    config_ = config;
}

NodeQualityTracker::QualityConfig NodeQualityTracker::getConfig() const {
    return config_;
}

std::map<std::string, std::string> NodeQualityTracker::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["total_nodes"] = std::to_string(getNodeCount());
    status["good_nodes"] = std::to_string(getGoodNodeCount());
    status["bad_nodes"] = std::to_string(getBadNodeCount());
    status["unknown_nodes"] = std::to_string(getUnknownNodeCount());
    status["blacklisted_nodes"] = std::to_string(getBlacklistedNodeCount());
    status["average_quality_score"] = std::to_string(getAverageQualityScore());
    
    return status;
}

} // namespace dht_crawler
