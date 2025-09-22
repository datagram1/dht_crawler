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
#include <random>

namespace dht_crawler {

/**
 * Enhanced bootstrap process using magnetico's approach
 * Provides bootstrap node list, retry logic, and bootstrap success validation
 */
class EnhancedBootstrap {
public:
    enum class BootstrapState {
        NOT_STARTED,        // Bootstrap not started
        CONNECTING,         // Connecting to bootstrap nodes
        BOOTSTRAPPING,      // Bootstrapping in progress
        VALIDATING,         // Validating bootstrap success
        COMPLETED,          // Bootstrap completed successfully
        FAILED,             // Bootstrap failed
        TIMEOUT,            // Bootstrap timed out
        RETRYING             // Retrying bootstrap
    };

    enum class BootstrapNodeType {
        PRIMARY,            // Primary bootstrap nodes
        SECONDARY,          // Secondary bootstrap nodes
        FALLBACK,           // Fallback bootstrap nodes
        CUSTOM              // Custom bootstrap nodes
    };

    struct BootstrapConfig {
        std::chrono::milliseconds connection_timeout{10000};     // 10 seconds
        std::chrono::milliseconds bootstrap_timeout{60000};      // 1 minute
        std::chrono::milliseconds validation_timeout{30000};    // 30 seconds
        int max_retry_attempts = 3;                            // Max retry attempts
        std::chrono::milliseconds retry_delay{2000};           // 2 seconds
        double retry_backoff_multiplier = 2.0;                 // Retry backoff multiplier
        bool enable_parallel_bootstrap = true;                 // Enable parallel bootstrap
        int max_parallel_connections = 5;                       // Max parallel connections
        bool enable_fallback_nodes = true;                     // Enable fallback nodes
        bool enable_validation = true;                          // Enable bootstrap validation
        int min_successful_nodes = 3;                           // Minimum successful nodes
        bool enable_node_rotation = true;                      // Enable node rotation
        std::chrono::hours node_rotation_interval{24};         // Node rotation interval
    };

    struct BootstrapNode {
        std::string node_id;
        std::string host;
        int port;
        BootstrapNodeType type;
        bool is_active;
        std::chrono::steady_clock::time_point last_used;
        std::chrono::steady_clock::time_point last_success;
        int connection_attempts;
        int successful_connections;
        int failed_connections;
        std::chrono::milliseconds avg_response_time;
        double success_rate;
        std::string error_message;
        std::map<std::string, std::string> metadata;
    };

    struct BootstrapSession {
        std::string session_id;
        BootstrapState state;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        
        // Bootstrap nodes
        std::vector<BootstrapNode> primary_nodes;
        std::vector<BootstrapNode> secondary_nodes;
        std::vector<BootstrapNode> fallback_nodes;
        std::vector<BootstrapNode> custom_nodes;
        
        // Connection tracking
        std::map<std::string, bool> node_connections;
        std::map<std::string, std::chrono::milliseconds> node_response_times;
        std::map<std::string, std::string> node_errors;
        
        // Success tracking
        int total_nodes_contacted;
        int successful_nodes;
        int failed_nodes;
        int timeout_nodes;
        
        // Validation
        bool validation_passed;
        std::string validation_message;
        std::vector<std::string> validation_errors;
        
        // Statistics
        std::chrono::milliseconds total_duration;
        std::chrono::milliseconds avg_node_time;
        std::chrono::milliseconds max_node_time;
        std::chrono::milliseconds min_node_time;
        
        // Error tracking
        std::vector<std::string> session_errors;
        std::chrono::steady_clock::time_point last_error_time;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct BootstrapStatistics {
        int total_sessions = 0;
        int successful_sessions = 0;
        int failed_sessions = 0;
        int timeout_sessions = 0;
        int retry_sessions = 0;
        std::chrono::milliseconds avg_session_duration;
        std::chrono::milliseconds max_session_duration;
        std::chrono::milliseconds min_session_duration;
        std::map<BootstrapState, int> sessions_by_state;
        std::map<BootstrapNodeType, int> nodes_by_type;
        std::map<std::string, int> nodes_by_host;
        double overall_success_rate = 0.0;
        double node_success_rate = 0.0;
        double validation_success_rate = 0.0;
        int total_nodes_contacted = 0;
        int total_successful_nodes = 0;
        int total_failed_nodes = 0;
        std::chrono::steady_clock::time_point last_update;
    };

private:
    BootstrapConfig config_;
    std::map<std::string, std::shared_ptr<BootstrapSession>> active_sessions_;
    std::mutex sessions_mutex_;
    
    std::map<std::string, std::vector<BootstrapSession>> session_history_;
    std::mutex history_mutex_;
    
