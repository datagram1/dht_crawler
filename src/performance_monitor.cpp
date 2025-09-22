#include "performance_monitor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace dht_crawler {

PerformanceMonitor::PerformanceMonitor(const PerformanceConfig& config) : config_(config), should_stop_(false) {
    monitor_thread_ = std::thread(&PerformanceMonitor::monitorLoop, this);
}

PerformanceMonitor::~PerformanceMonitor() {
    stop();
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void PerformanceMonitor::monitorLoop() {
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
        
        // Monitor performance
        monitorPerformance();
        
        // Cleanup expired metrics
        cleanupExpiredMetrics();
        
        // Update statistics
        updateStatistics();
    }
}

void PerformanceMonitor::monitorPerformance() {
    // Check performance thresholds
    checkPerformanceThresholds();
    
    // Generate performance alerts
    generatePerformanceAlerts();
    
    // Analyze performance trends
    analyzePerformanceTrends();
    
    // Optimize performance
    optimizePerformance();
}

void PerformanceMonitor::cleanupExpiredMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : metrics_) {
        auto& metric_list = pair.second;
        for (auto it = metric_list.begin(); it != metric_list.end();) {
            if (isMetricExpired(*it)) {
                it = metric_list.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void PerformanceMonitor::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_metrics = 0;
    stats_.counter_metrics = 0;
    stats_.gauge_metrics = 0;
    stats_.histogram_metrics = 0;
    stats_.timer_metrics = 0;
    stats_.rate_metrics = 0;
    stats_.percentile_metrics = 0;
    stats_.total_alerts = 0;
    stats_.info_alerts = 0;
    stats_.warning_alerts = 0;
    stats_.error_alerts = 0;
    stats_.critical_alerts = 0;
    
    double total_metric_value = 0.0;
    double min_metric_value = std::numeric_limits<double>::max();
    double max_metric_value = std::numeric_limits<double>::min();
    
    std::chrono::milliseconds total_metric_age(0);
    std::chrono::milliseconds min_metric_age(std::numeric_limits<long>::max());
    std::chrono::milliseconds max_metric_age(0);
    
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& pair : metrics_) {
        for (const auto& metric : pair.second) {
            stats_.total_metrics++;
            
            switch (metric.type) {
                case MetricType::COUNTER:
                    stats_.counter_metrics++;
                    break;
                case MetricType::GAUGE:
                    stats_.gauge_metrics++;
                    break;
                case MetricType::HISTOGRAM:
                    stats_.histogram_metrics++;
                    break;
                case MetricType::TIMER:
                    stats_.timer_metrics++;
                    break;
                case MetricType::RATE:
                    stats_.rate_metrics++;
                    break;
                case MetricType::PERCENTILE:
                    stats_.percentile_metrics++;
                    break;
            }
            
            total_metric_value += metric.value;
            min_metric_value = std::min(min_metric_value, metric.value);
            max_metric_value = std::max(max_metric_value, metric.value);
            
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - metric.timestamp);
            total_metric_age += age;
            min_metric_age = std::min(min_metric_age, age);
            max_metric_age = std::max(max_metric_age, age);
        }
    }
    
    if (stats_.total_metrics > 0) {
        stats_.avg_metric_value = total_metric_value / stats_.total_metrics;
        stats_.min_metric_value = min_metric_value;
        stats_.max_metric_value = max_metric_value;
        stats_.avg_metric_age = std::chrono::milliseconds(total_metric_age.count() / stats_.total_metrics);
        stats_.min_metric_age = min_metric_age;
        stats_.max_metric_age = max_metric_age;
    }
    
    // Update alerts statistics
    for (const auto& alert : alerts_) {
        stats_.total_alerts++;
        
        switch (alert.level) {
            case AlertLevel::INFO:
                stats_.info_alerts++;
                break;
            case AlertLevel::WARNING:
                stats_.warning_alerts++;
                break;
            case AlertLevel::ERROR:
                stats_.error_alerts++;
                break;
            case AlertLevel::CRITICAL:
                stats_.critical_alerts++;
                break;
        }
    }
    
    stats_.last_update = now;
}

