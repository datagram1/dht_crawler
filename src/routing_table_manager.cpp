#include "routing_table_manager.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace dht_crawler {

RoutingTableManager::RoutingTableManager(const std::string& our_node_id, const RoutingConfig& config) 
    : config_(config), our_node_id_(our_node_id), should_stop_(false) {
    initializeRoutingBuckets();
    routing_monitor_thread_ = std::thread(&RoutingTableManager::routingMonitorLoop, this);
}

RoutingTableManager::~RoutingTableManager() {
    stop();
    if (routing_monitor_thread_.joinable()) {
        routing_monitor_thread_.join();
    }
}

void RoutingTableManager::routingMonitorLoop() {
    while (!should_stop_) {
        std::unique_lock<std::mutex> lock(monitor_mutex_);
        
        // Wait for timeout or stop signal
        monitor_condition_.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return should_stop_;
        });
        
        if (should_stop_) {
            break;
        }
        
        lock.unlock();
        
        // Monitor routing table
        monitorRoutingTable();
        
        // Cleanup expired nodes
        cleanupExpiredNodes();
        
        // Update statistics
        updateStatistics();
    }
}

void RoutingTableManager::monitorRoutingTable() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    // Check for bucket splitting/merging
    for (auto& pair : routing_buckets_) {
        if (shouldSplitBucket(*pair.second)) {
            splitBucket(pair.second);
        } else if (shouldMergeBucket(*pair.second)) {
            mergeBucket(pair.second);
        }
    }
}

