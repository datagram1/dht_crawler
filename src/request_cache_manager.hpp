#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <thread>
#include <functional>
#include <unordered_map>
#include <list>

namespace dht_crawler {

/**
 * Sophisticated request caching manager for DHT operations
 * Implements Tribler-inspired request caching with timeout handling and completion callbacks
 * Reference: py-ipv8/dht/community.py - Request Class
 */
class RequestCacheManager {
public:
    enum class RequestType {
        FIND_NODE,
        GET_PEERS,
        ANNOUNCE_PEER,
        PING,
        GET,
        PUT,
        CUSTOM
    };

    enum class RequestStatus {
        PENDING,
        IN_PROGRESS,
        COMPLETED,
        FAILED,
        TIMEOUT,
        CANCELLED
    };

    struct RequestConfig {
        std::chrono::milliseconds default_timeout{30000};  // 30 seconds
        std::chrono::milliseconds cleanup_interval{60000}; // 1 minute
        size_t max_cache_size = 10000;
        bool enable_deduplication = true;
        bool enable_timeout_handling = true;
        bool enable_completion_callbacks = true;
        bool enable_statistics = true;
        int max_retries = 3;
        std::chrono::milliseconds retry_delay{1000};
        double retry_backoff_multiplier = 2.0;
    };

    struct RequestInfo {
        std::string request_id;
        RequestType type;
        std::string target_node;
        std::string target_ip;
        int target_port;
        std::map<std::string, std::string> parameters;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point timeout_at;
        std::chrono::milliseconds timeout_duration;
        RequestStatus status;
        std::function<void(const std::string&)> completion_callback;
        std::function<void(const std::string&)> error_callback;
        std::function<void(const std::string&)> timeout_callback;
        std::string response_data;
        std::string error_message;
        int retry_count;
        std::map<std::string, std::string> metadata;
        std::vector<std::string> related_requests;
    };

    struct CacheStatistics {
        int total_requests = 0;
        int completed_requests = 0;
        int failed_requests = 0;
        int timeout_requests = 0;
        int cancelled_requests = 0;
        int retry_attempts = 0;
        std::chrono::milliseconds avg_response_time;
        std::chrono::milliseconds max_response_time;
        std::chrono::milliseconds min_response_time;
        std::map<RequestType, int> requests_by_type;
        std::map<RequestStatus, int> requests_by_status;
        std::map<std::string, int> requests_by_node;
        double cache_hit_rate = 0.0;
        double success_rate = 0.0;
        size_t current_cache_size = 0;
    };

private:
    RequestConfig config_;
    std::unordered_map<std::string, std::shared_ptr<RequestInfo>> request_cache_;
    std::mutex cache_mutex_;
    
    // LRU cache implementation
    std::list<std::string> lru_order_;
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map_;
    std::mutex lru_mutex_;
    
    // Request deduplication
    std::map<std::string, std::vector<std::string>> duplicate_requests_;
    std::mutex dedup_mutex_;
    
    // Timeout management
    std::priority_queue<std::pair<std::chrono::steady_clock::time_point, std::string>> timeout_queue_;
    std::mutex timeout_mutex_;
    
    // Background processing
    std::thread cleanup_thread_;
    std::thread timeout_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable cleanup_condition_;
    
    CacheStatistics stats_;
    std::mutex stats_mutex_;
    
    // Internal methods
    std::string generateRequestId();
    void updateLRU(const std::string& request_id);
    void evictLRU();
    bool isDuplicateRequest(const RequestInfo& request_info);
    void addDuplicateRequest(const std::string& request_id, const RequestInfo& request_info);
    void processTimeout();
    void cleanupExpiredRequests();
    void updateStatistics(const RequestInfo& request_info, bool success, std::chrono::milliseconds duration);
    std::string createRequestKey(const RequestInfo& request_info);
    void notifyCompletion(const std::string& request_id, bool success, const std::string& data = "");

public:
    RequestCacheManager(const RequestConfig& config = RequestConfig{});
    ~RequestCacheManager();
    
    /**
     * Add a request to the cache
     * @param request_info Request information
     * @return Request ID
     */
    std::string addRequest(const RequestInfo& request_info);
    
    /**
     * Add a request with custom timeout
     * @param request_info Request information
     * @param timeout_duration Custom timeout duration
     * @return Request ID
     */
    std::string addRequestWithTimeout(const RequestInfo& request_info, 
                                    std::chrono::milliseconds timeout_duration);
    