void PerformanceMonitor::checkPerformanceThresholds() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    for (const auto& pair : metrics_) {
        const auto& metric_name = pair.first;
        const auto& metric_list = pair.second;
        
        if (!metric_list.empty()) {
            const auto& latest_metric = metric_list.back();
            
            // Check if metric exceeds threshold
            if (latest_metric.value > config_.performance_threshold) {
                // Generate alert
                PerformanceAlert alert;
                alert.id = generateAlertId();
                alert.level = AlertLevel::WARNING;
                alert.message = "Performance threshold exceeded for metric: " + metric_name;
                alert.metric_name = metric_name;
                alert.threshold = config_.performance_threshold;
                alert.current_value = latest_metric.value;
                alert.timestamp = std::chrono::steady_clock::now();
                alert.labels = latest_metric.labels;
                alert.metadata = latest_metric.metadata;
                
                alerts_.push_back(alert);
            }
        }
    }
}

void PerformanceMonitor::generatePerformanceAlerts() {
    // Check for critical performance issues
    if (stats_.total_metrics > config_.max_metrics_per_type) {
        PerformanceAlert alert;
        alert.id = generateAlertId();
        alert.level = AlertLevel::CRITICAL;
        alert.message = "Too many metrics collected";
        alert.metric_name = "system";
        alert.threshold = config_.max_metrics_per_type;
        alert.current_value = stats_.total_metrics;
        alert.timestamp = std::chrono::steady_clock::now();
        
        alerts_.push_back(alert);
    }
}

void PerformanceMonitor::analyzePerformanceTrends() {
    // Analyze performance trends over time
    // This is a simplified implementation
}

void PerformanceMonitor::optimizePerformance() {
    // Optimize performance based on collected metrics
    // This is a simplified implementation
}

bool PerformanceMonitor::recordCounter(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PerformanceMetric metric;
    metric.name = name;
    metric.type = MetricType::COUNTER;
    metric.value = value;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.labels = labels;
    
    metrics_[name].push_back(metric);
    
    // Limit number of metrics per name
    if (metrics_[name].size() > config_.max_metrics_per_type) {
        metrics_[name].erase(metrics_[name].begin());
    }
    
    return true;
}

bool PerformanceMonitor::recordGauge(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PerformanceMetric metric;
    metric.name = name;
    metric.type = MetricType::GAUGE;
    metric.value = value;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.labels = labels;
    
    metrics_[name].push_back(metric);
    
    // Limit number of metrics per name
    if (metrics_[name].size() > config_.max_metrics_per_type) {
        metrics_[name].erase(metrics_[name].begin());
    }
    
    return true;
}

bool PerformanceMonitor::recordHistogram(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PerformanceMetric metric;
    metric.name = name;
    metric.type = MetricType::HISTOGRAM;
    metric.value = value;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.labels = labels;
    
    metrics_[name].push_back(metric);
    
    // Limit number of metrics per name
    if (metrics_[name].size() > config_.max_metrics_per_type) {
        metrics_[name].erase(metrics_[name].begin());
    }
    
    return true;
}

bool PerformanceMonitor::recordTimer(const std::string& name, std::chrono::milliseconds duration, const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PerformanceMetric metric;
    metric.name = name;
    metric.type = MetricType::TIMER;
    metric.value = duration.count();
    metric.timestamp = std::chrono::steady_clock::now();
    metric.labels = labels;
    
    metrics_[name].push_back(metric);
    
    // Limit number of metrics per name
    if (metrics_[name].size() > config_.max_metrics_per_type) {
        metrics_[name].erase(metrics_[name].begin());
    }
    
    return true;
}