void RoutingTableManager::cleanupExpiredNodes() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : routing_buckets_) {
        auto& nodes = pair.second->nodes;
        for (auto it = nodes.begin(); it != nodes.end();) {
            if (isNodeExpired(**it)) {
                it = nodes.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void RoutingTableManager::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_nodes = 0;
    stats_.good_nodes = 0;
    stats_.unknown_nodes = 0;
    stats_.bad_nodes = 0;
    stats_.questionable_nodes = 0;
    stats_.evicted_nodes = 0;
    stats_.total_buckets = 0;
    stats_.active_buckets = 0;
    stats_.full_buckets = 0;
    stats_.split_buckets = 0;
    stats_.merged_buckets = 0;
    
    double total_quality_score = 0.0;
    long total_response_time = 0;
    int nodes_with_response_time = 0;
    
    for (const auto& pair : routing_buckets_) {
        stats_.total_buckets++;
        
        if (pair.second->status == BucketStatus::ACTIVE) {
            stats_.active_buckets++;
        }
        
        if (pair.second->nodes.size() >= config_.max_bucket_size) {
            stats_.full_buckets++;
        }
        
        if (pair.second->status == BucketStatus::SPLIT) {
            stats_.split_buckets++;
        }
        
        if (pair.second->status == BucketStatus::MERGED) {
            stats_.merged_buckets++;
        }
        
        for (const auto& node : pair.second->nodes) {
            stats_.total_nodes++;
            
            switch (node->status) {
                case NodeStatus::GOOD:
                    stats_.good_nodes++;
                    break;
                case NodeStatus::UNKNOWN:
                    stats_.unknown_nodes++;
                    break;
                case NodeStatus::BAD:
                    stats_.bad_nodes++;
                    break;
                case NodeStatus::QUESTIONABLE:
                    stats_.questionable_nodes++;
                    break;
                case NodeStatus::EVICTED:
                    stats_.evicted_nodes++;
                    break;
            }
            
            total_quality_score += node->quality_score;
            
            if (node->avg_response_time.count() > 0) {
                total_response_time += node->avg_response_time.count();
                nodes_with_response_time++;
            }
        }
    }
    
    if (stats_.total_nodes > 0) {
        stats_.avg_quality_score = total_quality_score / stats_.total_nodes;
    }
    
    if (nodes_with_response_time > 0) {
        stats_.avg_response_time = std::chrono::milliseconds(total_response_time / nodes_with_response_time);
    }
    
    stats_.last_update = std::chrono::steady_clock::now();
}

std::string RoutingTableManager::calculateXORDistance(const std::string& node_id1, const std::string& node_id2) {
    if (node_id1.length() != node_id2.length()) {
        return "";
    }
    
    std::string distance;
    for (size_t i = 0; i < node_id1.length(); i++) {
        distance += static_cast<char>(node_id1[i] ^ node_id2[i]);
    }
    
    return distance;
}

int RoutingTableManager::calculateBucketIndex(const std::string& node_id) {
    std::string distance = calculateXORDistance(our_node_id_, node_id);
    
    if (distance.empty()) {
        return -1;
    }
    
    // Find the first non-zero byte
    for (size_t i = 0; i < distance.length(); i++) {
        if (distance[i] != 0) {
            return static_cast<int>(i * 8 + __builtin_clz(static_cast<unsigned char>(distance[i])));
        }
    }
    
    return 0;
}

int RoutingTableManager::calculateBucketDistance(const std::string& node_id) {
    std::string distance = calculateXORDistance(our_node_id_, node_id);
    
    if (distance.empty()) {
        return -1;
    }
    
    // Calculate XOR distance as integer
    int dist = 0;
    for (size_t i = 0; i < distance.length(); i++) {
        dist = (dist << 8) | static_cast<unsigned char>(distance[i]);
    }
    
    return dist;
}

bool RoutingTableManager::addNode(const std::string& node_id, const std::string& ip_address, int port) {
    if (!isValidNodeId(node_id) || !isValidIPAddress(ip_address) || !isValidPort(port)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    // Check if node already exists
    auto it = node_registry_.find(node_id);
    if (it != node_registry_.end()) {
        return false;
    }
    
    // Create node
    auto node = std::make_shared<DHTNode>();
    node->node_id = node_id;
    node->ip_address = ip_address;
    node->port = port;
    node->status = NodeStatus::UNKNOWN;
    node->last_seen = std::chrono::steady_clock::now();
    node->last_ping = node->last_seen;
    node->last_response = node->last_seen;
    node->distance = calculateXORDistance(our_node_id_, node_id);
    node->bucket_index = calculateBucketIndex(node_id);
    node->bucket_distance = calculateBucketDistance(node_id);
    node->quality_score = 0.5;
    node->ping_attempts = 0;
    node->successful_pings = 0;
    node->failed_pings = 0;
    node->avg_response_time = std::chrono::milliseconds(0);
    node->min_response_time = std::chrono::milliseconds(0);
    node->max_response_time = std::chrono::milliseconds(0);
    node->total_queries = 0;
    node->successful_queries = 0;
    node->failed_queries = 0;
    node->timeout_queries = 0;
    node->is_evictable = true;
    node->eviction_time = std::chrono::steady_clock::now();
    node->eviction_attempts = 0;
    
    // Add to registry
    node_registry_[node_id] = node;
    
    // Add to appropriate bucket
    if (node->bucket_index >= 0) {
        auto bucket_it = routing_buckets_.find(node->bucket_index);
        if (bucket_it != routing_buckets_.end()) {
            bucket_it->second->nodes.push_back(node);
            bucket_it->second->last_updated = std::chrono::steady_clock::now();
        }
    }
    
    return true;
}

bool RoutingTableManager::addNodeWithQuality(const std::string& node_id,
                                           const std::string& ip_address,
                                           int port,
                                           double quality_score,
                                           std::chrono::milliseconds response_time) {
    if (!addNode(node_id, ip_address, port)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it != node_registry_.end()) {
        it->second->quality_score = quality_score;
        it->second->avg_response_time = response_time;
        it->second->min_response_time = response_time;
        it->second->max_response_time = response_time;
        updateNodeQuality(it->second, true, response_time);
    }
    
    return true;
}

bool RoutingTableManager::removeNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it == node_registry_.end()) {
        return false;
    }
    
    auto node = it->second;
    
    // Remove from bucket
    if (node->bucket_index >= 0) {
        auto bucket_it = routing_buckets_.find(node->bucket_index);
        if (bucket_it != routing_buckets_.end()) {
            auto& nodes = bucket_it->second->nodes;
            nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
        }
    }
    
    // Remove from registry
    node_registry_.erase(it);
    
    return true;
}

bool RoutingTableManager::updateNode(const std::string& node_id, bool success, std::chrono::milliseconds response_time) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it == node_registry_.end()) {
        return false;
    }
    
    auto node = it->second;
    node->last_seen = std::chrono::steady_clock::now();
    
    if (success) {
        node->last_response = node->last_seen;
        node->successful_queries++;
    } else {
        node->failed_queries++;
    }
    
    node->total_queries++;
    updateNodeQuality(node, success, response_time);
    
    return true;
}

bool RoutingTableManager::updateNodeStatus(const std::string& node_id, NodeStatus status) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it == node_registry_.end()) {
        return false;
    }
    
    it->second->status = status;
    return true;
}

