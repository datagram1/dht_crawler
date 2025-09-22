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
#include <unordered_map>
#include <bitset>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace dht_crawler {

/**
 * Performance monitor for comprehensive monitoring
 * Provides performance metrics, monitoring, and analysis
 */
class PerformanceMonitor {
public:
    enum class MetricType {
        COUNTER,        // Counter metric
        GAUGE,          // Gauge metric
        HISTOGRAM,      // Histogram metric
        TIMER,          // Timer metric
        RATE,           // Rate metric
        PERCENTILE      // Percentile metric
    };

    enum class AlertLevel {
        INFO,           // Informational alert
        WARNING,        // Warning alert
        ERROR,          // Error alert
        CRITICAL        // Critical alert
    };

    struct PerformanceConfig {
        std::chrono::milliseconds monitoring_interval{1000};    // 1 second
        std::chrono::milliseconds metrics_retention{3600000};   // 1 hour
        std::chrono::milliseconds alert_retention{86400000};     // 24 hours
        bool enable_real_time_monitoring = true;                // Enable real-time monitoring
        bool enable_historical_monitoring = true;               // Enable historical monitoring
        bool enable_performance_alerts = true;                  // Enable performance alerts
        bool enable_metrics_export = true;                      // Enable metrics export
        bool enable_performance_analysis = true;                // Enable performance analysis
        bool enable_trend_analysis = true;                      // Enable trend analysis
        bool enable_capacity_planning = true;                   // Enable capacity planning
        bool enable_performance_optimization = true;            // Enable performance optimization
        double performance_threshold = 0.8;                     // Performance threshold
        double alert_threshold = 0.9;                           // Alert threshold
        int max_metrics_per_type = 1000;                        // Max metrics per type
        int max_alerts_per_level = 100;                         // Max alerts per level
        std::chrono::milliseconds cleanup_interval{300000};     // 5 minutes
    };

    struct PerformanceMetric {
        std::string name;
        MetricType type;
        double value;
        std::chrono::steady_clock::time_point timestamp;
        std::map<std::string, std::string> labels;
        std::map<std::string, std::string> metadata;
    };

    struct PerformanceAlert {
        std::string id;
        AlertLevel level;
        std::string message;
        std::string metric_name;
        double threshold;
        double current_value;
        std::chrono::steady_clock::time_point timestamp;
        std::map<std::string, std::string> labels;
        std::map<std::string, std::string> metadata;
    };

    struct PerformanceStatistics {
        int total_metrics = 0;
        int counter_metrics = 0;
        int gauge_metrics = 0;
        int histogram_metrics = 0;
        int timer_metrics = 0;
        int rate_metrics = 0;
        int percentile_metrics = 0;
        int total_alerts = 0;
        int info_alerts = 0;
        int warning_alerts = 0;
        int error_alerts = 0;
        int critical_alerts = 0;
        double avg_metric_value = 0.0;
        double min_metric_value = 0.0;
        double max_metric_value = 0.0;
        double avg_alert_threshold = 0.0;
        double min_alert_threshold = 0.0;
        double max_alert_threshold = 0.0;
        std::chrono::milliseconds avg_metric_age;
        std::chrono::milliseconds min_metric_age;
        std::chrono::milliseconds max_metric_age;
        std::chrono::milliseconds avg_alert_age;
        std::chrono::milliseconds min_alert_age;
        std::chrono::milliseconds max_alert_age;
        std::map<MetricType, int> metrics_by_type;
        std::map<AlertLevel, int> alerts_by_level;
        std::map<std::string, int> metrics_by_name;
        std::map<std::string, int> alerts_by_metric;
        std::chrono::steady_clock::time_point last_update;
    };

private:
    PerformanceConfig config_;
    std::map<std::string, std::vector<PerformanceMetric>> metrics_;
    std::mutex metrics_mutex_;
    
    std::vector<PerformanceAlert> alerts_;
    std::mutex alerts_mutex_;
    
    PerformanceStatistics stats_;
    std::mutex stats_mutex_;
    