bool PerformanceMonitor::recordRate(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PerformanceMetric metric;
    metric.name = name;
    metric.type = MetricType::RATE;
    metric.value = value;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.labels = labels;
    
    metrics_[name].push_back(metric);
    
    // Limit number of metrics per name
    if (metrics_[name].size() > config_.max_metrics_per_type) {
        metrics_[name].erase(metrics_[name].begin());
    }
    
    return true;
}

bool PerformanceMonitor::recordPercentile(const std::string& name, double value, double percentile, const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    PerformanceMetric metric;
    metric.name = name;
    metric.type = MetricType::PERCENTILE;
    metric.value = value;
    metric.timestamp = std::chrono::steady_clock::now();
    metric.labels = labels;
    metric.metadata["percentile"] = std::to_string(percentile);
    
    metrics_[name].push_back(metric);
    
    // Limit number of metrics per name
    if (metrics_[name].size() > config_.max_metrics_per_type) {
        metrics_[name].erase(metrics_[name].begin());
    }
    
    return true;
}

std::vector<PerformanceMonitor::PerformanceMetric> PerformanceMonitor::getMetrics(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
        return it->second;
    }
    
    return {};
}

double PerformanceMonitor::getLatestMetricValue(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        return it->second.back().value;
    }
    
    return 0.0;
}

double PerformanceMonitor::getMetricAverageValue(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        double sum = 0.0;
        for (const auto& metric : it->second) {
            sum += metric.value;
        }
        return sum / it->second.size();
    }
    
    return 0.0;
}

double PerformanceMonitor::getMetricMinimumValue(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        double min_value = it->second[0].value;
        for (const auto& metric : it->second) {
            min_value = std::min(min_value, metric.value);
        }
        return min_value;
    }
    
    return 0.0;
}

double PerformanceMonitor::getMetricMaximumValue(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        double max_value = it->second[0].value;
        for (const auto& metric : it->second) {
            max_value = std::max(max_value, metric.value);
        }
        return max_value;
    }
    
    return 0.0;
}

double PerformanceMonitor::getMetricSumValue(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        double sum = 0.0;
        for (const auto& metric : it->second) {
            sum += metric.value;
        }
        return sum;
    }
    
    return 0.0;
}

int PerformanceMonitor::getMetricCount(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
        return static_cast<int>(it->second.size());
    }
    
    return 0;
}

PerformanceMonitor::MetricType PerformanceMonitor::getMetricType(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        return it->second.back().type;
    }
    
    return MetricType::COUNTER;
}

std::map<std::string, std::string> PerformanceMonitor::getMetricLabels(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        return it->second.back().labels;
    }
    
    return {};
}

std::map<std::string, std::string> PerformanceMonitor::getMetricMetadata(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        return it->second.back().metadata;
    }
    
    return {};
}

bool PerformanceMonitor::setMetricMetadata(const std::string& name, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        it->second.back().metadata[key] = value;
        return true;
    }
    
    return false;
}

bool PerformanceMonitor::removeMetricMetadata(const std::string& name, const std::string& key) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        it->second.back().metadata.erase(key);
        return true;
    }
    
    return false;
}

bool PerformanceMonitor::clearMetricMetadata(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end() && !it->second.empty()) {
        it->second.back().metadata.clear();
        return true;
    }
    
    return false;
}

bool PerformanceMonitor::hasMetric(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_.find(name) != metrics_.end();
}

std::vector<std::string> PerformanceMonitor::getAllMetricNames() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : metrics_) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::vector<std::string> PerformanceMonitor::getMetricsByType(MetricType type) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : metrics_) {
        if (!pair.second.empty() && pair.second.back().type == type) {
            names.push_back(pair.first);
        }
    }
    
    return names;
}

std::vector<std::string> PerformanceMonitor::getMetricsByLabels(const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : metrics_) {
        if (!pair.second.empty()) {
            const auto& metric_labels = pair.second.back().labels;
            bool matches = true;
            for (const auto& label : labels) {
                if (metric_labels.find(label.first) == metric_labels.end() || 
                    metric_labels.at(label.first) != label.second) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                names.push_back(pair.first);
            }
        }
    }
    
    return names;
}