std::shared_ptr<RoutingTableManager::DHTNode> RoutingTableManager::getNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it != node_registry_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<RoutingTableManager::DHTNode>> RoutingTableManager::getNodesByStatus(NodeStatus status) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    std::vector<std::shared_ptr<DHTNode>> nodes;
    for (const auto& pair : node_registry_) {
        if (pair.second->status == status) {
            nodes.push_back(pair.second);
        }
    }
    
    return nodes;
}

std::vector<std::shared_ptr<RoutingTableManager::DHTNode>> RoutingTableManager::getNodesInBucket(int bucket_index) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = routing_buckets_.find(bucket_index);
    if (it != routing_buckets_.end()) {
        return it->second->nodes;
    }
    
    return {};
}

std::vector<std::shared_ptr<RoutingTableManager::DHTNode>> RoutingTableManager::getClosestNodes(const std::string& target_id, int count) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    std::vector<std::shared_ptr<DHTNode>> all_nodes;
    for (const auto& pair : node_registry_) {
        all_nodes.push_back(pair.second);
    }
    
    // Sort by XOR distance
    std::sort(all_nodes.begin(), all_nodes.end(),
              [&](const std::shared_ptr<DHTNode>& a, const std::shared_ptr<DHTNode>& b) {
                  return isNodeCloser(a->node_id, b->node_id);
              });
    
    // Return closest nodes
    if (count > 0 && count < static_cast<int>(all_nodes.size())) {
        all_nodes.resize(count);
    }
    
    return all_nodes;
}

std::vector<std::shared_ptr<RoutingTableManager::DHTNode>> RoutingTableManager::getRandomNodes(int count) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    std::vector<std::shared_ptr<DHTNode>> all_nodes;
    for (const auto& pair : node_registry_) {
        all_nodes.push_back(pair.second);
    }
    
    // Shuffle and return random nodes
    std::random_shuffle(all_nodes.begin(), all_nodes.end());
    
    if (count > 0 && count < static_cast<int>(all_nodes.size())) {
        all_nodes.resize(count);
    }
    
    return all_nodes;
}

std::vector<std::shared_ptr<RoutingTableManager::DHTNode>> RoutingTableManager::getGoodNodes(int count) {
    return getNodesByStatus(NodeStatus::GOOD);
}

std::vector<std::string> RoutingTableManager::getNodesNeedingPing() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    std::vector<std::string> nodes;
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& pair : node_registry_) {
        if (now - pair.second->last_ping > std::chrono::milliseconds(config_.ping_interval)) {
            nodes.push_back(pair.first);
        }
    }
    
    return nodes;
}

bool RoutingTableManager::hasNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    return node_registry_.find(node_id) != node_registry_.end();
}

bool RoutingTableManager::isNodeGood(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    return it != node_registry_.end() && it->second->status == NodeStatus::GOOD;
}

bool RoutingTableManager::isNodeBad(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    return it != node_registry_.end() && it->second->status == NodeStatus::BAD;
}

RoutingTableManager::NodeStatus RoutingTableManager::getNodeStatus(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it != node_registry_.end()) {
        return it->second->status;
    }
    
    return NodeStatus::UNKNOWN;
}

double RoutingTableManager::getNodeQualityScore(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it != node_registry_.end()) {
        return it->second->quality_score;
    }
    
    return 0.0;
}

std::string RoutingTableManager::getNodeDistance(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it != node_registry_.end()) {
        return it->second->distance;
    }
    
    return "";
}

std::shared_ptr<RoutingTableManager::RoutingBucket> RoutingTableManager::getBucket(int bucket_index) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = routing_buckets_.find(bucket_index);
    if (it != routing_buckets_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::map<int, std::shared_ptr<RoutingTableManager::RoutingBucket>> RoutingTableManager::getAllBuckets() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    return routing_buckets_;
}

RoutingTableManager::RoutingTableStatistics RoutingTableManager::getStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void RoutingTableManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = RoutingTableStatistics{};
}

void RoutingTableManager::updateConfig(const RoutingConfig& config) {
    config_ = config;
}

size_t RoutingTableManager::getRoutingTableSize() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    return node_registry_.size();
}

size_t RoutingTableManager::getMaxRoutingTableSize() {
    return config_.max_routing_table_size;
}

