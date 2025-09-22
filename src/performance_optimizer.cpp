#include "performance_optimizer.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace dht_crawler {

PerformanceOptimizer::PerformanceOptimizer()
    : monitoring_enabled_(false)
    , adaptive_optimization_enabled_(false)
    , profiling_enabled_(false)
    , strategy_(OptimizationStrategy::ADAPTIVE)
    , current_thread_count_(8)
    , current_max_connections_(200)
    , current_cache_size_(10000)
    , current_batch_size_(1000)
{
    loadDefaultConfig();
}

PerformanceOptimizer::~PerformanceOptimizer() {
    stopMonitoring();
}

void PerformanceOptimizer::startMonitoring() {
    monitoring_enabled_ = true;
    logOptimization("MONITORING", "Started performance monitoring");
}

void PerformanceOptimizer::stopMonitoring() {
    monitoring_enabled_ = false;
    logOptimization("MONITORING", "Stopped performance monitoring");
}

bool PerformanceOptimizer::isMonitoring() const {
    return monitoring_enabled_;
}

PerformanceOptimizer::PerformanceMetrics PerformanceOptimizer::getCurrentMetrics() {
    PerformanceMetrics metrics;
    collectSystemMetrics(metrics);
    return metrics;
}

std::vector<PerformanceOptimizer::PerformanceMetrics> PerformanceOptimizer::getHistoricalMetrics(int count) {
    if (count <= 0 || metrics_history_.empty()) {
        return metrics_history_;
    }
    
    int start = std::max(0, static_cast<int>(metrics_history_.size()) - count);
    return std::vector<PerformanceMetrics>(
        metrics_history_.begin() + start, 
        metrics_history_.end()
    );
}

void PerformanceOptimizer::recordMetric(const PerformanceMetrics& metric) {
    if (!monitoring_enabled_) {
        return;
    }
    
    metrics_history_.push_back(metric);
    
    // Keep only last 1000 metrics
    if (metrics_history_.size() > 1000) {
        metrics_history_.erase(metrics_history_.begin());
    }
    
    // Check performance alerts
    checkPerformanceAlerts(metric);
    
    // Perform adaptive optimization if enabled
    if (adaptive_optimization_enabled_) {
        analyzePerformance();
    }
}

void PerformanceOptimizer::optimize() {
    switch (strategy_) {
        case OptimizationStrategy::ADAPTIVE:
            adaptiveOptimization();
            break;
        case OptimizationStrategy::MANUAL:
            // Manual optimization - no automatic changes
            break;
        case OptimizationStrategy::PROFILE_BASED:
            profileBasedOptimization();
            break;
        case OptimizationStrategy::ML_BASED:
            mlBasedOptimization();
            break;
    }
    
    last_optimization_ = std::chrono::steady_clock::now();
}

void PerformanceOptimizer::setOptimizationStrategy(OptimizationStrategy strategy) {
    strategy_ = strategy;
    logOptimization("STRATEGY", "Changed optimization strategy");
}

PerformanceOptimizer::OptimizationStrategy PerformanceOptimizer::getOptimizationStrategy() const {
    return strategy_;
}

void PerformanceOptimizer::enableAdaptiveOptimization(bool enable) {
    adaptive_optimization_enabled_ = enable;
    logOptimization("ADAPTIVE", enable ? "Enabled" : "Disabled");
}

bool PerformanceOptimizer::isAdaptiveOptimizationEnabled() const {
    return adaptive_optimization_enabled_;
}

void PerformanceOptimizer::setAdaptiveThresholds(const std::map<std::string, double>& thresholds) {
    adaptive_thresholds_ = thresholds;
    logOptimization("THRESHOLDS", "Updated adaptive thresholds");
}

bool PerformanceOptimizer::isPerformanceDegraded() const {
    if (metrics_history_.empty()) {
        return false;
    }
    
    const auto& latest = metrics_history_.back();
    
    // Check various performance indicators
    if (latest.cpu_usage_percent > 80.0) return true;
    if (latest.memory_usage_mb > 800.0) return true;
    if (latest.average_response_time_ms > 5000.0) return true;
    if (latest.success_rate_percent < 70.0) return true;
    
    return false;
}

std::string PerformanceOptimizer::getPerformanceBottleneck() const {
    if (metrics_history_.empty()) {
        return "Unknown";
    }
    
    const auto& latest = metrics_history_.back();
    
    if (isCPUBottleneck()) return "CPU";
    if (isMemoryBottleneck()) return "Memory";
    if (isNetworkBottleneck()) return "Network";
    if (isDatabaseBottleneck()) return "Database";
    
    return "None";
}