    // Background processing
    std::thread monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Internal methods
    void monitorLoop();
    void monitorPerformance();
    void cleanupExpiredMetrics();
    void cleanupExpiredAlerts();
    void updateStatistics();
    void checkPerformanceThresholds();
    void generatePerformanceAlerts();
    void analyzePerformanceTrends();
    void optimizePerformance();
    std::string metricTypeToString(MetricType type);
    std::string alertLevelToString(AlertLevel level);
    bool isMetricExpired(const PerformanceMetric& metric);
    bool isAlertExpired(const PerformanceAlert& alert);
    void updateMetricQuality(std::shared_ptr<PerformanceMetric> metric);
    void updateAlertQuality(std::shared_ptr<PerformanceAlert> alert);
    MetricType determineMetricType(const std::string& name);
    AlertLevel determineAlertLevel(double threshold, double current_value);
    void redistributeMetrics(std::shared_ptr<PerformanceMetric> metric);
    std::vector<std::string> getMetricNames();
    std::vector<std::string> getAlertIds();
    void initializePerformanceMetrics();
    void createMetric(const std::string& name, MetricType type);
    void destroyMetric(const std::string& name);
    bool isValidMetricName(const std::string& name);
    bool isValidMetricValue(double value);
    bool isValidAlertLevel(AlertLevel level);
    bool isValidAlertThreshold(double threshold);

public:
    PerformanceMonitor(const PerformanceConfig& config = PerformanceConfig{});
    ~PerformanceMonitor();
    
    /**
     * Record a counter metric
     * @param name Metric name
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordCounter(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record a gauge metric
     * @param name Metric name
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordGauge(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record a histogram metric
     * @param name Metric name
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordHistogram(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record a timer metric
     * @param name Metric name
     * @param duration Duration
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordTimer(const std::string& name, std::chrono::milliseconds duration, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record a rate metric
     * @param name Metric name
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordRate(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record a percentile metric
     * @param name Metric name
     * @param value Metric value
     * @param percentile Percentile
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordPercentile(const std::string& name, double value, double percentile, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Get metric information
     * @param name Metric name
     * @return Vector of metric information
     */
    std::vector<PerformanceMetric> getMetrics(const std::string& name);
    
    /**
     * Get latest metric value
     * @param name Metric name
     * @return Latest metric value or 0.0 if not found
     */
    double getLatestMetricValue(const std::string& name);
    
    /**
     * Get metric average value
     * @param name Metric name
     * @return Average metric value or 0.0 if not found
     */
    double getMetricAverageValue(const std::string& name);
    
    /**
     * Get metric minimum value
     * @param name Metric name
     * @return Minimum metric value or 0.0 if not found
     */
    double getMetricMinimumValue(const std::string& name);
    
    /**
     * Get metric maximum value
     * @param name Metric name
     * @return Maximum metric value or 0.0 if not found
     */
    double getMetricMaximumValue(const std::string& name);
    
    /**
     * Get metric sum value
     * @param name Metric name
     * @return Sum metric value or 0.0 if not found
     */
    double getMetricSumValue(const std::string& name);
    
    /**
     * Get metric count
     * @param name Metric name
     * @return Metric count or 0 if not found
     */
    int getMetricCount(const std::string& name);
    
    /**
     * Get metric type
     * @param name Metric name
     * @return Metric type or COUNTER if not found
     */
    MetricType getMetricType(const std::string& name);
    
    /**
     * Get metric labels
     * @param name Metric name
     * @return Metric labels or empty map if not found
     */
    std::map<std::string, std::string> getMetricLabels(const std::string& name);
    
    /**
     * Get metric metadata
     * @param name Metric name
     * @return Metric metadata or empty map if not found
     */
    std::map<std::string, std::string> getMetricMetadata(const std::string& name);
    
    /**
     * Set metric metadata
     * @param name Metric name
     * @param key Metadata key
     * @param value Metadata value
     * @return true if metadata was set successfully
     */
    bool setMetricMetadata(const std::string& name, const std::string& key, const std::string& value);
    
    /**
     * Remove metric metadata
     * @param name Metric name
     * @param key Metadata key
     * @return true if metadata was removed successfully
     */
    bool removeMetricMetadata(const std::string& name, const std::string& key);
    
    /**
     * Clear metric metadata
     * @param name Metric name
     * @return true if metadata was cleared successfully
     */
    bool clearMetricMetadata(const std::string& name);
    
    /**
     * Check if metric exists
     * @param name Metric name
     * @return true if metric exists
     */
    bool hasMetric(const std::string& name);
    
    /**
     * Get all metric names
     * @return Vector of metric names
     */
    std::vector<std::string> getAllMetricNames();
    
    /**
     * Get metrics by type
     * @param type Metric type
     * @return Vector of metric names
     */
    std::vector<std::string> getMetricsByType(MetricType type);
    
