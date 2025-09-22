#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <condition_variable>

namespace dht_crawler {

/**
 * Sophisticated timeout management system
 * Provides granular timeout handling with escalation and retry logic
 */
class TimeoutManager {
public:
    enum class TimeoutType {
        CONNECTION,     // Connection establishment timeout
        METADATA,       // Metadata extraction timeout
        DHT_QUERY,      // DHT query timeout
        PEER_RESPONSE,  // Peer response timeout
        TRACKER_REQUEST, // Tracker request timeout
        BOOTSTRAP,      // Bootstrap timeout
        CUSTOM          // Custom timeout type
    };

    struct TimeoutConfig {
        std::chrono::milliseconds connection_timeout;    // 30 seconds
        std::chrono::milliseconds metadata_timeout;     // 120 seconds
        std::chrono::milliseconds dht_query_timeout;   // 10 seconds
        std::chrono::milliseconds peer_response_timeout; // 15 seconds
        std::chrono::milliseconds tracker_timeout;      // 30 seconds
        std::chrono::milliseconds bootstrap_timeout;     // 60 seconds
        
        // Retry configuration
        int max_retries;
        std::chrono::milliseconds retry_delay;           // 1 second
        double retry_backoff_multiplier;
        std::chrono::milliseconds max_retry_delay;      // 30 seconds
        
        // Escalation configuration
        bool enable_escalation;
        std::chrono::milliseconds escalation_threshold;  // 5 seconds
        int escalation_steps;
        
        TimeoutConfig() :
            connection_timeout(30000),
            metadata_timeout(120000),
            dht_query_timeout(10000),
            peer_response_timeout(15000),
            tracker_timeout(30000),
            bootstrap_timeout(60000),
            max_retries(3),
            retry_delay(1000),
            retry_backoff_multiplier(2.0),
            max_retry_delay(30000),
            enable_escalation(true),
            escalation_threshold(5000),
            escalation_steps(3) {}
    };

    struct TimeoutRequest {
        std::string request_id;
        TimeoutType type;
        std::chrono::milliseconds timeout_duration;
        std::chrono::steady_clock::time_point start_time;
        std::function<void()> timeout_callback;
        std::function<void()> success_callback;
        int retry_count = 0;
        bool is_active = true;
        std::string context;
        std::map<std::string, std::string> metadata;
    };

    struct TimeoutStatistics {
        std::map<TimeoutType, int> total_requests;
        std::map<TimeoutType, int> successful_requests;
        std::map<TimeoutType, int> timed_out_requests;
        std::map<TimeoutType, int> retry_attempts;
        std::map<TimeoutType, std::chrono::milliseconds> avg_response_time;
        std::map<TimeoutType, std::chrono::milliseconds> max_response_time;
        std::map<TimeoutType, std::chrono::milliseconds> min_response_time;
    };

private:
    TimeoutConfig config_;
    std::map<std::string, std::shared_ptr<TimeoutRequest>> active_timeouts_;
    std::mutex timeouts_mutex_;
    std::thread timeout_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable timeout_condition_;
    std::queue<std::string> timeout_queue_;
    std::mutex queue_mutex_;
    
    TimeoutStatistics stats_;
    std::mutex stats_mutex_;
    
    // Internal methods
    void timeoutMonitorLoop();
    void processTimeout(const std::string& request_id);
    void handleTimeoutEscalation(std::shared_ptr<TimeoutRequest> request);
    void updateStatistics(const TimeoutRequest& request, bool success);
    std::chrono::milliseconds calculateRetryDelay(int retry_count);

public:
    TimeoutManager(const TimeoutConfig& config = TimeoutConfig());
    ~TimeoutManager();
    
    /**
     * Start a timeout for a specific request
     * @param request_id Unique identifier for the request
     * @param type Type of timeout
     * @param timeout_callback Callback to execute on timeout
     * @param success_callback Callback to execute on success (optional)
     * @param context Additional context information
     * @param metadata Additional metadata
     * @return true if timeout was started successfully
     */
    bool startTimeout(const std::string& request_id,
                     TimeoutType type,
                     std::function<void()> timeout_callback,
                     std::function<void()> success_callback = nullptr,
                     const std::string& context = "",
                     const std::map<std::string, std::string>& metadata = {});
    
    /**
     * Start a timeout with custom duration
     * @param request_id Unique identifier for the request
     * @param type Type of timeout
     * @param duration Custom timeout duration
     * @param timeout_callback Callback to execute on timeout
     * @param success_callback Callback to execute on success (optional)
     * @param context Additional context information
     * @return true if timeout was started successfully
     */
    bool startCustomTimeout(const std::string& request_id,
                           TimeoutType type,
                           std::chrono::milliseconds duration,
                           std::function<void()> timeout_callback,
                           std::function<void()> success_callback = nullptr,
                           const std::string& context = "");
    
