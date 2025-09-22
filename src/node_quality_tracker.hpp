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

namespace dht_crawler {

/**
 * Node quality tracker for DHT node status management
 * Implements Tribler-inspired node status tracking (GOOD/UNKNOWN/BAD) with RTT calculation
 * Reference: py-ipv8/dht/routing.py - Node Status
 */
class NodeQualityTracker {
public:
    enum class NodeStatus {
        GOOD,        // Node is responding well
        UNKNOWN,     // Node status is unknown
        BAD,         // Node is not responding well
        BLACKLISTED  // Node is blacklisted
    };

    enum class QueryType {
        PING,
        FIND_NODE,
        GET_PEERS,
        ANNOUNCE_PEER,
        GET,
        PUT,
        CUSTOM
    };

    struct NodeConfig {
        int max_queries_per_node = 10;           // NODE_LIMIT_QUERIES = 10
        std::chrono::milliseconds ping_interval{25000}; // PING_INTERVAL = 25s
        std::chrono::milliseconds response_timeout{10000}; // 10 seconds
        double good_threshold = 0.8;             // Threshold for GOOD status
        double bad_threshold = 0.3;              // Threshold for BAD status
        int min_responses_for_evaluation = 5;    // Minimum responses for status evaluation
        bool enable_blacklisting = true;         // Enable node blacklisting
        std::chrono::hours blacklist_duration{24}; // Blacklist duration
        bool enable_quality_scoring = true;      // Enable quality scoring
        double quality_decay_rate = 0.1;         // Quality decay rate per hour
    };

    struct NodeInfo {
        std::string node_id;
        std::string node_ip;
        int node_port;
        NodeStatus status;
        double quality_score;
        std::chrono::steady_clock::time_point first_seen;
        std::chrono::steady_clock::time_point last_seen;
        std::chrono::steady_clock::time_point last_ping;
        std::chrono::steady_clock::time_point last_response;
        
        // Query statistics
        int total_queries;
        int successful_queries;
        int failed_queries;
        int timeout_queries;
        
        // Response time tracking
        std::chrono::milliseconds avg_response_time;
        std::chrono::milliseconds min_response_time;
        std::chrono::milliseconds max_response_time;
        std::vector<std::chrono::milliseconds> recent_response_times;
        
        // Query type statistics
        std::map<QueryType, int> queries_by_type;
        std::map<QueryType, int> successful_queries_by_type;
        std::map<QueryType, std::chrono::milliseconds> avg_response_time_by_type;
        
        // Quality metrics
        double success_rate;
        double response_time_score;
        double consistency_score;
        double reliability_score;
        
        // Blacklist information
        bool is_blacklisted;
        std::chrono::steady_clock::time_point blacklisted_at;
        std::string blacklist_reason;
        int blacklist_count;
        
        // Bucket information
        int bucket_distance;
        int bucket_index;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct QualityMetrics {
        int total_nodes = 0;
        int good_nodes = 0;
        int unknown_nodes = 0;
        int bad_nodes = 0;
        int blacklisted_nodes = 0;
        double avg_quality_score = 0.0;
        double avg_response_time_ms = 0.0;
        double avg_success_rate = 0.0;
        std::map<NodeStatus, int> nodes_by_status;
        std::map<QueryType, int> total_queries_by_type;
        std::map<QueryType, double> success_rate_by_type;
        std::map<std::string, int> nodes_by_ip;
        std::chrono::steady_clock::time_point last_update;
    };

private:
    NodeConfig config_;
    std::map<std::string, std::shared_ptr<NodeInfo>> node_registry_;
    std::mutex registry_mutex_;
    
    // Quality scoring
    std::map<std::string, double> quality_scores_;
    std::mutex quality_mutex_;
    
    // Blacklist management
    std::map<std::string, std::chrono::steady_clock::time_point> blacklist_;
    std::mutex blacklist_mutex_;
    
    // Background processing
    std::thread maintenance_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable maintenance_condition_;
    
    QualityMetrics metrics_;
    std::mutex metrics_mutex_;
    
    // Internal methods
    void maintenanceLoop();
    void updateNodeQuality(const std::string& node_id);
    void updateQualityMetrics();
    void cleanupExpiredNodes();
    void processBlacklist();
    double calculateQualityScore(const NodeInfo& node_info);
    double calculateSuccessRate(const NodeInfo& node_info);
    double calculateResponseTimeScore(const NodeInfo& node_info);
    double calculateConsistencyScore(const NodeInfo& node_info);
    double calculateReliabilityScore(const NodeInfo& node_info);
    NodeStatus determineNodeStatus(const NodeInfo& node_info);
    void addToBlacklist(const std::string& node_id, const std::string& reason);
    void removeFromBlacklist(const std::string& node_id);
    bool isBlacklisted(const std::string& node_id);
    void updateRecentResponseTimes(NodeInfo& node_info, std::chrono::milliseconds response_time);

public:
    NodeQualityTracker(const NodeConfig& config = NodeConfig{});
    ~NodeQualityTracker();
    