    /**
     * Get metrics by labels
     * @param labels Metric labels
     * @return Vector of metric names
     */
    std::vector<std::string> getMetricsByLabels(const std::map<std::string, std::string>& labels);
    
    /**
     * Get metrics by metadata
     * @param metadata Metric metadata
     * @return Vector of metric names
     */
    std::vector<std::string> getMetricsByMetadata(const std::map<std::string, std::string>& metadata);
    
    /**
     * Get metric count by type
     * @param type Metric type
     * @return Metric count
     */
    int getMetricCountByType(MetricType type);
    
    /**
     * Get metric count by labels
     * @param labels Metric labels
     * @return Metric count
     */
    int getMetricCountByLabels(const std::map<std::string, std::string>& labels);
    
    /**
     * Get metric count by metadata
     * @param metadata Metric metadata
     * @return Metric count
     */
    int getMetricCountByMetadata(const std::map<std::string, std::string>& metadata);
    
    /**
     * Get total metric count
     * @return Total metric count
     */
    int getTotalMetricCount();
    
    /**
     * Get total metric count by type
     * @param type Metric type
     * @return Total metric count
     */
    int getTotalMetricCountByType(MetricType type);
    
    /**
     * Get total metric count by labels
     * @param labels Metric labels
     * @return Total metric count
     */
    int getTotalMetricCountByLabels(const std::map<std::string, std::string>& labels);
    
    /**
     * Get total metric count by metadata
     * @param metadata Metric metadata
     * @return Total metric count
     */
    int getTotalMetricCountByMetadata(const std::map<std::string, std::string>& metadata);
    
    /**
     * Get metric statistics
     * @param name Metric name
     * @return Metric statistics
     */
    std::map<std::string, double> getMetricStatistics(const std::string& name);
    
    /**
     * Get performance monitor statistics
     * @return Performance monitor statistics
     */
    PerformanceStatistics getStatistics();
    
    /**
     * Reset performance monitor statistics
     */
    void resetStatistics();
    
    /**
     * Update performance configuration
     * @param config New performance configuration
     */
    void updateConfig(const PerformanceConfig& config);
    
    /**
     * Get performance configuration
     * @return Current performance configuration
     */
    PerformanceConfig getConfig();
    
    /**
     * Start performance monitor
     */
    void start();
    
    /**
     * Stop performance monitor
     */
    void stop();
    
    /**
     * Check if performance monitor is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export metrics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportMetrics(const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get performance monitor health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear metrics for metric name
     * @param name Metric name
     * @return Number of metrics cleared
     */
    int clearMetrics(const std::string& name);
    
    /**
     * Clear all metrics
     * @return Number of metrics cleared
     */
    int clearAllMetrics();
    
    /**
     * Clear expired metrics
     * @return Number of metrics cleared
     */
    int clearExpiredMetrics();
    
    /**
     * Force cleanup of metrics
     */
    void forceCleanup();
    
    /**
     * Get metric statistics by name
     * @return Map of metric names to counts
     */
    std::map<std::string, int> getMetricStatisticsByName();
    
    /**
     * Get metric statistics by type
     * @return Map of metric types to counts
     */
    std::map<MetricType, int> getMetricStatisticsByType();
    
    /**
     * Get performance monitor recommendations
     * @return Vector of performance monitor recommendations
     */
    std::vector<std::string> getPerformanceMonitorRecommendations();
    
    /**
     * Optimize performance monitor
     */
    void optimizePerformanceMonitor();
    
    /**
     * Get performance monitor performance metrics
     * @return Performance metrics
     */
    std::map<std::string, double> getPerformanceMonitorPerformanceMetrics();
    
    /**
     * Force metric cleanup
     * @param name Metric name
     * @return true if metric was cleaned up successfully
     */
    bool forceMetricCleanup(const std::string& name);
    
    /**
     * Get performance monitor capacity
     * @return Performance monitor capacity information
     */
    std::map<std::string, int> getPerformanceMonitorCapacity();
    
    /**
     * Validate performance monitor integrity
     * @return true if performance monitor is valid
     */
    bool validatePerformanceMonitorIntegrity();
    
    /**
     * Get performance monitor issues
     * @return Vector of performance monitor issues
     */
    std::vector<std::string> getPerformanceMonitorIssues();
    
    /**
     * Repair performance monitor
     * @return Number of issues repaired
     */
    int repairPerformanceMonitor();
};

/**
 * Performance alert manager for comprehensive alert handling
 */
