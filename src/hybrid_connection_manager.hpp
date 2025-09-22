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
#include "direct_peer_connector.hpp"
#include "bittorrent_protocol.hpp"
#include "ut_metadata_protocol.hpp"

namespace dht_crawler {

/**
 * Hybrid connection manager to manage both libtorrent and direct connection types
 * Provides connection strategy selection, fallback mechanisms, and performance tracking
 */
class HybridConnectionManager {
public:
    enum class ConnectionStrategy {
        LIBTORRENT_FIRST,    // Try libtorrent first, fallback to direct
        DIRECT_FIRST,        // Try direct first, fallback to libtorrent
        PERFORMANCE_BASED,   // Choose based on performance metrics
        LOAD_BALANCED,       // Load balance between connection types
        CUSTOM               // Custom strategy
    };

    enum class ConnectionType {
        LIBTORRENT,          // libtorrent connection
        DIRECT_TCP,          // Direct TCP connection
        HYBRID               // Hybrid connection (both types)
    };

    enum class RequestStatus {
        PENDING,             // Request pending
        IN_PROGRESS,         // Request in progress
        SUCCESS,             // Request successful
        FAILED,              // Request failed
        TIMEOUT,             // Request timed out
        CANCELLED             // Request cancelled
    };

    struct HybridConfig {
        ConnectionStrategy default_strategy = ConnectionStrategy::LIBTORRENT_FIRST;
        std::chrono::milliseconds libtorrent_timeout{30000};     // 30 seconds
        std::chrono::milliseconds direct_timeout{30000};         // 30 seconds
        std::chrono::milliseconds fallback_delay{1000};          // 1 second
        bool enable_performance_tracking = true;                // Enable performance tracking
        bool enable_load_balancing = true;                     // Enable load balancing
        bool enable_automatic_fallback = true;                 // Enable automatic fallback
        int max_concurrent_requests = 10;                      // Max concurrent requests
        double performance_threshold = 0.8;                   // Performance threshold for strategy selection
        std::chrono::minutes performance_window{5};            // Performance tracking window
        bool enable_connection_pooling = true;                 // Enable connection pooling
        int max_connections_per_peer = 3;                       // Max connections per peer
    };

    struct ConnectionRequest {
        std::string request_id;
        std::string peer_ip;
        int peer_port;
        std::string peer_id;
        std::string info_hash;
        ConnectionType preferred_type;
        ConnectionStrategy strategy;
        RequestStatus status;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        
        // Connection information
        std::string libtorrent_connection_id;
        std::string direct_connection_id;
        std::string active_connection_id;
        ConnectionType active_type;
        
        // Performance metrics
        std::chrono::milliseconds connection_time;
        std::chrono::milliseconds metadata_time;
        size_t bytes_transferred;
        int messages_exchanged;
        
        // Fallback information
        bool fallback_attempted;
        ConnectionType fallback_type;
        std::string fallback_reason;
        
        // Error tracking
        std::vector<std::string> errors;
        std::chrono::steady_clock::time_point last_error_time;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct PerformanceMetrics {
        // libtorrent metrics
        int libtorrent_requests = 0;
        int libtorrent_successes = 0;
        int libtorrent_failures = 0;
        std::chrono::milliseconds libtorrent_avg_time;
        std::chrono::milliseconds libtorrent_max_time;
        std::chrono::milliseconds libtorrent_min_time;
        
        // Direct connection metrics
        int direct_requests = 0;
        int direct_successes = 0;
        int direct_failures = 0;
        std::chrono::milliseconds direct_avg_time;
        std::chrono::milliseconds direct_max_time;
        std::chrono::milliseconds direct_min_time;
        
        // Hybrid metrics
        int hybrid_requests = 0;
        int hybrid_successes = 0;
        int hybrid_failures = 0;
        int fallback_attempts = 0;
        int fallback_successes = 0;
        
        // Overall metrics
        double libtorrent_success_rate = 0.0;
        double direct_success_rate = 0.0;
        double hybrid_success_rate = 0.0;
        double overall_success_rate = 0.0;
        
        // Load balancing metrics
        std::map<std::string, int> requests_by_peer;
        std::map<ConnectionType, int> requests_by_type;
        std::map<ConnectionStrategy, int> requests_by_strategy;
        
        // Performance trends
        std::map<std::string, double> performance_trends;
        std::chrono::steady_clock::time_point last_update;
    };

private:
    HybridConfig config_;
    std::map<std::string, std::shared_ptr<ConnectionRequest>> active_requests_;
    std::mutex requests_mutex_;
    
    std::map<std::string, std::vector<ConnectionRequest>> request_history_;
    std::mutex history_mutex_;
    
    PerformanceMetrics metrics_;
    std::mutex metrics_mutex_;
    
