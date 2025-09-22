#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <set>
#include <algorithm>

namespace dht_crawler {

/**
 * Routing table manager for better DHT node management
 * Provides node distance calculation, quality assessment, and routing table size limits
 */
class RoutingTableManager {
public:
    enum class NodeStatus {
        GOOD,           // Node is responding well
        UNKNOWN,        // Node status is unknown
        BAD,            // Node is not responding well
        QUESTIONABLE,   // Node is questionable
        EVICTED         // Node has been evicted
    };

    enum class BucketStatus {
        ACTIVE,         // Bucket is active
        INACTIVE,       // Bucket is inactive
        FULL,           // Bucket is full
        SPLIT,          // Bucket has been split
        MERGED          // Bucket has been merged
    };

    struct RoutingConfig {
        size_t max_routing_table_size = 8000;           // 8000 nodes like magnetico
        size_t max_bucket_size = 8;                     // Max nodes per bucket
        size_t min_bucket_size = 4;                     // Min nodes per bucket
        std::chrono::milliseconds node_timeout{300000}; // 5 minutes
        std::chrono::milliseconds ping_interval{300000}; // 5 minutes
        bool enable_bucket_splitting = true;           // Enable bucket splitting
        bool enable_bucket_merging = true;              // Enable bucket merging
        bool enable_node_eviction = true;               // Enable node eviction
        bool enable_quality_assessment = true;          // Enable quality assessment
        double good_threshold = 0.8;                     // Threshold for GOOD status
        double bad_threshold = 0.3;                     // Threshold for BAD status
        int max_eviction_attempts = 3;                  // Max eviction attempts
        std::chrono::milliseconds eviction_delay{1000}; // Eviction delay
    };

    struct DHTNode {
        std::string node_id;
        std::string ip_address;
        int port;
        NodeStatus status;
        std::chrono::steady_clock::time_point last_seen;
        std::chrono::steady_clock::time_point last_ping;
        std::chrono::steady_clock::time_point last_response;
        
        // Distance and routing
        std::string distance; // XOR distance as hex string
        int bucket_index;
        int bucket_distance;
        
        // Quality metrics
        double quality_score;
        int ping_attempts;
        int successful_pings;
        int failed_pings;
        std::chrono::milliseconds avg_response_time;
        std::chrono::milliseconds min_response_time;
        std::chrono::milliseconds max_response_time;
        
        // Query statistics
        int total_queries;
        int successful_queries;
        int failed_queries;
        int timeout_queries;
        
        // Eviction tracking
        bool is_evictable;
        std::chrono::steady_clock::time_point eviction_time;
        int eviction_attempts;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct RoutingBucket {
        int bucket_index;
        int bucket_distance;
        BucketStatus status;
        std::vector<std::shared_ptr<DHTNode>> nodes;
        std::chrono::steady_clock::time_point last_updated;
        std::chrono::steady_clock::time_point last_split;
        std::chrono::steady_clock::time_point last_merge;
        
        // Bucket statistics
        int total_nodes_added;
        int total_nodes_removed;
        int total_nodes_evicted;
        double avg_quality_score;
        std::chrono::milliseconds avg_response_time;
        
        // Bucket metadata
        std::map<std::string, std::string> metadata;
    };

    struct RoutingTableStatistics {
        int total_nodes = 0;
        int good_nodes = 0;
        int unknown_nodes = 0;
        int bad_nodes = 0;
        int questionable_nodes = 0;
        int evicted_nodes = 0;
        int total_buckets = 0;
        int active_buckets = 0;
        int full_buckets = 0;
        int split_buckets = 0;
        int merged_buckets = 0;
        double avg_quality_score = 0.0;
        std::chrono::milliseconds avg_response_time;
        std::map<NodeStatus, int> nodes_by_status;
        std::map<BucketStatus, int> buckets_by_status;
        std::map<int, int> nodes_by_bucket;
        std::map<std::string, int> nodes_by_ip;
        double routing_efficiency = 0.0;
        double node_utilization = 0.0;
        std::chrono::steady_clock::time_point last_update;
    };

private:
    RoutingConfig config_;
    std::string our_node_id_;
    std::map<int, std::shared_ptr<RoutingBucket>> routing_buckets_;
    std::mutex buckets_mutex_;
    
    std::map<std::string, std::shared_ptr<DHTNode>> node_registry_;
    std::mutex registry_mutex_;
    
    RoutingTableStatistics stats_;
    std::mutex stats_mutex_;
    