    std::vector<BootstrapNode> bootstrap_nodes_;
    std::mutex nodes_mutex_;
    
    BootstrapStatistics stats_;
    std::mutex stats_mutex_;
    
    // Background processing
    std::thread bootstrap_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Random number generation
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<> dis_;
    
    // Internal methods
    void bootstrapMonitorLoop();
    void monitorSessions();
    void cleanupExpiredSessions();
    void updateStatistics(const BootstrapSession& session, bool success);
    std::string generateSessionId();
    std::string bootstrapStateToString(BootstrapState state);
    std::string bootstrapNodeTypeToString(BootstrapNodeType type);
    void initializeBootstrapNodes();
    BootstrapNode createBootstrapNode(const std::string& host, int port, BootstrapNodeType type);
    bool connectToBootstrapNode(const BootstrapNode& node, std::chrono::milliseconds timeout);
    bool validateBootstrapSuccess(const BootstrapSession& session);
    void rotateBootstrapNodes();
    void updateNodeStatistics(BootstrapNode& node, bool success, std::chrono::milliseconds response_time);
    std::vector<BootstrapNode> selectBootstrapNodes(int count, BootstrapNodeType type);
    std::vector<BootstrapNode> getRandomBootstrapNodes(int count);
    bool isBootstrapNodeAvailable(const BootstrapNode& node);
    void markBootstrapNodeAsUsed(BootstrapNode& node);
    void resetBootstrapNode(BootstrapNode& node);

public:
    EnhancedBootstrap(const BootstrapConfig& config = BootstrapConfig{});
    ~EnhancedBootstrap();
    
    /**
     * Start bootstrap process
     * @return Session ID
     */
    std::string startBootstrap();
    
    /**
     * Start bootstrap process with custom nodes
     * @param custom_nodes Custom bootstrap nodes
     * @return Session ID
     */
    std::string startBootstrapWithCustomNodes(const std::vector<BootstrapNode>& custom_nodes);
    
    /**
     * Start bootstrap process with specific node types
     * @param primary_count Number of primary nodes to use
     * @param secondary_count Number of secondary nodes to use
     * @param fallback_count Number of fallback nodes to use
     * @return Session ID
     */
    std::string startBootstrapWithNodeTypes(int primary_count, int secondary_count, int fallback_count);
    
    /**
     * Process bootstrap node connection result
     * @param session_id Session ID
     * @param node_id Node ID
     * @param success Whether connection was successful
     * @param response_time Response time
     * @param error_message Error message (if failed)
     * @return true if result was processed successfully
     */
    bool processBootstrapNodeResult(const std::string& session_id,
                                   const std::string& node_id,
                                   bool success,
                                   std::chrono::milliseconds response_time,
                                   const std::string& error_message = "");
    
    /**
     * Complete bootstrap session
     * @param session_id Session ID
     * @param success Whether bootstrap was successful
     * @param validation_passed Whether validation passed
     * @param validation_message Validation message
     * @return true if session was completed successfully
     */
    bool completeBootstrapSession(const std::string& session_id,
                                bool success,
                                bool validation_passed,
                                const std::string& validation_message = "");
    
    /**
     * Cancel bootstrap session
     * @param session_id Session ID
     * @return true if session was cancelled successfully
     */
    bool cancelBootstrapSession(const std::string& session_id);
    
    /**
     * Get session information
     * @param session_id Session ID
     * @return Session information or nullptr if not found
     */
    std::shared_ptr<BootstrapSession> getSession(const std::string& session_id);
    
    /**
     * Get sessions by state
     * @param state Bootstrap state
     * @return Vector of session information
     */
    std::vector<std::shared_ptr<BootstrapSession>> getSessionsByState(BootstrapState state);
    
    /**
     * Get active sessions
     * @return Vector of active session information
     */
    std::vector<std::shared_ptr<BootstrapSession>> getActiveSessions();
    
    /**
     * Get session history
     * @return Vector of historical session information
     */
    std::vector<BootstrapSession> getSessionHistory();
    
    /**
     * Check if session is active
     * @param session_id Session ID
     * @return true if session is active
     */
    bool isSessionActive(const std::string& session_id);
    
    /**
     * Check if session is completed
     * @param session_id Session ID
     * @return true if session is completed
     */
    bool isSessionCompleted(const std::string& session_id);
    
    /**
     * Get session progress
     * @param session_id Session ID
     * @return Progress percentage (0.0 to 1.0)
     */
    double getSessionProgress(const std::string& session_id);
    
    /**
     * Get bootstrap nodes
     * @param type Node type (optional)
     * @return Vector of bootstrap nodes
     */
    std::vector<BootstrapNode> getBootstrapNodes(BootstrapNodeType type = BootstrapNodeType::PRIMARY);
    