    /**
     * Complete a request successfully
     * @param request_id Request ID
     * @param response_data Response data
     * @return true if request was completed successfully
     */
    bool completeRequest(const std::string& request_id, const std::string& response_data);
    
    /**
     * Fail a request
     * @param request_id Request ID
     * @param error_message Error message
     * @return true if request was failed successfully
     */
    bool failRequest(const std::string& request_id, const std::string& error_message);
    
    /**
     * Cancel a request
     * @param request_id Request ID
     * @return true if request was cancelled successfully
     */
    bool cancelRequest(const std::string& request_id);
    
    /**
     * Get request information
     * @param request_id Request ID
     * @return Request information or nullptr if not found
     */
    std::shared_ptr<RequestInfo> getRequest(const std::string& request_id);
    
    /**
     * Get requests by type
     * @param type Request type
     * @return Vector of request information
     */
    std::vector<std::shared_ptr<RequestInfo>> getRequestsByType(RequestType type);
    
    /**
     * Get requests by status
     * @param status Request status
     * @return Vector of request information
     */
    std::vector<std::shared_ptr<RequestInfo>> getRequestsByStatus(RequestStatus status);
    
    /**
     * Get requests by node
     * @param node_id Node ID
     * @return Vector of request information
     */
    std::vector<std::shared_ptr<RequestInfo>> getRequestsByNode(const std::string& node_id);
    
    /**
     * Get pending requests
     * @return Vector of pending request information
     */
    std::vector<std::shared_ptr<RequestInfo>> getPendingRequests();
    
    /**
     * Get expired requests
     * @return Vector of expired request information
     */
    std::vector<std::shared_ptr<RequestInfo>> getExpiredRequests();
    
    /**
     * Check if request exists
     * @param request_id Request ID
     * @return true if request exists
     */
    bool hasRequest(const std::string& request_id);
    
    /**
     * Check if request is pending
     * @param request_id Request ID
     * @return true if request is pending
     */
    bool isRequestPending(const std::string& request_id);
    
    /**
     * Check if request is completed
     * @param request_id Request ID
     * @return true if request is completed
     */
    bool isRequestCompleted(const std::string& request_id);
    
    /**
     * Get request status
     * @param request_id Request ID
     * @return Request status or UNKNOWN if not found
     */
    RequestStatus getRequestStatus(const std::string& request_id);
    
    /**
     * Get request response data
     * @param request_id Request ID
     * @return Response data or empty string if not found
     */
    std::string getRequestResponse(const std::string& request_id);
    
    /**
     * Get request error message
     * @param request_id Request ID
     * @return Error message or empty string if not found
     */
    std::string getRequestError(const std::string& request_id);
    
    /**
     * Retry a failed request
     * @param request_id Request ID
     * @return true if retry was initiated
     */
    bool retryRequest(const std::string& request_id);
    
    /**
     * Get cache statistics
     * @return Current cache statistics
     */
    CacheStatistics getStatistics();
    
    /**
     * Reset cache statistics
     */
    void resetStatistics();
    
    /**
     * Update cache configuration
     * @param config New cache configuration
     */
    void updateConfig(const RequestConfig& config);
    
    /**
     * Clear all requests
     */
    void clearCache();
    
    /**
     * Clear requests by type
     * @param type Request type
     * @return Number of requests cleared
     */
    int clearRequestsByType(RequestType type);
    
    /**
     * Clear requests by status
     * @param status Request status
     * @return Number of requests cleared
     */
    int clearRequestsByStatus(RequestStatus status);
    
    /**
     * Clear expired requests
     * @return Number of requests cleared
     */
    int clearExpiredRequests();
    
    /**
     * Get cache size
     * @return Current cache size
     */
    size_t getCacheSize();
    
    /**
     * Get maximum cache size
     * @return Maximum cache size
     */
    size_t getMaxCacheSize();
    
    /**
     * Set maximum cache size
     * @param max_size New maximum cache size
     */
    void setMaxCacheSize(size_t max_size);
    
    /**
     * Enable or disable deduplication
     * @param enable true to enable deduplication
     */
    void setDeduplicationEnabled(bool enable);
    
    /**
     * Check if deduplication is enabled
     * @return true if deduplication is enabled
     */
    bool isDeduplicationEnabled();
    
