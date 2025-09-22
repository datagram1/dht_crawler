#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>

namespace dht_crawler {

/**
 * Performance Optimizer
 * Provides runtime performance optimization and tuning
 */
class PerformanceOptimizer {
public:
    // Performance metrics
    struct PerformanceMetrics {
        double cpu_usage_percent;
        double memory_usage_mb;
        double network_throughput_mbps;
        double average_response_time_ms;
        double success_rate_percent;
        int active_connections;
        int queued_requests;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    // Optimization strategies
    enum class OptimizationStrategy {
        ADAPTIVE,           // Automatically adjust based on performance
        MANUAL,             // Manual optimization
        PROFILE_BASED,      // Use predefined profiles
        ML_BASED            // Machine learning based optimization
    };
    
    // Constructor
    PerformanceOptimizer();
    ~PerformanceOptimizer();
    
    // Performance monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Metrics collection
    PerformanceMetrics getCurrentMetrics();
    std::vector<PerformanceMetrics> getHistoricalMetrics(int count = 100);
    void recordMetric(const PerformanceMetrics& metric);
    
    // Optimization
    void optimize();
    void setOptimizationStrategy(OptimizationStrategy strategy);
    OptimizationStrategy getOptimizationStrategy() const;
    
    // Adaptive optimization
    void enableAdaptiveOptimization(bool enable);
    bool isAdaptiveOptimizationEnabled() const;
    void setAdaptiveThresholds(const std::map<std::string, double>& thresholds);
    
    // Performance analysis
    bool isPerformanceDegraded() const;
    std::string getPerformanceBottleneck() const;
    std::vector<std::string> getOptimizationRecommendations() const;
    
    // Resource management
    void optimizeMemoryUsage();
    void optimizeCPUUsage();
    void optimizeNetworkUsage();
    void optimizeDatabaseUsage();
    
    // Threading optimization
    void optimizeThreadPool();
    void setOptimalThreadCount();
    int getOptimalThreadCount() const;
    
    // Connection optimization
    void optimizeConnections();
    void setOptimalConnectionLimits();
    std::pair<int, int> getOptimalConnectionLimits() const;
    
    // Cache optimization
    void optimizeCache();
    void setOptimalCacheSize();
    size_t getOptimalCacheSize() const;
    
    // Database optimization
    void optimizeDatabase();
    void setOptimalBatchSize();
    int getOptimalBatchSize() const;
    
    // Performance profiling
    void startProfiling();
    void stopProfiling();
    bool isProfiling() const;
    std::string getProfilingReport() const;
    
    // Configuration
    void loadOptimizationConfig(const std::string& filename);
    void saveOptimizationConfig(const std::string& filename);
    
    // Statistics
    double getAverageCPUUsage() const;
    double getAverageMemoryUsage() const;
    double getAverageResponseTime() const;
    double getAverageSuccessRate() const;
    
    // Alerts
    void setPerformanceAlert(const std::string& metric, double threshold, 
                           std::function<void()> callback);
    void clearPerformanceAlert(const std::string& metric);
    
private:
    // Internal state
    bool monitoring_enabled_;
    bool adaptive_optimization_enabled_;
    bool profiling_enabled_;
    OptimizationStrategy strategy_;
    
    // Metrics storage
    std::vector<PerformanceMetrics> metrics_history_;
    std::chrono::steady_clock::time_point last_optimization_;
    
    // Configuration
    std::map<std::string, double> adaptive_thresholds_;
    std::map<std::string, std::function<void()>> performance_alerts_;
    
    // Optimization state
    int current_thread_count_;
    int current_max_connections_;
    size_t current_cache_size_;
    int current_batch_size_;
    
    // Internal methods
    void collectSystemMetrics(PerformanceMetrics& metrics);
    void analyzePerformance();
    void applyOptimizations();
    void updateConfiguration();
    void checkPerformanceAlerts(const PerformanceMetrics& metrics);
    
    // Optimization algorithms
    void adaptiveOptimization();
    void profileBasedOptimization();
    void mlBasedOptimization();
    
    // Utility methods
    double calculateOptimalThreadCount() const;
    std::pair<int, int> calculateOptimalConnectionLimits() const;
    size_t calculateOptimalCacheSize() const;
    int calculateOptimalBatchSize() const;
    
    // Performance analysis
    bool isCPUBottleneck() const;
    bool isMemoryBottleneck() const;
    bool isNetworkBottleneck() const;
    bool isDatabaseBottleneck() const;
    
    // Configuration management
    void loadDefaultConfig();
    void validateConfig();
    void logOptimization(const std::string& action, const std::string& details);
};

} // namespace dht_crawler