    /**
     * Register a new node
     * @param node_id Node ID
     * @param node_ip Node IP address
     * @param node_port Node port
     * @return true if node was registered successfully
     */
    bool registerNode(const std::string& node_id, 
                     const std::string& node_ip, 
                     int node_port);
    
    /**
     * Update node information after a query
     * @param node_id Node ID
     * @param query_type Type of query
     * @param success Whether query was successful
     * @param response_time Response time
     * @return true if update was successful
     */
    bool updateNodeQuery(const std::string& node_id,
                        QueryType query_type,
                        bool success,
                        std::chrono::milliseconds response_time);
    
    /**
     * Update node ping information
     * @param node_id Node ID
     * @param success Whether ping was successful
     * @param response_time Response time
     * @return true if update was successful
     */
    bool updateNodePing(const std::string& node_id,
                       bool success,
                       std::chrono::milliseconds response_time);
    
    /**
     * Get node information
     * @param node_id Node ID
     * @return Node information or nullptr if not found
     */
    std::shared_ptr<NodeInfo> getNodeInfo(const std::string& node_id);
    
    /**
     * Get nodes by status
     * @param status Node status
     * @return Vector of node information
     */
    std::vector<std::shared_ptr<NodeInfo>> getNodesByStatus(NodeStatus status);
    
    /**
     * Get good nodes
     * @return Vector of good node information
     */
    std::vector<std::shared_ptr<NodeInfo>> getGoodNodes();
    
    /**
     * Get unknown nodes
     * @return Vector of unknown node information
     */
    std::vector<std::shared_ptr<NodeInfo>> getUnknownNodes();
    
    /**
     * Get bad nodes
     * @return Vector of bad node information
     */
    std::vector<std::shared_ptr<NodeInfo>> getBadNodes();
    
    /**
     * Get blacklisted nodes
     * @return Vector of blacklisted node information
     */
    std::vector<std::shared_ptr<NodeInfo>> getBlacklistedNodes();
    
    /**
     * Get nodes by quality score range
     * @param min_score Minimum quality score
     * @param max_score Maximum quality score
     * @return Vector of node information
     */
    std::vector<std::shared_ptr<NodeInfo>> getNodesByQualityRange(double min_score, double max_score);
    
    /**
     * Get top quality nodes
     * @param count Number of nodes to return
     * @return Vector of top quality node information
     */
    std::vector<std::shared_ptr<NodeInfo>> getTopQualityNodes(int count = 10);
    
    /**
     * Get nodes that need pinging
     * @return Vector of node IDs that need pinging
     */
    std::vector<std::string> getNodesNeedingPing();
    
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
     * Check if node is blacklisted
     * @param node_id Node ID
     * @return true if node is blacklisted
     */
    bool isNodeBlacklisted(const std::string& node_id);
    
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
     * Get node success rate
     * @param node_id Node ID
     * @return Success rate or 0.0 if not found
     */
    double getNodeSuccessRate(const std::string& node_id);
    
    /**
     * Get node average response time
     * @param node_id Node ID
     * @return Average response time or 0ms if not found
     */
    std::chrono::milliseconds getNodeAvgResponseTime(const std::string& node_id);
    
    /**
     * Blacklist a node
     * @param node_id Node ID
     * @param reason Blacklist reason
     * @return true if node was blacklisted successfully
     */
    bool blacklistNode(const std::string& node_id, const std::string& reason);
    
    /**
     * Remove node from blacklist
     * @param node_id Node ID
     * @return true if node was removed from blacklist successfully
     */
    bool unblacklistNode(const std::string& node_id);
    
    /**
     * Remove a node from tracking
     * @param node_id Node ID
     * @return true if node was removed successfully
     */
    bool removeNode(const std::string& node_id);
    
    /**
     * Get quality metrics
     * @return Current quality metrics
     */
    QualityMetrics getQualityMetrics();
    
    /**
     * Reset quality metrics
     */
    void resetQualityMetrics();
    
    /**
     * Update node configuration
     * @param config New node configuration
     */
    void updateConfig(const NodeConfig& config);
    
    /**
     * Get node count
     * @return Total number of tracked nodes
     */
    int getNodeCount();
    
    /**
     * Get node count by status
     * @param status Node status
     * @return Number of nodes with the status
     */
    int getNodeCount(NodeStatus status);
    
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
     * Get average success rate
     * @return Average success rate
     */
    double getAverageSuccessRate();
    