void RoutingTableManager::setMaxRoutingTableSize(size_t max_size) {
    config_.max_routing_table_size = max_size;
}

int RoutingTableManager::getBucketCount() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    return static_cast<int>(routing_buckets_.size());
}

double RoutingTableManager::getAverageQualityScore() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.avg_quality_score;
}

std::chrono::milliseconds RoutingTableManager::getAverageResponseTime() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.avg_response_time;
}

double RoutingTableManager::getRoutingEfficiency() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (stats_.total_nodes == 0) {
        return 0.0;
    }
    
    return static_cast<double>(stats_.good_nodes) / stats_.total_nodes;
}

double RoutingTableManager::getNodeUtilization() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (config_.max_routing_table_size == 0) {
        return 0.0;
    }
    
    return static_cast<double>(stats_.total_nodes) / config_.max_routing_table_size;
}

void RoutingTableManager::start() {
    should_stop_ = false;
    if (!routing_monitor_thread_.joinable()) {
        routing_monitor_thread_ = std::thread(&RoutingTableManager::routingMonitorLoop, this);
    }
}

void RoutingTableManager::stop() {
    should_stop_ = true;
    monitor_condition_.notify_all();
}

bool RoutingTableManager::isRunning() const {
    return !should_stop_ && routing_monitor_thread_.joinable();
}

bool RoutingTableManager::exportRoutingTable(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    file << "NodeID,IP,Port,Status,QualityScore,ResponseTime" << std::endl;
    
    for (const auto& pair : node_registry_) {
        const auto& node = pair.second;
        file << node->node_id << "," << node->ip_address << "," << node->port << ","
             << static_cast<int>(node->status) << "," << node->quality_score << ","
             << node->avg_response_time.count() << std::endl;
    }
    
    file.close();
    return true;
}

bool RoutingTableManager::exportStatistics(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    file << "Metric,Value" << std::endl;
    file << "TotalNodes," << stats_.total_nodes << std::endl;
    file << "GoodNodes," << stats_.good_nodes << std::endl;
    file << "UnknownNodes," << stats_.unknown_nodes << std::endl;
    file << "BadNodes," << stats_.bad_nodes << std::endl;
    file << "TotalBuckets," << stats_.total_buckets << std::endl;
    file << "ActiveBuckets," << stats_.active_buckets << std::endl;
    file << "AverageQualityScore," << stats_.avg_quality_score << std::endl;
    file << "AverageResponseTime," << stats_.avg_response_time.count() << std::endl;
    
    file.close();
    return true;
}

std::map<std::string, std::string> RoutingTableManager::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["total_nodes"] = std::to_string(getRoutingTableSize());
    status["good_nodes"] = std::to_string(getGoodNodeCount());
    status["bad_nodes"] = std::to_string(getBadNodeCount());
    status["unknown_nodes"] = std::to_string(getUnknownNodeCount());
    status["total_buckets"] = std::to_string(getBucketCount());
    status["average_quality_score"] = std::to_string(getAverageQualityScore());
    status["routing_efficiency"] = std::to_string(getRoutingEfficiency());
    status["node_utilization"] = std::to_string(getNodeUtilization());
    status["is_running"] = isRunning() ? "true" : "false";
    
    return status;
}

void RoutingTableManager::clearRoutingTable() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    node_registry_.clear();
    routing_buckets_.clear();
    initializeRoutingBuckets();
}

int RoutingTableManager::clearExpiredNodes() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    int cleared_count = 0;
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = node_registry_.begin(); it != node_registry_.end();) {
        if (isNodeExpired(*it->second)) {
            it = node_registry_.erase(it);
            cleared_count++;
        } else {
            ++it;
        }
    }
    
    return cleared_count;
}

void RoutingTableManager::forceCleanup() {
    clearExpiredNodes();
}

std::map<std::string, int> RoutingTableManager::getNodeStatisticsByIP() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    std::map<std::string, int> stats;
    for (const auto& pair : node_registry_) {
        stats[pair.second->ip_address]++;
    }
    
    return stats;
}

std::map<int, std::map<std::string, int>> RoutingTableManager::getBucketStatistics() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    std::map<int, std::map<std::string, int>> stats;
    for (const auto& pair : routing_buckets_) {
        stats[pair.first]["total_nodes"] = static_cast<int>(pair.second->nodes.size());
        stats[pair.first]["active_nodes"] = 0;
        stats[pair.first]["good_nodes"] = 0;
        stats[pair.first]["bad_nodes"] = 0;
        
        for (const auto& node : pair.second->nodes) {
            if (node->status == NodeStatus::GOOD) {
                stats[pair.first]["good_nodes"]++;
            } else if (node->status == NodeStatus::BAD) {
                stats[pair.first]["bad_nodes"]++;
            }
        }
    }
    
    return stats;
}