std::vector<std::string> PerformanceMonitor::getMetricsByMetadata(const std::map<std::string, std::string>& metadata) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : metrics_) {
        if (!pair.second.empty()) {
            const auto& metric_metadata = pair.second.back().metadata;
            bool matches = true;
            for (const auto& meta : metadata) {
                if (metric_metadata.find(meta.first) == metric_metadata.end() || 
                    metric_metadata.at(meta.first) != meta.second) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                names.push_back(pair.first);
            }
        }
    }
    
    return names;
}

int PerformanceMonitor::getMetricCountByType(MetricType type) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    int count = 0;
    for (const auto& pair : metrics_) {
        if (!pair.second.empty() && pair.second.back().type == type) {
            count++;
        }
    }
    
    return count;
}

int PerformanceMonitor::getMetricCountByLabels(const std::map<std::string, std::string>& labels) {
    return static_cast<int>(getMetricsByLabels(labels).size());
}

int PerformanceMonitor::getMetricCountByMetadata(const std::map<std::string, std::string>& metadata) {
    return static_cast<int>(getMetricsByMetadata(metadata).size());
}

int PerformanceMonitor::getTotalMetricCount() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    int count = 0;
    for (const auto& pair : metrics_) {
        count += static_cast<int>(pair.second.size());
    }
    
    return count;
}

int PerformanceMonitor::getTotalMetricCountByType(MetricType type) {
    return getMetricCountByType(type);
}

int PerformanceMonitor::getTotalMetricCountByLabels(const std::map<std::string, std::string>& labels) {
    return getMetricCountByLabels(labels);
}

int PerformanceMonitor::getTotalMetricCountByMetadata(const std::map<std::string, std::string>& metadata) {
    return getMetricCountByMetadata(metadata);
}

std::map<std::string, double> PerformanceMonitor::getMetricStatistics(const std::string& name) {
    std::map<std::string, double> stats;
    
    stats["latest"] = getLatestMetricValue(name);
    stats["average"] = getMetricAverageValue(name);
    stats["minimum"] = getMetricMinimumValue(name);
    stats["maximum"] = getMetricMaximumValue(name);
    stats["sum"] = getMetricSumValue(name);
    stats["count"] = getMetricCount(name);
    
    return stats;
}

PerformanceMonitor::PerformanceStatistics PerformanceMonitor::getStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void PerformanceMonitor::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = PerformanceStatistics{};
}

void PerformanceMonitor::updateConfig(const PerformanceConfig& config) {
    config_ = config;
}

PerformanceMonitor::PerformanceConfig PerformanceMonitor::getConfig() const {
    return config_;
}

void PerformanceMonitor::start() {
    should_stop_ = false;
    if (!monitor_thread_.joinable()) {
        monitor_thread_ = std::thread(&PerformanceMonitor::monitorLoop, this);
    }
}

void PerformanceMonitor::stop() {
    should_stop_ = true;
    monitor_condition_.notify_all();
}

bool PerformanceMonitor::isRunning() const {
    return !should_stop_ && monitor_thread_.joinable();
}

bool PerformanceMonitor::exportMetrics(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    file << "MetricName,Type,Value,Timestamp,Labels,Metadata" << std::endl;
    
    for (const auto& pair : metrics_) {
        for (const auto& metric : pair.second) {
            file << metric.name << "," << static_cast<int>(metric.type) << "," << metric.value << ","
                 << metric.timestamp.time_since_epoch().count() << ",";
            
            // Write labels
            for (const auto& label : metric.labels) {
                file << label.first << "=" << label.second << ";";
            }
            file << ",";
            
            // Write metadata
            for (const auto& meta : metric.metadata) {
                file << meta.first << "=" << meta.second << ";";
            }
            file << std::endl;
        }
    }
    
    file.close();
    return true;
}