    // Component managers
    std::shared_ptr<DirectPeerConnector> direct_connector_;
    std::shared_ptr<BitTorrentProtocol> bittorrent_protocol_;
    std::shared_ptr<UTMetadataProtocol> metadata_protocol_;
    
    // Background processing
    std::thread hybrid_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Internal methods
    void hybridMonitorLoop();
    void monitorRequests();
    void cleanupExpiredRequests();
    void updatePerformanceMetrics(const ConnectionRequest& request, bool success);
    std::string generateRequestId();
    std::string connectionTypeToString(ConnectionType type);
    std::string connectionStrategyToString(ConnectionStrategy strategy);
    std::string requestStatusToString(RequestStatus status);
    ConnectionStrategy determineStrategy(const std::string& peer_ip, int peer_port);
    ConnectionType selectConnectionType(ConnectionStrategy strategy, const std::string& peer_ip, int peer_port);
    bool attemptLibtorrentConnection(std::shared_ptr<ConnectionRequest> request);
    bool attemptDirectConnection(std::shared_ptr<ConnectionRequest> request);
    bool attemptFallback(std::shared_ptr<ConnectionRequest> request);
    void completeRequest(std::shared_ptr<ConnectionRequest> request, bool success, const std::string& error = "");
    double calculatePerformanceScore(ConnectionType type, const std::string& peer_ip, int peer_port);
    void updateLoadBalancing();

public:
    HybridConnectionManager(const HybridConfig& config = HybridConfig{});
    ~HybridConnectionManager();
    
    /**
     * Initialize hybrid connection manager with component managers
     * @param direct_connector Direct peer connector
     * @param bittorrent_protocol BitTorrent protocol handler
     * @param metadata_protocol UT_Metadata protocol handler
     */
    void initialize(std::shared_ptr<DirectPeerConnector> direct_connector,
                   std::shared_ptr<BitTorrentProtocol> bittorrent_protocol,
                   std::shared_ptr<UTMetadataProtocol> metadata_protocol);
    
    /**
     * Start a metadata request using hybrid connection management
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param info_hash Info hash
     * @param preferred_type Preferred connection type
     * @param strategy Connection strategy
     * @return Request ID
     */
    std::string startMetadataRequest(const std::string& peer_ip,
                                    int peer_port,
                                    const std::string& peer_id,
                                    const std::string& info_hash,
                                    ConnectionType preferred_type = ConnectionType::LIBTORRENT,
                                    ConnectionStrategy strategy = ConnectionStrategy::LIBTORRENT_FIRST);
    
    /**
     * Start a metadata request with automatic strategy selection
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param info_hash Info hash
     * @return Request ID
     */
    std::string startMetadataRequest(const std::string& peer_ip,
                                    int peer_port,
                                    const std::string& peer_id,
                                    const std::string& info_hash);
    
    /**
     * Process connection result
     * @param request_id Request ID
     * @param success Whether connection was successful
     * @param connection_type Connection type used
     * @param connection_id Connection ID
     * @param error_message Error message (if failed)
     * @return true if result was processed successfully
     */
    bool processConnectionResult(const std::string& request_id,
                                bool success,
                                ConnectionType connection_type,
                                const std::string& connection_id,
                                const std::string& error_message = "");
    
    /**
     * Process metadata result
     * @param request_id Request ID
     * @param success Whether metadata was received successfully
     * @param metadata_data Metadata data (if successful)
     * @param error_message Error message (if failed)
     * @return true if result was processed successfully
     */
    bool processMetadataResult(const std::string& request_id,
                              bool success,
                              const std::vector<uint8_t>& metadata_data,
                              const std::string& error_message = "");
    
    /**
     * Cancel a request
     * @param request_id Request ID
     * @return true if request was cancelled successfully
     */
    bool cancelRequest(const std::string& request_id);
    
    /**
     * Cancel all requests to a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Number of requests cancelled
     */
    int cancelRequestsToPeer(const std::string& peer_ip, int peer_port);
    
    /**
     * Get request information
     * @param request_id Request ID
     * @return Request information or nullptr if not found
     */
    std::shared_ptr<ConnectionRequest> getRequest(const std::string& request_id);
    
    /**
     * Get requests by status
     * @param status Request status
     * @return Vector of request information
     */
    std::vector<std::shared_ptr<ConnectionRequest>> getRequestsByStatus(RequestStatus status);
    
    /**
     * Get requests by connection type
     * @param type Connection type
     * @return Vector of request information
     */
    std::vector<std::shared_ptr<ConnectionRequest>> getRequestsByType(ConnectionType type);
    
    /**
     * Get active requests
     * @return Vector of active request information
     */
    std::vector<std::shared_ptr<ConnectionRequest>> getActiveRequests();
    