std::vector<std::string> RoutingTableManager::getRoutingTableRecommendations() {
    std::vector<std::string> recommendations;
    
    if (getRoutingTableSize() < config_.max_routing_table_size * 0.5) {
        recommendations.push_back("Consider adding more bootstrap nodes to increase routing table size");
    }
    
    if (getRoutingEfficiency() < 0.7) {
        recommendations.push_back("Routing efficiency is low, consider improving node quality assessment");
    }
    
    if (getNodeUtilization() > 0.9) {
        recommendations.push_back("Routing table is nearly full, consider increasing max size or improving eviction");
    }
    
    return recommendations;
}

void RoutingTableManager::optimizeRoutingTable() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    // Remove bad nodes
    for (auto it = node_registry_.begin(); it != node_registry_.end();) {
        if (it->second->status == NodeStatus::BAD) {
            it = node_registry_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Update bucket statuses
    for (auto& pair : routing_buckets_) {
        if (pair.second->nodes.size() >= config_.max_bucket_size) {
            pair.second->status = BucketStatus::FULL;
        } else if (pair.second->nodes.size() < config_.min_bucket_size) {
            pair.second->status = BucketStatus::INACTIVE;
        } else {
            pair.second->status = BucketStatus::ACTIVE;
        }
    }
}

std::map<std::string, double> RoutingTableManager::getRoutingTablePerformanceMetrics() {
    std::map<std::string, double> metrics;
    
    metrics["routing_efficiency"] = getRoutingEfficiency();
    metrics["node_utilization"] = getNodeUtilization();
    metrics["average_quality_score"] = getAverageQualityScore();
    metrics["response_time_ms"] = getAverageResponseTime().count();
    
    return metrics;
}

bool RoutingTableManager::forceBucketSplit(int bucket_index) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = routing_buckets_.find(bucket_index);
    if (it != routing_buckets_.end()) {
        splitBucket(it->second);
        return true;
    }
    
    return false;
}

bool RoutingTableManager::forceBucketMerge(int bucket_index) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = routing_buckets_.find(bucket_index);
    if (it != routing_buckets_.end()) {
        mergeBucket(it->second);
        return true;
    }
    
    return false;
}

bool RoutingTableManager::forceNodeEviction(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    auto it = node_registry_.find(node_id);
    if (it != node_registry_.end()) {
        evictNode(it->second);
        return true;
    }
    
    return false;
}

std::map<std::string, int> RoutingTableManager::getRoutingTableCapacity() {
    std::map<std::string, int> capacity;
    
    capacity["current_size"] = static_cast<int>(getRoutingTableSize());
    capacity["max_size"] = static_cast<int>(config_.max_routing_table_size);
    capacity["available_capacity"] = static_cast<int>(config_.max_routing_table_size - getRoutingTableSize());
    capacity["utilization_percent"] = static_cast<int>(getNodeUtilization() * 100);
    
    return capacity;
}