std::vector<std::string> PerformanceOptimizer::getOptimizationRecommendations() const {
    std::vector<std::string> recommendations;
    
    if (isCPUBottleneck()) {
        recommendations.push_back("Reduce thread count");
        recommendations.push_back("Optimize CPU-intensive operations");
        recommendations.push_back("Use more efficient algorithms");
    }
    
    if (isMemoryBottleneck()) {
        recommendations.push_back("Reduce cache size");
        recommendations.push_back("Implement memory pooling");
        recommendations.push_back("Optimize data structures");
    }
    
    if (isNetworkBottleneck()) {
        recommendations.push_back("Reduce connection limits");
        recommendations.push_back("Implement connection pooling");
        recommendations.push_back("Optimize network protocols");
    }
    
    if (isDatabaseBottleneck()) {
        recommendations.push_back("Reduce batch size");
        recommendations.push_back("Optimize database queries");
        recommendations.push_back("Implement database connection pooling");
    }
    
    return recommendations;
}

void PerformanceOptimizer::optimizeMemoryUsage() {
    if (isMemoryBottleneck()) {
        current_cache_size_ = calculateOptimalCacheSize();
        logOptimization("MEMORY", "Optimized memory usage");
    }
}

void PerformanceOptimizer::optimizeCPUUsage() {
    if (isCPUBottleneck()) {
        current_thread_count_ = calculateOptimalThreadCount();
        logOptimization("CPU", "Optimized CPU usage");
    }
}

void PerformanceOptimizer::optimizeNetworkUsage() {
    if (isNetworkBottleneck()) {
        auto limits = calculateOptimalConnectionLimits();
        current_max_connections_ = limits.first;
        logOptimization("NETWORK", "Optimized network usage");
    }
}

void PerformanceOptimizer::optimizeDatabaseUsage() {
    if (isDatabaseBottleneck()) {
        current_batch_size_ = calculateOptimalBatchSize();
        logOptimization("DATABASE", "Optimized database usage");
    }
}

void PerformanceOptimizer::optimizeThreadPool() {
    current_thread_count_ = calculateOptimalThreadCount();
    logOptimization("THREADS", "Optimized thread pool");
}

void PerformanceOptimizer::setOptimalThreadCount() {
    current_thread_count_ = calculateOptimalThreadCount();
}

int PerformanceOptimizer::getOptimalThreadCount() const {
    return calculateOptimalThreadCount();
}

void PerformanceOptimizer::optimizeConnections() {
    auto limits = calculateOptimalConnectionLimits();
    current_max_connections_ = limits.first;
    logOptimization("CONNECTIONS", "Optimized connection limits");
}

void PerformanceOptimizer::setOptimalConnectionLimits() {
    auto limits = calculateOptimalConnectionLimits();
    current_max_connections_ = limits.first;
}

std::pair<int, int> PerformanceOptimizer::getOptimalConnectionLimits() const {
    return calculateOptimalConnectionLimits();
}

void PerformanceOptimizer::optimizeCache() {
    current_cache_size_ = calculateOptimalCacheSize();
    logOptimization("CACHE", "Optimized cache size");
}

void PerformanceOptimizer::setOptimalCacheSize() {
    current_cache_size_ = calculateOptimalCacheSize();
}

size_t PerformanceOptimizer::getOptimalCacheSize() const {
    return calculateOptimalCacheSize();
}

void PerformanceOptimizer::optimizeDatabase() {
    current_batch_size_ = calculateOptimalBatchSize();
    logOptimization("DATABASE", "Optimized database settings");
}

void PerformanceOptimizer::setOptimalBatchSize() {
    current_batch_size_ = calculateOptimalBatchSize();
}

int PerformanceOptimizer::getOptimalBatchSize() const {
    return calculateOptimalBatchSize();
}

void PerformanceOptimizer::startProfiling() {
    profiling_enabled_ = true;
    logOptimization("PROFILING", "Started performance profiling");
}

void PerformanceOptimizer::stopProfiling() {
    profiling_enabled_ = false;
    logOptimization("PROFILING", "Stopped performance profiling");
}

bool PerformanceOptimizer::isProfiling() const {
    return profiling_enabled_;
}