class PerformanceAlertManager {
private:
    std::shared_ptr<PerformanceMonitor> monitor_;
    std::vector<PerformanceAlert> alerts_;
    std::mutex alerts_mutex_;
    
public:
    PerformanceAlertManager(std::shared_ptr<PerformanceMonitor> monitor);
    
    /**
     * Create performance alert
     * @param level Alert level
     * @param message Alert message
     * @param metric_name Metric name
     * @param threshold Alert threshold
     * @param current_value Current value
     * @param labels Alert labels
     * @return true if alert was created successfully
     */
    bool createAlert(AlertLevel level,
                    const std::string& message,
                    const std::string& metric_name,
                    double threshold,
                    double current_value,
                    const std::map<std::string, std::string>& labels = {});
    
    /**
     * Get alert information
     * @param id Alert ID
     * @return Alert information or nullptr if not found
     */
    std::shared_ptr<PerformanceAlert> getAlert(const std::string& id);
    
    /**
     * Get all alerts
     * @return Vector of alert information
     */
    std::vector<PerformanceAlert> getAllAlerts();
    
    /**
     * Get alerts by level
     * @param level Alert level
     * @return Vector of alert information
     */
    std::vector<PerformanceAlert> getAlertsByLevel(AlertLevel level);
    
    /**
     * Get alerts by metric
     * @param metric_name Metric name
     * @return Vector of alert information
     */
    std::vector<PerformanceAlert> getAlertsByMetric(const std::string& metric_name);
    
    /**
     * Get alerts by labels
     * @param labels Alert labels
     * @return Vector of alert information
     */
    std::vector<PerformanceAlert> getAlertsByLabels(const std::map<std::string, std::string>& labels);
    
    /**
     * Get alerts by metadata
     * @param metadata Alert metadata
     * @return Vector of alert information
     */
    std::vector<PerformanceAlert> getAlertsByMetadata(const std::map<std::string, std::string>& metadata);
    
    /**
     * Check if alert exists
     * @param id Alert ID
     * @return true if alert exists
     */
    bool hasAlert(const std::string& id);
    
    /**
     * Check if alert is active
     * @param id Alert ID
     * @return true if alert is active
     */
    bool isAlertActive(const std::string& id);
    
    /**
     * Check if alert is expired
     * @param id Alert ID
     * @return true if alert is expired
     */
    bool isAlertExpired(const std::string& id);
    
    /**
     * Get alert level
     * @param id Alert ID
     * @return Alert level or INFO if not found
     */
    AlertLevel getAlertLevel(const std::string& id);
    
    /**
     * Get alert message
     * @param id Alert ID
     * @return Alert message or empty string if not found
     */
    std::string getAlertMessage(const std::string& id);
    
    /**
     * Get alert metric name
     * @param id Alert ID
     * @return Alert metric name or empty string if not found
     */
    std::string getAlertMetricName(const std::string& id);
    
    /**
     * Get alert threshold
     * @param id Alert ID
     * @return Alert threshold or 0.0 if not found
     */
    double getAlertThreshold(const std::string& id);
    
    /**
     * Get alert current value
     * @param id Alert ID
     * @return Alert current value or 0.0 if not found
     */
    double getAlertCurrentValue(const std::string& id);
    
    /**
     * Get alert timestamp
     * @param id Alert ID
     * @return Alert timestamp or default time if not found
     */
    std::chrono::steady_clock::time_point getAlertTimestamp(const std::string& id);
    
    /**
     * Get alert labels
     * @param id Alert ID
     * @return Alert labels or empty map if not found
     */
    std::map<std::string, std::string> getAlertLabels(const std::string& id);
    
    /**
     * Get alert metadata
     * @param id Alert ID
     * @return Alert metadata or empty map if not found
     */
    std::map<std::string, std::string> getAlertMetadata(const std::string& id);
    
    /**
     * Set alert metadata
     * @param id Alert ID
     * @param key Metadata key
     * @param value Metadata value
     * @return true if metadata was set successfully
     */
    bool setAlertMetadata(const std::string& id, const std::string& key, const std::string& value);
    
    /**
     * Remove alert metadata
     * @param id Alert ID
     * @param key Metadata key
     * @return true if metadata was removed successfully
     */
    bool removeAlertMetadata(const std::string& id, const std::string& key);
    
    /**
     * Clear alert metadata
     * @param id Alert ID
     * @return true if metadata was cleared successfully
     */
    bool clearAlertMetadata(const std::string& id);
    
    /**
     * Get alert count
     * @return Alert count
     */
    int getAlertCount();
    