    // Background processing
    std::thread routing_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Internal methods
    void routingMonitorLoop();
    void monitorRoutingTable();
    void cleanupExpiredNodes();
    void updateStatistics();
    std::string calculateXORDistance(const std::string& node_id1, const std::string& node_id2);
    int calculateBucketIndex(const std::string& node_id);
    int calculateBucketDistance(const std::string& node_id);
    std::string nodeStatusToString(NodeStatus status);
    std::string bucketStatusToString(BucketStatus status);
    bool isNodeCloser(const std::string& node_id1, const std::string& node_id2);
    bool shouldSplitBucket(const RoutingBucket& bucket);
    bool shouldMergeBucket(const RoutingBucket& bucket);
    void splitBucket(std::shared_ptr<RoutingBucket> bucket);
    void mergeBucket(std::shared_ptr<RoutingBucket> bucket);
    void evictNode(std::shared_ptr<DHTNode> node);
    std::shared_ptr<DHTNode> findEvictableNode(const RoutingBucket& bucket);
    void updateNodeQuality(std::shared_ptr<DHTNode> node, bool success, std::chrono::milliseconds response_time);
    NodeStatus determineNodeStatus(const DHTNode& node);
    bool isNodeExpired(const DHTNode& node);
    void redistributeNodes(std::shared_ptr<RoutingBucket> bucket);
    std::vector<std::shared_ptr<DHTNode>> getClosestNodes(const std::string& target_id, int count);
    std::vector<std::shared_ptr<DHTNode>> getNodesInBucket(int bucket_index);
    std::vector<std::shared_ptr<DHTNode>> getNodesByStatus(NodeStatus status);
    void initializeRoutingBuckets();
    void createBucket(int bucket_index, int bucket_distance);
    void destroyBucket(int bucket_index);
    bool isValidNodeId(const std::string& node_id);
    bool isValidIPAddress(const std::string& ip_address);
    bool isValidPort(int port);

public:
    RoutingTableManager(const std::string& our_node_id, const RoutingConfig& config = RoutingConfig{});
    ~RoutingTableManager();
    
    /**
     * Add a node to the routing table
     * @param node_id Node ID
     * @param ip_address IP address
     * @param port Port
     * @return true if node was added successfully
     */
    bool addNode(const std::string& node_id, const std::string& ip_address, int port);
    
    /**
     * Add a node with quality information
     * @param node_id Node ID
     * @param ip_address IP address
     * @param port Port
     * @param quality_score Quality score
     * @param response_time Response time
     * @return true if node was added successfully
     */
    bool addNodeWithQuality(const std::string& node_id,
                           const std::string& ip_address,
                           int port,
                           double quality_score,
                           std::chrono::milliseconds response_time);
    
    /**
     * Remove a node from the routing table
     * @param node_id Node ID
     * @return true if node was removed successfully
     */
    bool removeNode(const std::string& node_id);
    
    /**
     * Update node information
     * @param node_id Node ID
     * @param success Whether last operation was successful
     * @param response_time Response time
     * @return true if node was updated successfully
     */
    bool updateNode(const std::string& node_id, bool success, std::chrono::milliseconds response_time);
    
    /**
     * Update node status
     * @param node_id Node ID
     * @param status New status
     * @return true if status was updated successfully
     */
    bool updateNodeStatus(const std::string& node_id, NodeStatus status);
    
    /**
     * Get node information
     * @param node_id Node ID
     * @return Node information or nullptr if not found
     */
    std::shared_ptr<DHTNode> getNode(const std::string& node_id);
    
    /**
     * Get nodes by status
     * @param status Node status
     * @return Vector of node information
     */
    std::vector<std::shared_ptr<DHTNode>> getNodesByStatus(NodeStatus status);
    
    /**
     * Get nodes in a specific bucket
     * @param bucket_index Bucket index
     * @return Vector of node information
     */
    std::vector<std::shared_ptr<DHTNode>> getNodesInBucket(int bucket_index);
    
    /**
     * Get closest nodes to a target
     * @param target_id Target node ID
     * @param count Number of nodes to return
     * @return Vector of closest node information
     */
    std::vector<std::shared_ptr<DHTNode>> getClosestNodes(const std::string& target_id, int count = 8);
    
    /**
     * Get random nodes from the routing table
     * @param count Number of nodes to return
     * @return Vector of random node information
     */
    std::vector<std::shared_ptr<DHTNode>> getRandomNodes(int count = 8);
    
    /**
     * Get good nodes from the routing table
     * @param count Number of nodes to return
     * @return Vector of good node information
     */
    std::vector<std::shared_ptr<DHTNode>> getGoodNodes(int count = 8);
    