    /**
     * Start node quality tracker
     */
    void start();
    
    /**
     * Stop node quality tracker
     */
    void stop();
    
    /**
     * Check if tracker is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Force quality update for all nodes
     */
    void forceQualityUpdate();
    
    /**
     * Force maintenance cycle
     */
    void forceMaintenance();
    
    /**
     * Export node data to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportNodeData(const std::string& filename);
    
    /**
     * Import node data from file
     * @param filename Input filename
     * @return true if import was successful
     */
    bool importNodeData(const std::string& filename);
    
    /**
     * Get tracker health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear all node data
     */
    void clearAllNodes();
    
    /**
     * Clear expired nodes
     * @return Number of nodes cleared
     */
    int clearExpiredNodes();
    
    /**
     * Get node statistics by IP
     * @return Map of IP addresses to node counts
     */
    std::map<std::string, int> getNodeStatisticsByIP();
    
    /**
     * Get query statistics by type
     * @return Map of query types to counts
     */
    std::map<QueryType, int> getQueryStatisticsByType();
    
    /**
     * Get success rate by query type
     * @return Map of query types to success rates
     */
    std::map<QueryType, double> getSuccessRateByQueryType();
};

/**
 * Node quality tracker factory for different tracking strategies
 */
class NodeQualityTrackerFactory {
public:
    enum class TrackingStrategy {
        AGGRESSIVE,  // Aggressive quality tracking with strict thresholds
        MODERATE,    // Moderate quality tracking with balanced thresholds
        LENIENT,     // Lenient quality tracking with relaxed thresholds
        CUSTOM       // Custom tracking configuration
    };
    
    static std::unique_ptr<NodeQualityTracker> createTracker(
        TrackingStrategy strategy,
        const NodeQualityTracker::NodeConfig& config = NodeQualityTracker::NodeConfig{}
    );
    
    static NodeQualityTracker::NodeConfig getDefaultConfig(TrackingStrategy strategy);
};

/**
 * RAII node tracking wrapper for automatic node management
 */
class NodeTrackingGuard {
private:
    std::shared_ptr<NodeQualityTracker> tracker_;
    std::string node_id_;
    bool registered_;

public:
    NodeTrackingGuard(std::shared_ptr<NodeQualityTracker> tracker,
                     const std::string& node_id,
                     const std::string& node_ip,
                     int node_port);
    
    ~NodeTrackingGuard();
    
    /**
     * Update node query
     * @param query_type Query type
     * @param success Whether query was successful
     * @param response_time Response time
     */
    void updateQuery(NodeQualityTracker::QueryType query_type,
                    bool success,
                    std::chrono::milliseconds response_time);
    
    /**
     * Update node ping
     * @param success Whether ping was successful
     * @param response_time Response time
     */
    void updatePing(bool success, std::chrono::milliseconds response_time);
    
    /**
     * Get node information
     * @return Node information
     */
    std::shared_ptr<NodeQualityTracker::NodeInfo> getNodeInfo();
    
    /**
     * Check if node is good
     * @return true if node is good
     */
    bool isGood();
    
    /**
     * Check if node is bad
     * @return true if node is bad
     */
    bool isBad();
    
    /**
     * Get quality score
     * @return Quality score
     */
    double getQualityScore();
};

/**
 * Node quality analyzer for pattern detection
 */
class NodeQualityAnalyzer {
private:
    std::shared_ptr<NodeQualityTracker> tracker_;
    
public:
    NodeQualityAnalyzer(std::shared_ptr<NodeQualityTracker> tracker);
    
    /**
     * Analyze node quality patterns
     * @return Map of quality patterns and their frequencies
     */
    std::map<std::string, int> analyzeQualityPatterns();
    
    /**
     * Detect quality clusters
     * @param time_window Time window for clustering
     * @return Vector of quality clusters
     */
    std::vector<std::vector<std::string>> detectQualityClusters(
        std::chrono::minutes time_window = std::chrono::minutes(10)
    );
    
    /**
     * Predict node quality trends
     * @return Map of node IDs to predicted quality scores
     */
    std::map<std::string, double> predictQualityTrends();
    
    /**
     * Get quality trend analysis
     * @param time_period Time period for trend analysis
     * @return Trend analysis results
     */
    std::map<std::string, double> getQualityTrends(
        std::chrono::hours time_period = std::chrono::hours(24)
    );
    
    /**
     * Get node performance ranking
     * @return Vector of node IDs ranked by performance
     */
    std::vector<std::string> getNodePerformanceRanking();
    
    /**
     * Get quality distribution statistics
     * @return Quality distribution statistics
     */
    std::map<std::string, int> getQualityDistribution();
};

} // namespace dht_crawler