bool PerformanceMonitor::exportStatistics(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    file << "Metric,Value" << std::endl;
    file << "TotalMetrics," << stats_.total_metrics << std::endl;
    file << "CounterMetrics," << stats_.counter_metrics << std::endl;
    file << "GaugeMetrics," << stats_.gauge_metrics << std::endl;
    file << "HistogramMetrics," << stats_.histogram_metrics << std::endl;
    file << "TimerMetrics," << stats_.timer_metrics << std::endl;
    file << "RateMetrics," << stats_.rate_metrics << std::endl;
    file << "PercentileMetrics," << stats_.percentile_metrics << std::endl;
    file << "TotalAlerts," << stats_.total_alerts << std::endl;
    file << "InfoAlerts," << stats_.info_alerts << std::endl;
    file << "WarningAlerts," << stats_.warning_alerts << std::endl;
    file << "ErrorAlerts," << stats_.error_alerts << std::endl;
    file << "CriticalAlerts," << stats_.critical_alerts << std::endl;
    file << "AverageMetricValue," << stats_.avg_metric_value << std::endl;
    file << "MinMetricValue," << stats_.min_metric_value << std::endl;
    file << "MaxMetricValue," << stats_.max_metric_value << std::endl;
    
    file.close();
    return true;
}

std::map<std::string, std::string> PerformanceMonitor::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["total_metrics"] = std::to_string(getTotalMetricCount());
    status["total_alerts"] = std::to_string(stats_.total_alerts);
    status["performance_threshold"] = std::to_string(config_.performance_threshold);
    status["alert_threshold"] = std::to_string(config_.alert_threshold);
    status["max_metrics_per_type"] = std::to_string(config_.max_metrics_per_type);
    status["max_alerts_per_level"] = std::to_string(config_.max_alerts_per_level);
    status["is_running"] = isRunning() ? "true" : "false";
    
    return status;
}

int PerformanceMonitor::clearMetrics(const std::string& name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
        int count = static_cast<int>(it->second.size());
        metrics_.erase(it);
        return count;
    }
    
    return 0;
}

int PerformanceMonitor::clearAllMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    int total_count = 0;
    for (const auto& pair : metrics_) {
        total_count += static_cast<int>(pair.second.size());
    }
    
    metrics_.clear();
    return total_count;
}

int PerformanceMonitor::clearExpiredMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    int cleared_count = 0;
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : metrics_) {
        auto& metric_list = pair.second;
        for (auto it = metric_list.begin(); it != metric_list.end();) {
            if (isMetricExpired(*it)) {
                it = metric_list.erase(it);
                cleared_count++;
            } else {
                ++it;
            }
        }
    }
    
    return cleared_count;
}

void PerformanceMonitor::forceCleanup() {
    clearExpiredMetrics();
}

std::map<std::string, int> PerformanceMonitor::getMetricStatisticsByName() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::map<std::string, int> stats;
    for (const auto& pair : metrics_) {
        stats[pair.first] = static_cast<int>(pair.second.size());
    }
    
    return stats;
}

std::map<PerformanceMonitor::MetricType, int> PerformanceMonitor::getMetricStatisticsByType() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    std::map<MetricType, int> stats;
    for (const auto& pair : metrics_) {
        if (!pair.second.empty()) {
            stats[pair.second.back().type]++;
        }
    }
    
    return stats;
}

std::vector<std::string> PerformanceMonitor::getPerformanceMonitorRecommendations() {
    std::vector<std::string> recommendations;
    
    if (getTotalMetricCount() > config_.max_metrics_per_type * 10) {
        recommendations.push_back("Consider reducing metric collection frequency");
    }
    
    if (stats_.total_alerts > config_.max_alerts_per_level * 5) {
        recommendations.push_back("High alert count, consider adjusting thresholds");
    }
    
    if (stats_.avg_metric_value > config_.performance_threshold * 2) {
        recommendations.push_back("Performance metrics are consistently high");
    }
    
    return recommendations;
}

