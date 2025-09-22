#include "enhanced_bootstrap.hpp"
#include <algorithm>
#include <random>

namespace dht_crawler {

EnhancedBootstrap::EnhancedBootstrap(const BootstrapConfig& config) : config_(config) {
    // Initialize random number generator
    std::random_device rd;
    rng_.seed(rd());
    
    // Add default bootstrap nodes
    addDefaultBootstrapNodes();
}

void EnhancedBootstrap::addDefaultBootstrapNodes() {
    // Add well-known DHT bootstrap nodes
    addBootstrapNode("router.bittorrent.com", 6881);
    addBootstrapNode("dht.transmissionbt.com", 6881);
    addBootstrapNode("router.utorrent.com", 6881);
    addBootstrapNode("dht.aelitis.com", 6881);
    addBootstrapNode("router.bitcomet.com", 6881);
}

bool EnhancedBootstrap::bootstrapDHT() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    
    if (bootstrap_nodes_.empty()) {
        return false;
    }
    
    // Reset bootstrap state
    bootstrap_state_ = BootstrapState::IN_PROGRESS;
    bootstrap_attempts_ = 0;
    successful_bootstrap_nodes_.clear();
    failed_bootstrap_nodes_.clear();
    
    // Try to bootstrap with available nodes
    for (const auto& node : bootstrap_nodes_) {
        if (bootstrap_attempts_ >= config_.max_bootstrap_attempts) {
            break;
        }
        
        if (attemptBootstrap(node)) {
            successful_bootstrap_nodes_.push_back(node);
            bootstrap_attempts_++;
        } else {
            failed_bootstrap_nodes_.push_back(node);
            bootstrap_attempts_++;
        }
    }
    
    // Determine bootstrap success
    if (successful_bootstrap_nodes_.size() >= config_.min_successful_nodes) {
        bootstrap_state_ = BootstrapState::SUCCESS;
        return true;
    } else {
        bootstrap_state_ = BootstrapState::FAILED;
        return false;
    }
}

bool EnhancedBootstrap::addBootstrapNode(const std::string& ip_address, int port) {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    
    BootstrapNode node;
    node.ip_address = ip_address;
    node.port = port;
    node.node_id = generateNodeId();
    node.status = NodeStatus::UNKNOWN;
    node.last_attempt = std::chrono::steady_clock::time_point{};
    node.success_count = 0;
    node.failure_count = 0;
    node.response_time = std::chrono::milliseconds(0);
    
    bootstrap_nodes_.push_back(node);
    return true;
}

bool EnhancedBootstrap::removeBootstrapNode(const std::string& ip_address, int port) {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    
    auto it = std::find_if(bootstrap_nodes_.begin(), bootstrap_nodes_.end(),
                          [&](const BootstrapNode& node) {
                              return node.ip_address == ip_address && node.port == port;
                          });
    
    if (it != bootstrap_nodes_.end()) {
        bootstrap_nodes_.erase(it);
        return true;
    }
    
    return false;
}

bool EnhancedBootstrap::validateBootstrap() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    
    if (bootstrap_state_ != BootstrapState::SUCCESS) {
        return false;
    }
    
    // Check if we have enough successful nodes
    if (successful_bootstrap_nodes_.size() < config_.min_successful_nodes) {
        return false;
    }
    
    // Check if bootstrap is not too old
    auto now = std::chrono::steady_clock::now();
    if (now - last_bootstrap_time_ > std::chrono::milliseconds(config_.bootstrap_timeout)) {
        bootstrap_state_ = BootstrapState::EXPIRED;
        return false;
    }
    
    return true;
}

std::string EnhancedBootstrap::generateNodeId() {
    std::string node_id;
    node_id.resize(20); // DHT node ID is 20 bytes
    
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (char& c : node_id) {
        c = static_cast<char>(dist(rng_));
    }
    
    return node_id;
}