std::string PerformanceOptimizer::getProfilingReport() const {
    if (!profiling_enabled_ || metrics_history_.empty()) {
        return "No profiling data available";
    }
    
    std::ostringstream report;
    report << "Performance Profiling Report\n";
    report << "==========================\n\n";
    
    report << "Total Metrics Collected: " << metrics_history_.size() << "\n";
    report << "Average CPU Usage: " << getAverageCPUUsage() << "%\n";
    report << "Average Memory Usage: " << getAverageMemoryUsage() << " MB\n";
    report << "Average Response Time: " << getAverageResponseTime() << " ms\n";
    report << "Average Success Rate: " << getAverageSuccessRate() << "%\n\n";
    
    report << "Current Bottleneck: " << getPerformanceBottleneck() << "\n";
    report << "Performance Degraded: " << (isPerformanceDegraded() ? "Yes" : "No") << "\n\n";
    
    auto recommendations = getOptimizationRecommendations();
    if (!recommendations.empty()) {
        report << "Recommendations:\n";
        for (const auto& rec : recommendations) {
            report << "- " << rec << "\n";
        }
    }
    
    return report.str();
}

void PerformanceOptimizer::loadOptimizationConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open optimization config file: " << filename << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            try {
                double threshold = std::stod(value);
                adaptive_thresholds_[key] = threshold;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing threshold for " << key << ": " << value << std::endl;
            }
        }
    }
    
    file.close();
    validateConfig();
}

void PerformanceOptimizer::saveOptimizationConfig(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to create optimization config file: " << filename << std::endl;
        return;
    }
    
    file << "# Performance Optimization Configuration" << std::endl;
    file << "# Generated automatically" << std::endl;
    file << std::endl;
    
    for (const auto& pair : adaptive_thresholds_) {
        file << pair.first << "=" << pair.second << std::endl;
    }
    
    file.close();
}

double PerformanceOptimizer::getAverageCPUUsage() const {
    if (metrics_history_.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& metric : metrics_history_) {
        sum += metric.cpu_usage_percent;
    }
    return sum / metrics_history_.size();
}

double PerformanceOptimizer::getAverageMemoryUsage() const {
    if (metrics_history_.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& metric : metrics_history_) {
        sum += metric.memory_usage_mb;
    }
    return sum / metrics_history_.size();
}

double PerformanceOptimizer::getAverageResponseTime() const {
    if (metrics_history_.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& metric : metrics_history_) {
        sum += metric.average_response_time_ms;
    }
    return sum / metrics_history_.size();
}

double PerformanceOptimizer::getAverageSuccessRate() const {
    if (metrics_history_.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& metric : metrics_history_) {
        sum += metric.success_rate_percent;
    }
    return sum / metrics_history_.size();
}

void PerformanceOptimizer::setPerformanceAlert(const std::string& metric, double threshold, 
                                                std::function<void()> callback) {
    adaptive_thresholds_[metric] = threshold;
    performance_alerts_[metric] = callback;
}

void PerformanceOptimizer::clearPerformanceAlert(const std::string& metric) {
    performance_alerts_.erase(metric);
}

void PerformanceOptimizer::collectSystemMetrics(PerformanceMetrics& metrics) {
    // This is a simplified implementation
    // In a real implementation, you would collect actual system metrics
    
    metrics.cpu_usage_percent = 25.0;  // Placeholder
    metrics.memory_usage_mb = 512.0;    // Placeholder
    metrics.network_throughput_mbps = 100.0;  // Placeholder
    metrics.average_response_time_ms = 150.0;  // Placeholder
    metrics.success_rate_percent = 95.0;  // Placeholder
    metrics.active_connections = 50;  // Placeholder
    metrics.queued_requests = 10;  // Placeholder
    metrics.timestamp = std::chrono::steady_clock::now();
}

void PerformanceOptimizer::analyzePerformance() {
    if (metrics_history_.empty()) return;
    
    const auto& latest = metrics_history_.back();
    
    // Analyze performance trends
    if (latest.cpu_usage_percent > 80.0) {
        optimizeCPUUsage();
    }
    
    if (latest.memory_usage_mb > 800.0) {
        optimizeMemoryUsage();
    }
    
    if (latest.average_response_time_ms > 5000.0) {
        optimizeNetworkUsage();
    }
    
    if (latest.success_rate_percent < 70.0) {
        optimizeDatabaseUsage();
    }
}

void PerformanceOptimizer::applyOptimizations() {
    // Apply the calculated optimizations
    updateConfiguration();
}