    /**
     * Add custom bootstrap node
     * @param host Node host
     * @param port Node port
     * @param type Node type
     * @return true if node was added successfully
     */
    bool addBootstrapNode(const std::string& host, int port, BootstrapNodeType type = BootstrapNodeType::CUSTOM);
    
    /**
     * Remove bootstrap node
     * @param host Node host
     * @param port Node port
     * @return true if node was removed successfully
     */
    bool removeBootstrapNode(const std::string& host, int port);
    
    /**
     * Update bootstrap node
     * @param host Node host
     * @param port Node port
     * @param node Updated node information
     * @return true if node was updated successfully
     */
    bool updateBootstrapNode(const std::string& host, int port, const BootstrapNode& node);
    
    /**
     * Get bootstrap statistics
     * @return Current bootstrap statistics
     */
    BootstrapStatistics getStatistics();
    
    /**
     * Reset bootstrap statistics
     */
    void resetStatistics();
    
    /**
     * Update bootstrap configuration
     * @param config New bootstrap configuration
     */
    void updateConfig(const BootstrapConfig& config);
    
    /**
     * Enable or disable parallel bootstrap
     * @param enable true to enable parallel bootstrap
     */
    void setParallelBootstrapEnabled(bool enable);
    
    /**
     * Check if parallel bootstrap is enabled
     * @return true if parallel bootstrap is enabled
     */
    bool isParallelBootstrapEnabled();
    
    /**
     * Set maximum parallel connections
     * @param max_connections Maximum parallel connections
     */
    void setMaxParallelConnections(int max_connections);
    
    /**
     * Get maximum parallel connections
     * @return Maximum parallel connections
     */
    int getMaxParallelConnections();
    
    /**
     * Enable or disable fallback nodes
     * @param enable true to enable fallback nodes
     */
    void setFallbackNodesEnabled(bool enable);
    
    /**
     * Check if fallback nodes are enabled
     * @return true if fallback nodes are enabled
     */
    bool isFallbackNodesEnabled();
    
    /**
     * Enable or disable validation
     * @param enable true to enable validation
     */
    void setValidationEnabled(bool enable);
    
    /**
     * Check if validation is enabled
     * @return true if validation is enabled
     */
    bool isValidationEnabled();
    
    /**
     * Get bootstrap success rate
     * @return Bootstrap success rate (0.0 to 1.0)
     */
    double getBootstrapSuccessRate();
    
    /**
     * Get node success rate
     * @return Node success rate (0.0 to 1.0)
     */
    double getNodeSuccessRate();
    
    /**
     * Get validation success rate
     * @return Validation success rate (0.0 to 1.0)
     */
    double getValidationSuccessRate();
    
    /**
     * Get average session duration
     * @return Average session duration
     */
    std::chrono::milliseconds getAverageSessionDuration();
    
    /**
     * Get session count
     * @return Total number of active sessions
     */
    int getSessionCount();
    
    /**
     * Get session count by state
     * @param state Bootstrap state
     * @return Number of sessions with the state
     */
    int getSessionCount(BootstrapState state);
    
    /**
     * Get bootstrap node count
     * @param type Node type (optional)
     * @return Number of bootstrap nodes
     */
    int getBootstrapNodeCount(BootstrapNodeType type = BootstrapNodeType::PRIMARY);
    
    /**
     * Start bootstrap monitor
     */
    void start();
    
    /**
     * Stop bootstrap monitor
     */
    void stop();
    
    /**
     * Check if bootstrap is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export session data to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportSessionData(const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get bootstrap health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear session history
     */
    void clearSessionHistory();
    
    /**
     * Clear active sessions
     */
    void clearActiveSessions();
    
    /**
     * Force cleanup of expired sessions
     */
    void forceCleanup();
    
    /**
     * Get bootstrap node statistics
     * @param host Node host
     * @param port Node port
     * @return Node statistics
     */
    std::map<std::string, int> getBootstrapNodeStatistics(const std::string& host, int port);
    
    /**
     * Get bootstrap node quality score
     * @param host Node host
     * @param port Node port
     * @return Quality score (0.0 to 1.0)
     */
    double getBootstrapNodeQualityScore(const std::string& host, int port);
    
    /**
     * Get bootstrap node reliability score
     * @param host Node host
     * @param port Node port
     * @return Reliability score (0.0 to 1.0)
     */
    double getBootstrapNodeReliabilityScore(const std::string& host, int port);
    