    /**
     * Get alert count by level
     * @param level Alert level
     * @return Alert count
     */
    int getAlertCountByLevel(AlertLevel level);
    
    /**
     * Get alert count by metric
     * @param metric_name Metric name
     * @return Alert count
     */
    int getAlertCountByMetric(const std::string& metric_name);
    
    /**
     * Get alert count by labels
     * @param labels Alert labels
     * @return Alert count
     */
    int getAlertCountByLabels(const std::map<std::string, std::string>& labels);
    
    /**
     * Get alert count by metadata
     * @param metadata Alert metadata
     * @return Alert count
     */
    int getAlertCountByMetadata(const std::map<std::string, std::string>& metadata);
    
    /**
     * Get total alert count
     * @return Total alert count
     */
    int getTotalAlertCount();
    
    /**
     * Get total alert count by level
     * @param level Alert level
     * @return Total alert count
     */
    int getTotalAlertCountByLevel(AlertLevel level);
    
    /**
     * Get total alert count by metric
     * @param metric_name Metric name
     * @return Total alert count
     */
    int getTotalAlertCountByMetric(const std::string& metric_name);
    
    /**
     * Get total alert count by labels
     * @param labels Alert labels
     * @return Total alert count
     */
    int getTotalAlertCountByLabels(const std::map<std::string, std::string>& labels);
    
    /**
     * Get total alert count by metadata
     * @param metadata Alert metadata
     * @return Total alert count
     */
    int getTotalAlertCountByMetadata(const std::map<std::string, std::string>& metadata);
    
    /**
     * Get alert statistics
     * @param id Alert ID
     * @return Alert statistics
     */
    std::map<std::string, double> getAlertStatistics(const std::string& id);
    
    /**
     * Get alert manager statistics
     * @return Alert manager statistics
     */
    std::map<std::string, int> getAlertManagerStatistics();
    
    /**
     * Reset alert manager statistics
     */
    void resetAlertManagerStatistics();
    
    /**
     * Clear alert for alert ID
     * @param id Alert ID
     * @return true if alert was cleared successfully
     */
    bool clearAlert(const std::string& id);
    
    /**
     * Clear all alerts
     * @return Number of alerts cleared
     */
    int clearAllAlerts();
    
    /**
     * Clear expired alerts
     * @return Number of alerts cleared
     */
    int clearExpiredAlerts();
    
    /**
     * Force cleanup of alerts
     */
    void forceCleanup();
    
    /**
     * Get alert statistics by level
     * @return Map of alert levels to counts
     */
    std::map<AlertLevel, int> getAlertStatisticsByLevel();
    
    /**
     * Get alert statistics by metric
     * @return Map of metric names to counts
     */
    std::map<std::string, int> getAlertStatisticsByMetric();
    
    /**
     * Get alert manager recommendations
     * @return Vector of alert manager recommendations
     */
    std::vector<std::string> getAlertManagerRecommendations();
    
    /**
     * Optimize alert manager
     */
    void optimizeAlertManager();
    
    /**
     * Get alert manager performance metrics
     * @return Performance metrics
     */
    std::map<std::string, double> getAlertManagerPerformanceMetrics();
    
    /**
     * Force alert cleanup
     * @param id Alert ID
     * @return true if alert was cleaned up successfully
     */
    bool forceAlertCleanup(const std::string& id);
    
    /**
     * Get alert manager capacity
     * @return Alert manager capacity information
     */
    std::map<std::string, int> getAlertManagerCapacity();
    
    /**
     * Validate alert manager integrity
     * @return true if alert manager is valid
     */
    bool validateAlertManagerIntegrity();
    
    /**
     * Get alert manager issues
     * @return Vector of alert manager issues
     */
    std::vector<std::string> getAlertManagerIssues();
    
    /**
     * Repair alert manager
     * @return Number of issues repaired
     */
    int repairAlertManager();
};

/**
 * Performance monitor factory for different monitoring strategies
 */
class PerformanceMonitorFactory {
public:
    enum class MonitoringStrategy {
        PERFORMANCE,    // Optimized for performance
        RELIABILITY,    // Optimized for reliability
        BALANCED,       // Balanced performance and reliability
        CUSTOM          // Custom monitoring configuration
    };
    
    static std::unique_ptr<PerformanceMonitor> createMonitor(
        MonitoringStrategy strategy,
        const PerformanceMonitor::PerformanceConfig& config = PerformanceMonitor::PerformanceConfig{}
    );
    
    static PerformanceMonitor::PerformanceConfig getDefaultConfig(MonitoringStrategy strategy);
};