    /**
     * Cancel a timeout
     * @param request_id Unique identifier for the request
     * @return true if timeout was cancelled successfully
     */
    bool cancelTimeout(const std::string& request_id);
    
    /**
     * Complete a timeout successfully
     * @param request_id Unique identifier for the request
     * @return true if timeout was completed successfully
     */
    bool completeTimeout(const std::string& request_id);
    
    /**
     * Extend a timeout duration
     * @param request_id Unique identifier for the request
     * @param additional_duration Additional time to add
     * @return true if timeout was extended successfully
     */
    bool extendTimeout(const std::string& request_id, 
                      std::chrono::milliseconds additional_duration);
    
    /**
     * Check if a timeout is active
     * @param request_id Unique identifier for the request
     * @return true if timeout is active
     */
    bool isTimeoutActive(const std::string& request_id);
    
    /**
     * Get remaining time for a timeout
     * @param request_id Unique identifier for the request
     * @return Remaining time in milliseconds, or 0 if not found
     */
    std::chrono::milliseconds getRemainingTime(const std::string& request_id);
    
    /**
     * Get timeout statistics
     * @return Current timeout statistics
     */
    TimeoutStatistics getStatistics();
    
    /**
     * Reset timeout statistics
     */
    void resetStatistics();
    
    /**
     * Update timeout configuration
     * @param config New timeout configuration
     */
    void updateConfig(const TimeoutConfig& config);
    
    /**
     * Get active timeout count
     * @return Number of active timeouts
     */
    size_t getActiveTimeoutCount();
    
    /**
     * Get timeout configuration for a specific type
     * @param type Timeout type
     * @return Timeout duration for the type
     */
    std::chrono::milliseconds getTimeoutDuration(TimeoutType type);
    
    /**
     * Set timeout duration for a specific type
     * @param type Timeout type
     * @param duration New timeout duration
     */
    void setTimeoutDuration(TimeoutType type, std::chrono::milliseconds duration);
    
    /**
     * Enable or disable timeout escalation
     * @param enable true to enable escalation
     */
    void setEscalationEnabled(bool enable);
    
    /**
     * Get timeout escalation status
     * @return true if escalation is enabled
     */
    bool isEscalationEnabled();
    
    /**
     * Force timeout for a specific request (for testing)
     * @param request_id Unique identifier for the request
     * @return true if timeout was forced successfully
     */
    bool forceTimeout(const std::string& request_id);
    
    /**
     * Get timeout request information
     * @param request_id Unique identifier for the request
     * @return Timeout request information or nullptr if not found
     */
    std::shared_ptr<TimeoutRequest> getTimeoutRequest(const std::string& request_id);
    
    /**
     * Get all active timeout requests
     * @return Map of active timeout requests
     */
    std::map<std::string, std::shared_ptr<TimeoutRequest>> getActiveTimeouts();
    
    /**
     * Clean up expired timeouts
     */
    void cleanupExpiredTimeouts();
    
    /**
     * Stop the timeout manager
     */
    void stop();
    
    /**
     * Start the timeout manager
     */
    void start();
};

/**
 * Timeout manager factory for different timeout strategies
 */
class TimeoutManagerFactory {
public:
    enum class TimeoutStrategy {
        AGGRESSIVE,  // Short timeouts for fast failure
        MODERATE,    // Balanced timeouts
        CONSERVATIVE, // Long timeouts for reliability
        CUSTOM       // Custom configuration
    };
    
    static std::unique_ptr<TimeoutManager> createTimeoutManager(
        TimeoutStrategy strategy,
        const TimeoutManager::TimeoutConfig& config = TimeoutManager::TimeoutConfig{}
    );
    
    static TimeoutManager::TimeoutConfig getDefaultConfig(TimeoutStrategy strategy);
};

/**
 * RAII timeout wrapper for automatic timeout management
 */
class TimeoutGuard {
private:
    std::shared_ptr<TimeoutManager> timeout_manager_;
    std::string request_id_;
    bool completed_;

public:
    TimeoutGuard(std::shared_ptr<TimeoutManager> timeout_manager,
                 const std::string& request_id,
                 TimeoutManager::TimeoutType type,
                 std::function<void()> timeout_callback,
                 const std::string& context = "");
    
    ~TimeoutGuard();
    
    /**
     * Complete the timeout successfully
     */
    void complete();
    
    /**
     * Cancel the timeout
     */
    void cancel();
    
    /**
     * Extend the timeout
     * @param additional_duration Additional time to add
     */
    void extend(std::chrono::milliseconds additional_duration);
    
    /**
     * Check if timeout is still active
     * @return true if timeout is active
     */
    bool isActive();
};

} // namespace dht_crawler