bool RoutingTableManager::validateRoutingTableIntegrity() {
    std::lock_guard<std::mutex> lock(buckets_mutex_);
    
    // Check if all nodes are in correct buckets
    for (const auto& pair : node_registry_) {
        const auto& node = pair.second;
        if (node->bucket_index >= 0) {
            auto bucket_it = routing_buckets_.find(node->bucket_index);
            if (bucket_it != routing_buckets_.end()) {
                const auto& nodes = bucket_it->second->nodes;
                if (std::find(nodes.begin(), nodes.end(), node) == nodes.end()) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

std::vector<std::string> RoutingTableManager::getRoutingTableIssues() {
    std::vector<std::string> issues;
    
    if (!validateRoutingTableIntegrity()) {
        issues.push_back("Routing table integrity check failed");
    }
    
    if (getRoutingEfficiency() < 0.5) {
        issues.push_back("Low routing efficiency");
    }
    
    if (getNodeUtilization() > 0.95) {
        issues.push_back("Routing table nearly full");
    }
    
    return issues;
}

int RoutingTableManager::repairRoutingTable() {
    int repaired_count = 0;
    
    if (!validateRoutingTableIntegrity()) {
        // Rebuild routing table
        clearRoutingTable();
        repaired_count++;
    }
    
    if (getRoutingEfficiency() < 0.5) {
        // Remove bad nodes
        repaired_count += clearExpiredNodes();
    }
    
    return repaired_count;
}

void RoutingTableManager::initializeRoutingBuckets() {
    // Initialize with a few default buckets
    for (int i = 0; i < 8; i++) {
        createBucket(i, i);
    }
}

void RoutingTableManager::createBucket(int bucket_index, int bucket_distance) {
    auto bucket = std::make_shared<RoutingBucket>();
    bucket->bucket_index = bucket_index;
    bucket->bucket_distance = bucket_distance;
    bucket->status = BucketStatus::ACTIVE;
    bucket->last_updated = std::chrono::steady_clock::now();
    bucket->last_split = std::chrono::steady_clock::now();
    bucket->last_merge = std::chrono::steady_clock::now();
    bucket->total_nodes_added = 0;
    bucket->total_nodes_removed = 0;
    bucket->total_nodes_evicted = 0;
    bucket->avg_quality_score = 0.0;
    bucket->avg_response_time = std::chrono::milliseconds(0);
    
    routing_buckets_[bucket_index] = bucket;
}

void RoutingTableManager::destroyBucket(int bucket_index) {
    auto it = routing_buckets_.find(bucket_index);
    if (it != routing_buckets_.end()) {
        routing_buckets_.erase(it);
    }
}

bool RoutingTableManager::isValidNodeId(const std::string& node_id) {
    return node_id.length() == 20; // DHT node ID is 20 bytes
}

bool RoutingTableManager::isValidIPAddress(const std::string& ip_address) {
    // Simple IP address validation
    return !ip_address.empty() && ip_address.find('.') != std::string::npos;
}

bool RoutingTableManager::isValidPort(int port) {
    return port > 0 && port <= 65535;
}

bool RoutingTableManager::isNodeCloser(const std::string& node_id1, const std::string& node_id2) {
    std::string distance1 = calculateXORDistance(our_node_id_, node_id1);
    std::string distance2 = calculateXORDistance(our_node_id_, node_id2);
    
    if (distance1.empty() || distance2.empty()) {
        return false;
    }
    
    // Compare XOR distances
    for (size_t i = 0; i < distance1.length(); i++) {
        if (distance1[i] < distance2[i]) {
            return true;
        } else if (distance1[i] > distance2[i]) {
            return false;
        }
    }
    
    return false;
}

bool RoutingTableManager::shouldSplitBucket(const RoutingBucket& bucket) {
    return bucket.nodes.size() >= config_.max_bucket_size && config_.enable_bucket_splitting;
}

bool RoutingTableManager::shouldMergeBucket(const RoutingBucket& bucket) {
    return bucket.nodes.size() < config_.min_bucket_size && config_.enable_bucket_merging;
}

void RoutingTableManager::splitBucket(std::shared_ptr<RoutingBucket> bucket) {
    if (!shouldSplitBucket(*bucket)) {
        return;
    }
    
    // Create new bucket
    int new_bucket_index = bucket->bucket_index + 1;
    createBucket(new_bucket_index, new_bucket_index);
    
    // Move nodes to appropriate bucket
    auto new_bucket = routing_buckets_[new_bucket_index];
    for (auto it = bucket->nodes.begin(); it != bucket->nodes.end();) {
        if ((*it)->bucket_index == new_bucket_index) {
            new_bucket->nodes.push_back(*it);
            it = bucket->nodes.erase(it);
        } else {
            ++it;
        }
    }
    
    bucket->status = BucketStatus::SPLIT;
    bucket->last_split = std::chrono::steady_clock::now();
}

void RoutingTableManager::mergeBucket(std::shared_ptr<RoutingBucket> bucket) {
    if (!shouldMergeBucket(*bucket)) {
        return;
    }
    
    // Find parent bucket
    int parent_bucket_index = bucket->bucket_index - 1;
    auto parent_it = routing_buckets_.find(parent_bucket_index);
    
    if (parent_it != routing_buckets_.end()) {
        // Move nodes to parent bucket
        parent_it->second->nodes.insert(parent_it->second->nodes.end(), 
                                       bucket->nodes.begin(), bucket->nodes.end());
        
        // Update node bucket indices
        for (auto& node : bucket->nodes) {
            node->bucket_index = parent_bucket_index;
        }
        
        // Remove bucket
        destroyBucket(bucket->bucket_index);
        
        parent_it->second->status = BucketStatus::MERGED;
        parent_it->second->last_merge = std::chrono::steady_clock::now();
    }
}

void RoutingTableManager::evictNode(std::shared_ptr<DHTNode> node) {
    node->status = NodeStatus::EVICTED;
    node->eviction_time = std::chrono::steady_clock::now();
    node->eviction_attempts++;
    
    // Remove from bucket
    if (node->bucket_index >= 0) {
        auto bucket_it = routing_buckets_.find(node->bucket_index);
        if (bucket_it != routing_buckets_.end()) {
            auto& nodes = bucket_it->second->nodes;
            nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
        }
    }
    
    // Remove from registry
    node_registry_.erase(node->node_id);
}

std::shared_ptr<RoutingTableManager::DHTNode> RoutingTableManager::findEvictableNode(const RoutingBucket& bucket) {
    for (const auto& node : bucket.nodes) {
        if (node->is_evictable && node->status == NodeStatus::BAD) {
            return node;
        }
    }
    
    // If no bad nodes, find oldest node
    auto oldest_node = std::min_element(bucket.nodes.begin(), bucket.nodes.end(),
                                       [](const std::shared_ptr<DHTNode>& a, const std::shared_ptr<DHTNode>& b) {
                                           return a->last_seen < b->last_seen;
                                       });
    
    return (oldest_node != bucket.nodes.end()) ? *oldest_node : nullptr;
}

void RoutingTableManager::updateNodeQuality(std::shared_ptr<DHTNode> node, bool success, std::chrono::milliseconds response_time) {
    if (success) {
        node->successful_pings++;
        node->avg_response_time = response_time;
        node->min_response_time = std::min(node->min_response_time, response_time);
        node->max_response_time = std::max(node->max_response_time, response_time);
    } else {
        node->failed_pings++;
    }
    
    node->ping_attempts++;
    
    // Update quality score
    if (node->ping_attempts > 0) {
        double success_rate = static_cast<double>(node->successful_pings) / node->ping_attempts;
        node->quality_score = success_rate;
    }
    
    // Update status based on quality
    node->status = determineNodeStatus(*node);
}

RoutingTableManager::NodeStatus RoutingTableManager::determineNodeStatus(const DHTNode& node) {
    if (node.quality_score >= config_.good_threshold) {
        return NodeStatus::GOOD;
    } else if (node.quality_score <= config_.bad_threshold) {
        return NodeStatus::BAD;
    } else {
        return NodeStatus::UNKNOWN;
    }
}

bool RoutingTableManager::isNodeExpired(const DHTNode& node) {
    auto now = std::chrono::steady_clock::now();
    return now - node.last_seen > std::chrono::milliseconds(config_.node_timeout);
}

void RoutingTableManager::redistributeNodes(std::shared_ptr<RoutingBucket> bucket) {
    // Redistribute nodes based on their actual bucket indices
    for (auto it = bucket->nodes.begin(); it != bucket->nodes.end();) {
        if ((*it)->bucket_index != bucket->bucket_index) {
            // Move to correct bucket
            auto target_bucket = routing_buckets_.find((*it)->bucket_index);
            if (target_bucket != routing_buckets_.end()) {
                target_bucket->second->nodes.push_back(*it);
            }
            it = bucket->nodes.erase(it);
        } else {
            ++it;
        }
    }
}

std::string RoutingTableManager::nodeStatusToString(NodeStatus status) {
    switch (status) {
        case NodeStatus::GOOD: return "GOOD";
        case NodeStatus::UNKNOWN: return "UNKNOWN";
        case NodeStatus::BAD: return "BAD";
        case NodeStatus::QUESTIONABLE: return "QUESTIONABLE";
        case NodeStatus::EVICTED: return "EVICTED";
        default: return "UNKNOWN";
    }
}

std::string RoutingTableManager::bucketStatusToString(BucketStatus status) {
    switch (status) {
        case BucketStatus::ACTIVE: return "ACTIVE";
        case BucketStatus::INACTIVE: return "INACTIVE";
        case BucketStatus::FULL: return "FULL";
        case BucketStatus::SPLIT: return "SPLIT";
        case BucketStatus::MERGED: return "MERGED";
        default: return "UNKNOWN";
    }
}

} // namespace dht_crawler