/**
 * RAII performance monitor wrapper for automatic performance monitoring
 */
class PerformanceMonitorGuard {
private:
    std::shared_ptr<PerformanceMonitor> monitor_;
    std::string metric_name_;
    std::chrono::steady_clock::time_point start_time_;

public:
    PerformanceMonitorGuard(std::shared_ptr<PerformanceMonitor> monitor, const std::string& metric_name);
    
    /**
     * Record counter metric
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordCounter(double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record gauge metric
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordGauge(double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record histogram metric
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordHistogram(double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record timer metric
     * @param duration Duration
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordTimer(std::chrono::milliseconds duration, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record rate metric
     * @param value Metric value
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordRate(double value, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Record percentile metric
     * @param value Metric value
     * @param percentile Percentile
     * @param labels Metric labels
     * @return true if metric was recorded successfully
     */
    bool recordPercentile(double value, double percentile, const std::map<std::string, std::string>& labels = {});
    
    /**
     * Get metric information
     * @return Vector of metric information
     */
    std::vector<PerformanceMonitor::PerformanceMetric> getMetrics();
    
    /**
     * Get latest metric value
     * @return Latest metric value or 0.0 if not found
     */
    double getLatestMetricValue();
    
    /**
     * Get metric average value
     * @return Average metric value or 0.0 if not found
     */
    double getMetricAverageValue();
    
    /**
     * Get metric minimum value
     * @return Minimum metric value or 0.0 if not found
     */
    double getMetricMinimumValue();
    
    /**
     * Get metric maximum value
     * @return Maximum metric value or 0.0 if not found
     */
    double getMetricMaximumValue();
    
    /**
     * Get metric sum value
     * @return Sum metric value or 0.0 if not found
     */
    double getMetricSumValue();
    
    /**
     * Get metric count
     * @return Metric count or 0 if not found
     */
    int getMetricCount();
    
    /**
     * Get metric type
     * @return Metric type or COUNTER if not found
     */
    PerformanceMonitor::MetricType getMetricType();
    
    /**
     * Get metric labels
     * @return Metric labels or empty map if not found
     */
    std::map<std::string, std::string> getMetricLabels();
    
    /**
     * Get metric metadata
     * @return Metric metadata or empty map if not found
     */
    std::map<std::string, std::string> getMetricMetadata();
    
    /**
     * Set metric metadata
     * @param key Metadata key
     * @param value Metadata value
     * @return true if metadata was set successfully
     */
    bool setMetricMetadata(const std::string& key, const std::string& value);
    
    /**
     * Remove metric metadata
     * @param key Metadata key
     * @return true if metadata was removed successfully
     */
    bool removeMetricMetadata(const std::string& key);
    
    /**
     * Clear metric metadata
     * @return true if metadata was cleared successfully
     */
    bool clearMetricMetadata();
    
    /**
     * Check if metric exists
     * @return true if metric exists
     */
    bool hasMetric();
    
    /**
     * Get metric statistics
     * @return Metric statistics
     */
    std::map<std::string, double> getMetricStatistics();
};

/**
 * Performance monitor analyzer for pattern detection and optimization
 */
class PerformanceMonitorAnalyzer {
private:
    std::shared_ptr<PerformanceMonitor> monitor_;
    
public:
    PerformanceMonitorAnalyzer(std::shared_ptr<PerformanceMonitor> monitor);
    
    /**
     * Analyze performance patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzePerformancePatterns();
    
    /**
     * Detect performance issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectPerformanceIssues();
    
    /**
     * Analyze performance distribution
     * @return Performance distribution analysis
     */
    std::map<std::string, int> analyzePerformanceDistribution();
    
    /**
     * Get performance optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getPerformanceOptimizationRecommendations();
    
    /**
     * Analyze performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzePerformanceTrends();
    
    /**
     * Get performance health score
     * @return Health score (0.0 to 1.0)
     */
    double getPerformanceHealthScore();
    
    /**
     * Get performance efficiency analysis
     * @return Performance efficiency analysis
     */
    std::map<std::string, double> getPerformanceEfficiencyAnalysis();
    
    /**
     * Get performance quality distribution
     * @return Performance quality distribution
     */
    std::map<std::string, int> getPerformanceQualityDistribution();
    
    /**
     * Get performance capacity analysis
     * @return Capacity analysis
     */
    std::map<std::string, double> getPerformanceCapacityAnalysis();
};

} // namespace dht_crawler