    /**
     * Get request history for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of historical request information
     */
    std::vector<ConnectionRequest> getRequestHistory(const std::string& peer_ip, int peer_port);
    
    /**
     * Check if request is active
     * @param request_id Request ID
     * @return true if request is active
     */
    bool isRequestActive(const std::string& request_id);
    
    /**
     * Check if request is completed
     * @param request_id Request ID
     * @return true if request is completed
     */
    bool isRequestCompleted(const std::string& request_id);
    
    /**
     * Get request progress
     * @param request_id Request ID
     * @return Progress percentage (0.0 to 1.0)
     */
    double getRequestProgress(const std::string& request_id);
    
    /**
     * Get performance metrics
     * @return Current performance metrics
     */
    PerformanceMetrics getPerformanceMetrics();
    
    /**
     * Reset performance metrics
     */
    void resetPerformanceMetrics();
    
    /**
     * Update hybrid configuration
     * @param config New hybrid configuration
     */
    void updateConfig(const HybridConfig& config);
    
    /**
     * Set default connection strategy
     * @param strategy Default strategy
     */
    void setDefaultStrategy(ConnectionStrategy strategy);
    
    /**
     * Get default connection strategy
     * @return Default strategy
     */
    ConnectionStrategy getDefaultStrategy();
    
    /**
     * Enable or disable performance tracking
     * @param enable true to enable performance tracking
     */
    void setPerformanceTrackingEnabled(bool enable);
    
    /**
     * Check if performance tracking is enabled
     * @return true if performance tracking is enabled
     */
    bool isPerformanceTrackingEnabled();
    
    /**
     * Enable or disable load balancing
     * @param enable true to enable load balancing
     */
    void setLoadBalancingEnabled(bool enable);
    
    /**
     * Check if load balancing is enabled
     * @return true if load balancing is enabled
     */
    bool isLoadBalancingEnabled();
    
    /**
     * Enable or disable automatic fallback
     * @param enable true to enable automatic fallback
     */
    void setAutomaticFallbackEnabled(bool enable);
    
    /**
     * Check if automatic fallback is enabled
     * @return true if automatic fallback is enabled
     */
    bool isAutomaticFallbackEnabled();
    
    /**
     * Get libtorrent success rate
     * @return libtorrent success rate (0.0 to 1.0)
     */
    double getLibtorrentSuccessRate();
    
    /**
     * Get direct connection success rate
     * @return Direct connection success rate (0.0 to 1.0)
     */
    double getDirectConnectionSuccessRate();
    
    /**
     * Get hybrid success rate
     * @return Hybrid success rate (0.0 to 1.0)
     */
    double getHybridSuccessRate();
    
    /**
     * Get overall success rate
     * @return Overall success rate (0.0 to 1.0)
     */
    double getOverallSuccessRate();
    
    /**
     * Get fallback success rate
     * @return Fallback success rate (0.0 to 1.0)
     */
    double getFallbackSuccessRate();
    
    /**
     * Get average connection time by type
     * @param type Connection type
     * @return Average connection time
     */
    std::chrono::milliseconds getAverageConnectionTime(ConnectionType type);
    
    /**
     * Get request count
     * @return Total number of active requests
     */
    int getRequestCount();
    
    /**
     * Get request count by status
     * @param status Request status
     * @return Number of requests with the status
     */
    int getRequestCount(RequestStatus status);
    
    /**
     * Get request count by type
     * @param type Connection type
     * @return Number of requests with the type
     */
    int getRequestCount(ConnectionType type);
    
    /**
     * Start hybrid connection manager
     */
    void start();
    
    /**
     * Stop hybrid connection manager
     */
    void stop();
    
    /**
     * Check if hybrid manager is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export request data to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportRequestData(const std::string& filename);
    
    /**
     * Export performance metrics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportPerformanceMetrics(const std::string& filename);
    
    /**
     * Get hybrid manager health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear request history
     */
    void clearRequestHistory();
    
    /**
     * Clear active requests
     */
    void clearActiveRequests();
    
    /**
     * Force cleanup of expired requests
     */
    void forceCleanup();
    
    /**
     * Get peer connection statistics
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Peer connection statistics
     */
    std::map<std::string, int> getPeerConnectionStatistics(const std::string& peer_ip, int peer_port);
    
    /**
     * Get connection type recommendations for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Recommended connection type
     */
    ConnectionType getRecommendedConnectionType(const std::string& peer_ip, int peer_port);
    
    /**
     * Get connection strategy recommendations for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Recommended connection strategy
     */
    ConnectionStrategy getRecommendedConnectionStrategy(const std::string& peer_ip, int peer_port);
    