void PerformanceOptimizer::updateConfiguration() {
    // Update the configuration with optimized values
    // This would typically involve updating the PerformanceConfig
}

void PerformanceOptimizer::checkPerformanceAlerts(const PerformanceMetrics& metrics) {
    for (const auto& alert : performance_alerts_) {
        auto it = adaptive_thresholds_.find(alert.first);
        if (it != adaptive_thresholds_.end()) {
            double threshold = it->second;
            double current_value = 0.0;
            
            if (alert.first == "cpu_usage") {
                current_value = metrics.cpu_usage_percent;
            } else if (alert.first == "memory_usage") {
                current_value = metrics.memory_usage_mb;
            } else if (alert.first == "response_time") {
                current_value = metrics.average_response_time_ms;
            } else if (alert.first == "success_rate") {
                current_value = metrics.success_rate_percent;
            }
            
            if (current_value > threshold) {
                alert.second();  // Call the alert callback
            }
        }
    }
}

void PerformanceOptimizer::adaptiveOptimization() {
    analyzePerformance();
    applyOptimizations();
}

void PerformanceOptimizer::profileBasedOptimization() {
    // Profile-based optimization logic
    // This would use predefined optimization profiles
}

void PerformanceOptimizer::mlBasedOptimization() {
    // Machine learning based optimization logic
    // This would use ML models to predict optimal settings
}

double PerformanceOptimizer::calculateOptimalThreadCount() const {
    // Simple heuristic: use number of CPU cores
    int cores = std::thread::hardware_concurrency();
    return std::max(2, std::min(cores, 16));
}

std::pair<int, int> PerformanceOptimizer::calculateOptimalConnectionLimits() const {
    // Simple heuristic based on system resources
    int max_connections = 200;
    int max_active = 1000;
    
    if (isMemoryBottleneck()) {
        max_connections = 100;
        max_active = 500;
    } else if (isNetworkBottleneck()) {
        max_connections = 50;
        max_active = 200;
    }
    
    return {max_connections, max_active};
}

size_t PerformanceOptimizer::calculateOptimalCacheSize() const {
    // Simple heuristic based on memory usage
    size_t base_size = 10000;
    
    if (isMemoryBottleneck()) {
        return base_size / 2;
    } else if (getAverageMemoryUsage() < 200.0) {
        return base_size * 2;
    }
    
    return base_size;
}

int PerformanceOptimizer::calculateOptimalBatchSize() const {
    // Simple heuristic based on database performance
    int base_size = 1000;
    
    if (isDatabaseBottleneck()) {
        return base_size / 2;
    } else if (getAverageResponseTime() < 100.0) {
        return base_size * 2;
    }
    
    return base_size;
}

bool PerformanceOptimizer::isCPUBottleneck() const {
    if (metrics_history_.empty()) return false;
    
    const auto& latest = metrics_history_.back();
    return latest.cpu_usage_percent > 80.0;
}

bool PerformanceOptimizer::isMemoryBottleneck() const {
    if (metrics_history_.empty()) return false;
    
    const auto& latest = metrics_history_.back();
    return latest.memory_usage_mb > 800.0;
}

bool PerformanceOptimizer::isNetworkBottleneck() const {
    if (metrics_history_.empty()) return false;
    
    const auto& latest = metrics_history_.back();
    return latest.average_response_time_ms > 5000.0;
}

bool PerformanceOptimizer::isDatabaseBottleneck() const {
    if (metrics_history_.empty()) return false;
    
    const auto& latest = metrics_history_.back();
    return latest.success_rate_percent < 70.0;
}

void PerformanceOptimizer::loadDefaultConfig() {
    adaptive_thresholds_["cpu_usage"] = 80.0;
    adaptive_thresholds_["memory_usage"] = 800.0;
    adaptive_thresholds_["response_time"] = 5000.0;
    adaptive_thresholds_["success_rate"] = 70.0;
}

void PerformanceOptimizer::validateConfig() {
    // Validate configuration values
    for (const auto& threshold : adaptive_thresholds_) {
        if (threshold.second < 0.0 || threshold.second > 100.0) {
            std::cerr << "Warning: Threshold " << threshold.first 
                      << " value " << threshold.second 
                      << " is outside valid range [0, 100]" << std::endl;
        }
    }
}

void PerformanceOptimizer::logOptimization(const std::string& action, const std::string& details) {
    std::cout << "[PERF_OPTIMIZER] " << action << ": " << details << std::endl;
}

} // namespace dht_crawler