void PerformanceMonitor::optimizePerformanceMonitor() {
    // Optimize performance monitor based on current state
    clearExpiredMetrics();
}

std::map<std::string, double> PerformanceMonitor::getPerformanceMonitorPerformanceMetrics() {
    std::map<std::string, double> metrics;
    
    metrics["total_metrics"] = getTotalMetricCount();
    metrics["total_alerts"] = stats_.total_alerts;
    metrics["avg_metric_value"] = stats_.avg_metric_value;
    metrics["performance_threshold"] = config_.performance_threshold;
    
    return metrics;
}

bool PerformanceMonitor::forceMetricCleanup(const std::string& name) {
    return clearMetrics(name) > 0;
}

std::map<std::string, int> PerformanceMonitor::getPerformanceMonitorCapacity() {
    std::map<std::string, int> capacity;
    
    capacity["current_metrics"] = getTotalMetricCount();
    capacity["max_metrics_per_type"] = config_.max_metrics_per_type;
    capacity["available_capacity"] = config_.max_metrics_per_type - getTotalMetricCount();
    capacity["utilization_percent"] = static_cast<int>((getTotalMetricCount() / static_cast<double>(config_.max_metrics_per_type)) * 100);
    
    return capacity;
}

bool PerformanceMonitor::validatePerformanceMonitorIntegrity() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // Check if metrics are within reasonable bounds
    for (const auto& pair : metrics_) {
        for (const auto& metric : pair.second) {
            if (metric.value < 0 || metric.value > 1e9) {
                return false;
            }
        }
    }
    
    return true;
}

std::vector<std::string> PerformanceMonitor::getPerformanceMonitorIssues() {
    std::vector<std::string> issues;
    
    if (!validatePerformanceMonitorIntegrity()) {
        issues.push_back("Performance monitor integrity check failed");
    }
    
    if (getTotalMetricCount() > config_.max_metrics_per_type * 10) {
        issues.push_back("Too many metrics collected");
    }
    
    if (stats_.total_alerts > config_.max_alerts_per_level * 5) {
        issues.push_back("Too many alerts generated");
    }
    
    return issues;
}

int PerformanceMonitor::repairPerformanceMonitor() {
    int repaired_count = 0;
    
    if (!validatePerformanceMonitorIntegrity()) {
        // Clear invalid metrics
        repaired_count += clearAllMetrics();
    }
    
    if (getTotalMetricCount() > config_.max_metrics_per_type * 10) {
        // Clear expired metrics
        repaired_count += clearExpiredMetrics();
    }
    
    return repaired_count;
}

std::string PerformanceMonitor::generateAlertId() {
    static std::atomic<int> counter(0);
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "ALERT_" << timestamp << "_" << counter.fetch_add(1);
    return ss.str();
}

bool PerformanceMonitor::isMetricExpired(const PerformanceMetric& metric) {
    auto now = std::chrono::steady_clock::now();
    return now - metric.timestamp > std::chrono::milliseconds(config_.metrics_retention);
}

std::string PerformanceMonitor::metricTypeToString(MetricType type) {
    switch (type) {
        case MetricType::COUNTER: return "COUNTER";
        case MetricType::GAUGE: return "GAUGE";
        case MetricType::HISTOGRAM: return "HISTOGRAM";
        case MetricType::TIMER: return "TIMER";
        case MetricType::RATE: return "RATE";
        case MetricType::PERCENTILE: return "PERCENTILE";
        default: return "UNKNOWN";
    }
}

std::string PerformanceMonitor::alertLevelToString(AlertLevel level) {
    switch (level) {
        case AlertLevel::INFO: return "INFO";
        case AlertLevel::WARNING: return "WARNING";
        case AlertLevel::ERROR: return "ERROR";
        case AlertLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

} // namespace dht_crawler