    /**
     * Test hybrid connection with a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param info_hash Info hash
     * @param timeout Test timeout
     * @return true if hybrid connection test was successful
     */
    bool testHybridConnection(const std::string& peer_ip,
                             int peer_port,
                             const std::string& info_hash,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    
    /**
     * Get hybrid connection recommendations
     * @return Vector of hybrid connection recommendations
     */
    std::vector<std::string> getHybridConnectionRecommendations();
    
    /**
     * Optimize connection strategy based on performance data
     */
    void optimizeConnectionStrategy();
    
    /**
     * Get connection type performance comparison
     * @return Performance comparison data
     */
    std::map<std::string, double> getConnectionTypePerformanceComparison();
};

/**
 * Hybrid connection manager factory for different hybrid strategies
 */
class HybridConnectionManagerFactory {
public:
    enum class HybridStrategy {
        LIBTORRENT_PRIORITY,  // Prioritize libtorrent connections
        DIRECT_PRIORITY,      // Prioritize direct connections
        PERFORMANCE_OPTIMIZED, // Optimize for performance
        LOAD_BALANCED,        // Load balance between types
        CUSTOM                // Custom hybrid configuration
    };
    
    static std::unique_ptr<HybridConnectionManager> createManager(
        HybridStrategy strategy,
        const HybridConnectionManager::HybridConfig& config = HybridConnectionManager::HybridConfig{}
    );
    
    static HybridConnectionManager::HybridConfig getDefaultConfig(HybridStrategy strategy);
};

/**
 * RAII hybrid request wrapper for automatic request management
 */
class HybridRequestGuard {
private:
    std::shared_ptr<HybridConnectionManager> manager_;
    std::string request_id_;
    bool completed_;

public:
    HybridRequestGuard(std::shared_ptr<HybridConnectionManager> manager,
                      const std::string& peer_ip,
                      int peer_port,
                      const std::string& peer_id,
                      const std::string& info_hash,
                      HybridConnectionManager::ConnectionType preferred_type = HybridConnectionManager::ConnectionType::LIBTORRENT,
                      HybridConnectionManager::ConnectionStrategy strategy = HybridConnectionManager::ConnectionStrategy::LIBTORRENT_FIRST);
    
    ~HybridRequestGuard();
    
    /**
     * Process connection result
     * @param success Whether connection was successful
     * @param connection_type Connection type used
     * @param connection_id Connection ID
     * @param error_message Error message
     */
    void processConnectionResult(bool success,
                                HybridConnectionManager::ConnectionType connection_type,
                                const std::string& connection_id,
                                const std::string& error_message = "");
    
    /**
     * Process metadata result
     * @param success Whether metadata was received successfully
     * @param metadata_data Metadata data
     * @param error_message Error message
     */
    void processMetadataResult(bool success,
                              const std::vector<uint8_t>& metadata_data,
                              const std::string& error_message = "");
    
    /**
     * Cancel the request
     */
    void cancel();
    
    /**
     * Get request ID
     * @return Request ID
     */
    const std::string& getRequestId() const;
    
    /**
     * Get request information
     * @return Request information
     */
    std::shared_ptr<HybridConnectionManager::ConnectionRequest> getRequestInfo();
    
    /**
     * Check if request is completed
     * @return true if completed
     */
    bool isCompleted();
    
    /**
     * Check if request is active
     * @return true if active
     */
    bool isActive();
    
    /**
     * Get request progress
     * @return Progress percentage (0.0 to 1.0)
     */
    double getProgress();
};

/**
 * Hybrid connection analyzer for pattern detection and optimization
 */
class HybridConnectionAnalyzer {
private:
    std::shared_ptr<HybridConnectionManager> manager_;
    
public:
    HybridConnectionAnalyzer(std::shared_ptr<HybridConnectionManager> manager);
    
    /**
     * Analyze hybrid connection patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzeHybridConnectionPatterns();
    
    /**
     * Detect hybrid connection issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectHybridConnectionIssues();
    
    /**
     * Analyze peer connection compatibility
     * @return Map of peer IPs to compatibility scores
     */
    std::map<std::string, double> analyzePeerConnectionCompatibility();
    
    /**
     * Get hybrid connection optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getHybridConnectionOptimizationRecommendations();
    
    /**
     * Analyze hybrid connection performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzeHybridConnectionPerformanceTrends();
    
    /**
     * Get hybrid connection health score
     * @return Health score (0.0 to 1.0)
     */
    double getHybridConnectionHealthScore();
    
    /**
     * Get connection type efficiency analysis
     * @return Efficiency analysis data
     */
    std::map<std::string, double> getConnectionTypeEfficiencyAnalysis();
    
    /**
     * Get load balancing effectiveness
     * @return Load balancing effectiveness score (0.0 to 1.0)
     */
    double getLoadBalancingEffectiveness();
};

} // namespace dht_crawler