    /**
     * Test bootstrap with a specific node
     * @param host Node host
     * @param port Node port
     * @param timeout Test timeout
     * @return true if bootstrap test was successful
     */
    bool testBootstrapNode(const std::string& host,
                          int port,
                          std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
    
    /**
     * Get bootstrap recommendations
     * @return Vector of bootstrap recommendations
     */
    std::vector<std::string> getBootstrapRecommendations();
    
    /**
     * Optimize bootstrap nodes based on performance data
     */
    void optimizeBootstrapNodes();
    
    /**
     * Get bootstrap node performance comparison
     * @return Performance comparison data
     */
    std::map<std::string, double> getBootstrapNodePerformanceComparison();
    
    /**
     * Force node rotation
     */
    void forceNodeRotation();
    
    /**
     * Get next available bootstrap node
     * @param type Node type
     * @return Next available bootstrap node
     */
    BootstrapNode getNextBootstrapNode(BootstrapNodeType type = BootstrapNodeType::PRIMARY);
    
    /**
     * Get random bootstrap node
     * @param type Node type
     * @return Random bootstrap node
     */
    BootstrapNode getRandomBootstrapNode(BootstrapNodeType type = BootstrapNodeType::PRIMARY);
};

/**
 * Enhanced bootstrap factory for different bootstrap strategies
 */
class EnhancedBootstrapFactory {
public:
    enum class BootstrapStrategy {
        AGGRESSIVE,      // Aggressive bootstrap with fast timeouts
        RELIABLE,        // Reliable bootstrap with conservative timeouts
        BALANCED,        // Balanced bootstrap with moderate timeouts
        CUSTOM           // Custom bootstrap configuration
    };
    
    static std::unique_ptr<EnhancedBootstrap> createBootstrap(
        BootstrapStrategy strategy,
        const EnhancedBootstrap::BootstrapConfig& config = EnhancedBootstrap::BootstrapConfig{}
    );
    
    static EnhancedBootstrap::BootstrapConfig getDefaultConfig(BootstrapStrategy strategy);
};

/**
 * RAII bootstrap session wrapper for automatic session management
 */
class BootstrapSessionGuard {
private:
    std::shared_ptr<EnhancedBootstrap> bootstrap_;
    std::string session_id_;
    bool completed_;

public:
    BootstrapSessionGuard(std::shared_ptr<EnhancedBootstrap> bootstrap);
    
    ~BootstrapSessionGuard();
    
    /**
     * Process bootstrap node result
     * @param node_id Node ID
     * @param success Whether connection was successful
     * @param response_time Response time
     * @param error_message Error message
     */
    void processNodeResult(const std::string& node_id,
                          bool success,
                          std::chrono::milliseconds response_time,
                          const std::string& error_message = "");
    
    /**
     * Complete bootstrap session
     * @param success Whether bootstrap was successful
     * @param validation_passed Whether validation passed
     * @param validation_message Validation message
     */
    void complete(bool success,
                 bool validation_passed,
                 const std::string& validation_message = "");
    
    /**
     * Cancel bootstrap session
     */
    void cancel();
    
    /**
     * Get session ID
     * @return Session ID
     */
    const std::string& getSessionId() const;
    
    /**
     * Get session information
     * @return Session information
     */
    std::shared_ptr<EnhancedBootstrap::BootstrapSession> getSessionInfo();
    
    /**
     * Check if session is completed
     * @return true if completed
     */
    bool isCompleted();
    
    /**
     * Check if session is active
     * @return true if active
     */
    bool isActive();
    
    /**
     * Get session progress
     * @return Progress percentage (0.0 to 1.0)
     */
    double getProgress();
};

/**
 * Bootstrap analyzer for pattern detection and optimization
 */
class BootstrapAnalyzer {
private:
    std::shared_ptr<EnhancedBootstrap> bootstrap_;
    
public:
    BootstrapAnalyzer(std::shared_ptr<EnhancedBootstrap> bootstrap);
    
    /**
     * Analyze bootstrap patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzeBootstrapPatterns();
    
    /**
     * Detect bootstrap issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectBootstrapIssues();
    
    /**
     * Analyze bootstrap node performance
     * @return Map of node hosts to performance scores
     */
    std::map<std::string, double> analyzeBootstrapNodePerformance();
    
    /**
     * Get bootstrap optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getBootstrapOptimizationRecommendations();
    
    /**
     * Analyze bootstrap performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzeBootstrapPerformanceTrends();
    
    /**
     * Get bootstrap health score
     * @return Health score (0.0 to 1.0)
     */
    double getBootstrapHealthScore();
    
    /**
     * Get bootstrap node efficiency analysis
     * @return Efficiency analysis data
     */
    std::map<std::string, double> getBootstrapNodeEfficiencyAnalysis();
    
    /**
     * Get bootstrap reliability analysis
     * @return Reliability analysis data
     */
    std::map<std::string, double> getBootstrapReliabilityAnalysis();
};

} // namespace dht_crawler