    /**
     * Enable or disable timeout handling
     * @param enable true to enable timeout handling
     */
    void setTimeoutHandlingEnabled(bool enable);
    
    /**
     * Check if timeout handling is enabled
     * @return true if timeout handling is enabled
     */
    bool isTimeoutHandlingEnabled();
    
    /**
     * Get cache hit rate
     * @return Cache hit rate (0.0 to 1.0)
     */
    double getCacheHitRate();
    
    /**
     * Get success rate
     * @return Success rate (0.0 to 1.0)
     */
    double getSuccessRate();
    
    /**
     * Get average response time
     * @return Average response time
     */
    std::chrono::milliseconds getAverageResponseTime();
    
    /**
     * Start cache manager
     */
    void start();
    
    /**
     * Stop cache manager
     */
    void stop();
    
    /**
     * Check if cache manager is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export cache to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportCache(const std::string& filename);
    
    /**
     * Import cache from file
     * @param filename Input filename
     * @return true if import was successful
     */
    bool importCache(const std::string& filename);
    
    /**
     * Get cache health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Force cleanup of expired requests
     */
    void forceCleanup();
    
    /**
     * Get duplicate requests for a request
     * @param request_id Request ID
     * @return Vector of duplicate request IDs
     */
    std::vector<std::string> getDuplicateRequests(const std::string& request_id);
    
    /**
     * Get related requests
     * @param request_id Request ID
     * @return Vector of related request IDs
     */
    std::vector<std::string> getRelatedRequests(const std::string& request_id);
    
    /**
     * Add related request
     * @param request_id Request ID
     * @param related_request_id Related request ID
     */
    void addRelatedRequest(const std::string& request_id, const std::string& related_request_id);
};

/**
 * Request cache manager factory for different caching strategies
 */
class RequestCacheManagerFactory {
public:
    enum class CacheStrategy {
        PERFORMANCE,    // Optimized for performance
        MEMORY,         // Optimized for memory usage
        RELIABILITY,    // Optimized for reliability
        CUSTOM          // Custom configuration
    };
    
    static std::unique_ptr<RequestCacheManager> createCacheManager(
        CacheStrategy strategy,
        const RequestCacheManager::RequestConfig& config = RequestCacheManager::RequestConfig{}
    );
    
    static RequestCacheManager::RequestConfig getDefaultConfig(CacheStrategy strategy);
};

/**
 * RAII request wrapper for automatic request management
 */
class RequestGuard {
private:
    std::shared_ptr<RequestCacheManager> cache_manager_;
    std::string request_id_;
    bool completed_;

public:
    RequestGuard(std::shared_ptr<RequestCacheManager> cache_manager,
                 const RequestCacheManager::RequestInfo& request_info);
    
    ~RequestGuard();
    
    /**
     * Complete the request successfully
     * @param response_data Response data
     */
    void complete(const std::string& response_data);
    
    /**
     * Fail the request
     * @param error_message Error message
     */
    void fail(const std::string& error_message);
    
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
     * Check if request is completed
     * @return true if completed
     */
    bool isCompleted();
    
    /**
     * Get request information
     * @return Request information
     */
    std::shared_ptr<RequestCacheManager::RequestInfo> getRequestInfo();
};

/**
 * Request deduplication analyzer
 */
class RequestDeduplicationAnalyzer {
private:
    std::shared_ptr<RequestCacheManager> cache_manager_;
    
public:
    RequestDeduplicationAnalyzer(std::shared_ptr<RequestCacheManager> cache_manager);
    
    /**
     * Analyze duplicate request patterns
     * @return Map of duplicate patterns and their frequencies
     */
    std::map<std::string, int> analyzeDuplicatePatterns();
    
    /**
     * Detect duplicate request clusters
     * @param time_window Time window for clustering
     * @return Vector of duplicate clusters
     */
    std::vector<std::vector<std::string>> detectDuplicateClusters(
        std::chrono::minutes time_window = std::chrono::minutes(5)
    );
    
    /**
     * Get deduplication efficiency
     * @return Deduplication efficiency (0.0 to 1.0)
     */
    double getDeduplicationEfficiency();
    
    /**
     * Get duplicate request statistics
     * @return Duplicate request statistics
     */
    std::map<std::string, int> getDuplicateStatistics();
};

} // namespace dht_crawler