bool EnhancedBootstrap::attemptBootstrap(const BootstrapNode& node) {
    // Simulate bootstrap attempt
    // In a real implementation, this would send DHT bootstrap messages
    
    // Simulate success/failure based on some criteria
    bool success = (node.failure_count < config_.max_node_failures);
    
    if (success) {
        // Update node success count
        const_cast<BootstrapNode&>(node).success_count++;
        const_cast<BootstrapNode&>(node).status = NodeStatus::GOOD;
        const_cast<BootstrapNode&>(node).last_attempt = std::chrono::steady_clock::now();
        const_cast<BootstrapNode&>(node).response_time = std::chrono::milliseconds(100 + (rng_() % 500));
    } else {
        // Update node failure count
        const_cast<BootstrapNode&>(node).failure_count++;
        const_cast<BootstrapNode&>(node).status = NodeStatus::BAD;
        const_cast<BootstrapNode&>(node).last_attempt = std::chrono::steady_clock::now();
    }
    
    return success;
}

std::vector<EnhancedBootstrap::BootstrapNode> EnhancedBootstrap::getBootstrapNodes() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return bootstrap_nodes_;
}

std::vector<EnhancedBootstrap::BootstrapNode> EnhancedBootstrap::getSuccessfulBootstrapNodes() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return successful_bootstrap_nodes_;
}

std::vector<EnhancedBootstrap::BootstrapNode> EnhancedBootstrap::getFailedBootstrapNodes() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return failed_bootstrap_nodes_;
}

EnhancedBootstrap::BootstrapState EnhancedBootstrap::getBootstrapState() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return bootstrap_state_;
}

int EnhancedBootstrap::getBootstrapAttempts() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return bootstrap_attempts_;
}

int EnhancedBootstrap::getSuccessfulNodeCount() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return static_cast<int>(successful_bootstrap_nodes_.size());
}

int EnhancedBootstrap::getFailedNodeCount() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return static_cast<int>(failed_bootstrap_nodes_.size());
}

int EnhancedBootstrap::getTotalNodeCount() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    return static_cast<int>(bootstrap_nodes_.size());
}

double EnhancedBootstrap::getSuccessRate() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    
    if (bootstrap_attempts_ == 0) {
        return 0.0;
    }
    
    return static_cast<double>(successful_bootstrap_nodes_.size()) / bootstrap_attempts_;
}

std::chrono::milliseconds EnhancedBootstrap::getAverageResponseTime() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    
    if (successful_bootstrap_nodes_.empty()) {
        return std::chrono::milliseconds(0);
    }
    
    long total_time = 0;
    for (const auto& node : successful_bootstrap_nodes_) {
        total_time += node.response_time.count();
    }
    
    return std::chrono::milliseconds(total_time / successful_bootstrap_nodes_.size());
}

void EnhancedBootstrap::cleanupExpiredNodes() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = bootstrap_nodes_.begin(); it != bootstrap_nodes_.end();) {
        if (now - it->last_attempt > std::chrono::milliseconds(config_.node_timeout)) {
            it = bootstrap_nodes_.erase(it);
        } else {
            ++it;
        }
    }
}

void EnhancedBootstrap::clearAllNodes() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    bootstrap_nodes_.clear();
    successful_bootstrap_nodes_.clear();
    failed_bootstrap_nodes_.clear();
}

void EnhancedBootstrap::updateConfig(const BootstrapConfig& config) {
    config_ = config;
}

EnhancedBootstrap::BootstrapConfig EnhancedBootstrap::getConfig() const {
    return config_;
}

std::map<std::string, std::string> EnhancedBootstrap::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["bootstrap_state"] = bootstrapStateToString(getBootstrapState());
    status["bootstrap_attempts"] = std::to_string(getBootstrapAttempts());
    status["successful_nodes"] = std::to_string(getSuccessfulNodeCount());
    status["failed_nodes"] = std::to_string(getFailedNodeCount());
    status["total_nodes"] = std::to_string(getTotalNodeCount());
    status["success_rate"] = std::to_string(getSuccessRate());
    status["avg_response_time"] = std::to_string(getAverageResponseTime().count());
    
    return status;
}

std::string EnhancedBootstrap::bootstrapStateToString(BootstrapState state) {
    switch (state) {
        case BootstrapState::NOT_STARTED: return "NOT_STARTED";
        case BootstrapState::IN_PROGRESS: return "IN_PROGRESS";
        case BootstrapState::SUCCESS: return "SUCCESS";
        case BootstrapState::FAILED: return "FAILED";
        case BootstrapState::EXPIRED: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

} // namespace dht_crawler