    /**
     * Get nodes that need pinging
     * @return Vector of node IDs that need pinging
     */
    std::vector<std::string> getNodesNeedingPing();
    
    /**
     * Check if node exists in routing table
     * @param node_id Node ID
     * @return true if node exists
     */
    bool hasNode(const std::string& node_id);
    
    /**
     * Check if node is good
     * @param node_id Node ID
     * @return true if node is good
     */
    bool isNodeGood(const std::string& node_id);
    
    /**
     * Check if node is bad
     * @param node_id Node ID
     * @return true if node is bad
     */
    bool isNodeBad(const std::string& node_id);
    
    /**
     * Get node status
     * @param node_id Node ID
     * @return Node status or UNKNOWN if not found
     */
    NodeStatus getNodeStatus(const std::string& node_id);
    
    /**
     * Get node quality score
     * @param node_id Node ID
     * @return Quality score or 0.0 if not found
     */
    double getNodeQualityScore(const std::string& node_id);
    
    /**
     * Get node distance
     * @param node_id Node ID
     * @return Distance or empty string if not found
     */
    std::string getNodeDistance(const std::string& node_id);
    
    /**
     * Get bucket information
     * @param bucket_index Bucket index
     * @return Bucket information or nullptr if not found
     */
    std::shared_ptr<RoutingBucket> getBucket(int bucket_index);
    
    /**
     * Get all buckets
     * @return Map of bucket index to bucket information
     */
    std::map<int, std::shared_ptr<RoutingBucket>> getAllBuckets();
    
    /**
     * Get routing table statistics
     * @return Current routing table statistics
     */
    RoutingTableStatistics getStatistics();
    
    /**
     * Reset routing table statistics
     */
    void resetStatistics();
    
    /**
     * Update routing configuration
     * @param config New routing configuration
     */
    void updateConfig(const RoutingConfig& config);
    
    /**
     * Get routing table size
     * @return Total number of nodes in routing table
     */
    size_t getRoutingTableSize();
    
    /**
     * Get maximum routing table size
     * @return Maximum routing table size
     */
    size_t getMaxRoutingTableSize();
    
    /**
     * Set maximum routing table size
     * @param max_size New maximum size
     */
    void setMaxRoutingTableSize(size_t max_size);
    
    /**
     * Get bucket count
     * @return Number of buckets
     */
    int getBucketCount();
    
    /**
     * Get average quality score
     * @return Average quality score
     */
    double getAverageQualityScore();
    
    /**
     * Get average response time
     * @return Average response time
     */
    std::chrono::milliseconds getAverageResponseTime();
    
    /**
     * Get routing efficiency
     * @return Routing efficiency (0.0 to 1.0)
     */
    double getRoutingEfficiency();
    
    /**
     * Get node utilization
     * @return Node utilization (0.0 to 1.0)
     */
    double getNodeUtilization();
    
    /**
     * Start routing table manager
     */
    void start();
    
    /**
     * Stop routing table manager
     */
    void stop();
    
    /**
     * Check if routing manager is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export routing table to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportRoutingTable(const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get routing manager health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear routing table
     */
    void clearRoutingTable();
    
    /**
     * Clear expired nodes
     * @return Number of nodes cleared
     */
    int clearExpiredNodes();
    
    /**
     * Force cleanup of routing table
     */
    void forceCleanup();
    
    /**
     * Get node statistics by IP
     * @return Map of IP addresses to node counts
     */
    std::map<std::string, int> getNodeStatisticsByIP();
    
    /**
     * Get bucket statistics
     * @return Map of bucket indices to statistics
     */
    std::map<int, std::map<std::string, int>> getBucketStatistics();
    
    /**
     * Get routing table recommendations
     * @return Vector of routing table recommendations
     */
    std::vector<std::string> getRoutingTableRecommendations();
    
    /**
     * Optimize routing table
     */
    void optimizeRoutingTable();
    
    /**
     * Get routing table performance metrics
     * @return Performance metrics
     */
    std::map<std::string, double> getRoutingTablePerformanceMetrics();
    
    /**
     * Force bucket splitting
     * @param bucket_index Bucket index to split
     * @return true if bucket was split successfully
     */
    bool forceBucketSplit(int bucket_index);
    
    /**
     * Force bucket merging
     * @param bucket_index Bucket index to merge
     * @return true if bucket was merged successfully
     */
    bool forceBucketMerge(int bucket_index);
    
    /**
     * Force node eviction
     * @param node_id Node ID to evict
     * @return true if node was evicted successfully
     */
    bool forceNodeEviction(const std::string& node_id);
    
