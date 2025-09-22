#pragma once

#include <string>
#include <map>
#include <vector>

namespace dht_crawler {

/**
 * Performance Configuration Manager
 * Manages performance-related settings and optimizations
 */
class PerformanceConfig {
public:
    // Connection limits
    static constexpr int MAX_CONNECTIONS = 200;
    static constexpr int MAX_ACTIVE_CONNECTIONS = 1000;
    static constexpr int MAX_CONCURRENT_REQUESTS = 50;
    
    // Timeout settings
    static constexpr int CONNECTION_TIMEOUT_MS = 30000;      // 30 seconds
    static constexpr int METADATA_TIMEOUT_MS = 120000;      // 2 minutes
    static constexpr int DHT_TIMEOUT_MS = 10000;            // 10 seconds
    static constexpr int HANDSHAKE_TIMEOUT_MS = 30000;      // 30 seconds
    
    // Retry settings
    static constexpr int MAX_RETRY_ATTEMPTS = 3;
    static constexpr int RETRY_DELAY_MS = 1000;            // 1 second
    static constexpr int EXPONENTIAL_BACKOFF_FACTOR = 2;
    
    // DHT settings
    static constexpr int DHT_BOOTSTRAP_NODES = 8;
    static constexpr int DHT_ROUTING_TABLE_SIZE = 8000;
    static constexpr int DHT_ANNOUNCE_INTERVAL = 15;
    static constexpr int DHT_PING_INTERVAL = 25;
    
    // Metadata settings
    static constexpr int METADATA_PIECE_SIZE = 16384;       // 16KB
    static constexpr int MAX_METADATA_SIZE = 10485760;      // 10MB
    static constexpr int METADATA_WORKER_THREADS = 10;
    static constexpr int METADATA_QUEUE_SIZE = 1000;
    
    // Performance monitoring
    static constexpr int METRICS_COLLECTION_INTERVAL_MS = 5000;  // 5 seconds
    static constexpr int PERFORMANCE_LOG_INTERVAL_MS = 60000;     // 1 minute
    
    // Memory management
    static constexpr size_t MAX_MEMORY_USAGE_MB = 1024;     // 1GB
    static constexpr size_t MEMORY_CLEANUP_INTERVAL_MS = 300000; // 5 minutes
    
    // Network optimization
    static constexpr int TCP_NODELAY = 1;
    static constexpr int TCP_KEEPALIVE = 1;
    static constexpr int TCP_KEEPALIVE_IDLE = 600;         // 10 minutes
    static constexpr int TCP_KEEPALIVE_INTERVAL = 60;       // 1 minute
    static constexpr int TCP_KEEPALIVE_COUNT = 3;
    
    // Threading
    static constexpr int MAX_WORKER_THREADS = 16;
    static constexpr int MIN_WORKER_THREADS = 4;
    static constexpr int THREAD_STACK_SIZE = 1048576;       // 1MB
    
    // Caching
    static constexpr int CACHE_SIZE = 10000;
    static constexpr int CACHE_TTL_SECONDS = 3600;         // 1 hour
    static constexpr int CACHE_CLEANUP_INTERVAL_MS = 300000; // 5 minutes
    
    // Database optimization
    static constexpr int DB_CONNECTION_POOL_SIZE = 10;
    static constexpr int DB_CONNECTION_TIMEOUT_MS = 5000;  // 5 seconds
    static constexpr int DB_QUERY_TIMEOUT_MS = 30000;      // 30 seconds
    static constexpr int DB_BATCH_SIZE = 1000;
    
    // Error handling
    static constexpr int MAX_ERROR_COUNT = 100;
    static constexpr int ERROR_RESET_INTERVAL_MS = 300000;  // 5 minutes
    
    // Logging
    static constexpr int LOG_BUFFER_SIZE = 8192;           // 8KB
    static constexpr int LOG_FLUSH_INTERVAL_MS = 1000;     // 1 second
    
    // Performance tuning methods
    static void optimizeForHighThroughput();
    static void optimizeForLowLatency();
    static void optimizeForMemoryUsage();
    static void optimizeForCPUUsage();
    
    // Configuration management
    static void setConfig(const std::string& key, const std::string& value);
    static std::string getConfig(const std::string& key);
    static void loadConfigFromFile(const std::string& filename);
    static void saveConfigToFile(const std::string& filename);
    
    // Performance monitoring
    static void enablePerformanceMonitoring(bool enable);
    static bool isPerformanceMonitoringEnabled();
    static void setPerformanceThreshold(const std::string& metric, double threshold);
    static double getPerformanceThreshold(const std::string& metric);
    
    // Resource limits
    static void setResourceLimit(const std::string& resource, size_t limit);
    static size_t getResourceLimit(const std::string& resource);
    
    // Optimization profiles
    enum class OptimizationProfile {
        BALANCED,           // Balanced performance and resource usage
        HIGH_THROUGHPUT,    // Maximize throughput
        LOW_LATENCY,        // Minimize latency
        LOW_MEMORY,         // Minimize memory usage
        LOW_CPU             // Minimize CPU usage
    };
    
    static void setOptimizationProfile(OptimizationProfile profile);
    static OptimizationProfile getOptimizationProfile();
    
private:
    static std::map<std::string, std::string> config_;
    static std::map<std::string, double> performance_thresholds_;
    static std::map<std::string, size_t> resource_limits_;
    static OptimizationProfile current_profile_;
    static bool performance_monitoring_enabled_;
    
    static void applyProfileSettings(OptimizationProfile profile);
    static void validateConfig();
    static void logConfigChange(const std::string& key, const std::string& value);
};

} // namespace dht_crawler