    /**
     * Get routing table capacity
     * @return Routing table capacity information
     */
    std::map<std::string, int> getRoutingTableCapacity();
    
    /**
     * Validate routing table integrity
     * @return true if routing table is valid
     */
    bool validateRoutingTableIntegrity();
    
    /**
     * Get routing table issues
     * @return Vector of routing table issues
     */
    std::vector<std::string> getRoutingTableIssues();
    
    /**
     * Repair routing table
     * @return Number of issues repaired
     */
    int repairRoutingTable();
};

/**
 * Routing table manager factory for different routing strategies
 */
class RoutingTableManagerFactory {
public:
    enum class RoutingStrategy {
        PERFORMANCE,    // Optimized for performance
        RELIABILITY,    // Optimized for reliability
        BALANCED,       // Balanced performance and reliability
        CUSTOM          // Custom routing configuration
    };
    
    static std::unique_ptr<RoutingTableManager> createManager(
        const std::string& our_node_id,
        RoutingStrategy strategy,
        const RoutingTableManager::RoutingConfig& config = RoutingTableManager::RoutingConfig{}
    );
    
    static RoutingTableManager::RoutingConfig getDefaultConfig(RoutingStrategy strategy);
};

/**
 * RAII routing table wrapper for automatic routing management
 */
class RoutingTableGuard {
private:
    std::shared_ptr<RoutingTableManager> manager_;
    std::string our_node_id_;

public:
    RoutingTableGuard(std::shared_ptr<RoutingTableManager> manager, const std::string& our_node_id);
    
    /**
     * Add a node to the routing table
     * @param node_id Node ID
     * @param ip_address IP address
     * @param port Port
     * @return true if node was added successfully
     */
    bool addNode(const std::string& node_id, const std::string& ip_address, int port);
    
    /**
     * Update node information
     * @param node_id Node ID
     * @param success Whether last operation was successful
     * @param response_time Response time
     * @return true if node was updated successfully
     */
    bool updateNode(const std::string& node_id, bool success, std::chrono::milliseconds response_time);
    
    /**
     * Get closest nodes to a target
     * @param target_id Target node ID
     * @param count Number of nodes to return
     * @return Vector of closest node information
     */
    std::vector<std::shared_ptr<RoutingTableManager::DHTNode>> getClosestNodes(const std::string& target_id, int count = 8);
    
    /**
     * Get good nodes from the routing table
     * @param count Number of nodes to return
     * @return Vector of good node information
     */
    std::vector<std::shared_ptr<RoutingTableManager::DHTNode>> getGoodNodes(int count = 8);
    
    /**
     * Get routing table statistics
     * @return Current routing table statistics
     */
    RoutingTableManager::RoutingTableStatistics getStatistics();
    
    /**
     * Get routing table size
     * @return Total number of nodes in routing table
     */
    size_t getRoutingTableSize();
    
    /**
     * Check if node exists in routing table
     * @param node_id Node ID
     * @return true if node exists
     */
    bool hasNode(const std::string& node_id);
    
    /**
     * Get node information
     * @param node_id Node ID
     * @return Node information or nullptr if not found
     */
    std::shared_ptr<RoutingTableManager::DHTNode> getNode(const std::string& node_id);
};

/**
 * Routing table analyzer for pattern detection and optimization
 */
class RoutingTableAnalyzer {
private:
    std::shared_ptr<RoutingTableManager> manager_;
    
public:
    RoutingTableAnalyzer(std::shared_ptr<RoutingTableManager> manager);
    
    /**
     * Analyze routing table patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzeRoutingTablePatterns();
    
    /**
     * Detect routing table issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectRoutingTableIssues();
    
    /**
     * Analyze node distribution
     * @return Node distribution analysis
     */
    std::map<std::string, int> analyzeNodeDistribution();
    
    /**
     * Get routing table optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getRoutingTableOptimizationRecommendations();
    
    /**
     * Analyze routing table performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzeRoutingTablePerformanceTrends();
    
    /**
     * Get routing table health score
     * @return Health score (0.0 to 1.0)
     */
    double getRoutingTableHealthScore();
    
    /**
     * Get bucket efficiency analysis
     * @return Bucket efficiency analysis
     */
    std::map<std::string, double> getBucketEfficiencyAnalysis();
    
    /**
     * Get node quality distribution
     * @return Node quality distribution
     */
    std::map<std::string, int> getNodeQualityDistribution();
    
    /**
     * Get routing table capacity analysis
     * @return Capacity analysis
     */
    std::map<std::string, double> getRoutingTableCapacityAnalysis();
};

} // namespace dht_crawler
