#include "performance_config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace dht_crawler {

// Static member definitions
std::map<std::string, std::string> PerformanceConfig::config_;
std::map<std::string, double> PerformanceConfig::performance_thresholds_;
std::map<std::string, size_t> PerformanceConfig::resource_limits_;
PerformanceConfig::OptimizationProfile PerformanceConfig::current_profile_ = OptimizationProfile::BALANCED;
bool PerformanceConfig::performance_monitoring_enabled_ = true;

void PerformanceConfig::optimizeForHighThroughput() {
    setOptimizationProfile(OptimizationProfile::HIGH_THROUGHPUT);
}

void PerformanceConfig::optimizeForLowLatency() {
    setOptimizationProfile(OptimizationProfile::LOW_LATENCY);
}

void PerformanceConfig::optimizeForMemoryUsage() {
    setOptimizationProfile(OptimizationProfile::LOW_MEMORY);
}

void PerformanceConfig::optimizeForCPUUsage() {
    setOptimizationProfile(OptimizationProfile::LOW_CPU);
}

void PerformanceConfig::setConfig(const std::string& key, const std::string& value) {
    config_[key] = value;
    logConfigChange(key, value);
}

std::string PerformanceConfig::getConfig(const std::string& key) {
    auto it = config_.find(key);
    return (it != config_.end()) ? it->second : "";
}

void PerformanceConfig::loadConfigFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key=value pairs
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            config_[key] = value;
        }
    }
    
    file.close();
    validateConfig();
}

void PerformanceConfig::saveConfigToFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to create config file: " << filename << std::endl;
        return;
    }
    
    file << "# Performance Configuration File" << std::endl;
    file << "# Generated automatically" << std::endl;
    file << std::endl;
    
    for (const auto& pair : config_) {
        file << pair.first << "=" << pair.second << std::endl;
    }
    
    file.close();
}

void PerformanceConfig::enablePerformanceMonitoring(bool enable) {
    performance_monitoring_enabled_ = enable;
}

bool PerformanceConfig::isPerformanceMonitoringEnabled() {
    return performance_monitoring_enabled_;
}

void PerformanceConfig::setPerformanceThreshold(const std::string& metric, double threshold) {
    performance_thresholds_[metric] = threshold;
}

double PerformanceConfig::getPerformanceThreshold(const std::string& metric) {
    auto it = performance_thresholds_.find(metric);
    return (it != performance_thresholds_.end()) ? it->second : 0.0;
}

void PerformanceConfig::setResourceLimit(const std::string& resource, size_t limit) {
    resource_limits_[resource] = limit;
}

size_t PerformanceConfig::getResourceLimit(const std::string& resource) {
    auto it = resource_limits_.find(resource);
    return (it != resource_limits_.end()) ? it->second : 0;
}

void PerformanceConfig::setOptimizationProfile(OptimizationProfile profile) {
    current_profile_ = profile;
    applyProfileSettings(profile);
}

PerformanceConfig::OptimizationProfile PerformanceConfig::getOptimizationProfile() {
    return current_profile_;
}

void PerformanceConfig::applyProfileSettings(OptimizationProfile profile) {
    switch (profile) {
        case OptimizationProfile::BALANCED:
            setConfig("max_connections", "200");
            setConfig("max_active_connections", "1000");
            setConfig("connection_timeout_ms", "30000");
            setConfig("metadata_timeout_ms", "120000");
            setConfig("max_worker_threads", "8");
            setConfig("cache_size", "10000");
            setConfig("db_connection_pool_size", "10");
            break;
            
        case OptimizationProfile::HIGH_THROUGHPUT:
            setConfig("max_connections", "500");
            setConfig("max_active_connections", "2000");
            setConfig("connection_timeout_ms", "60000");
            setConfig("metadata_timeout_ms", "300000");
            setConfig("max_worker_threads", "16");
            setConfig("cache_size", "50000");
            setConfig("db_connection_pool_size", "20");
            setConfig("db_batch_size", "5000");
            break;
            
        case OptimizationProfile::LOW_LATENCY:
            setConfig("max_connections", "100");
            setConfig("max_active_connections", "500");
            setConfig("connection_timeout_ms", "10000");
            setConfig("metadata_timeout_ms", "30000");
            setConfig("max_worker_threads", "4");
            setConfig("cache_size", "5000");
            setConfig("db_connection_pool_size", "5");
            setConfig("db_batch_size", "100");
            break;
            
        case OptimizationProfile::LOW_MEMORY:
            setConfig("max_connections", "50");
            setConfig("max_active_connections", "200");
            setConfig("connection_timeout_ms", "15000");
            setConfig("metadata_timeout_ms", "60000");
            setConfig("max_worker_threads", "2");
            setConfig("cache_size", "1000");
            setConfig("db_connection_pool_size", "3");
            setConfig("db_batch_size", "50");
            break;
            
        case OptimizationProfile::LOW_CPU:
            setConfig("max_connections", "100");
            setConfig("max_active_connections", "500");
            setConfig("connection_timeout_ms", "45000");
            setConfig("metadata_timeout_ms", "180000");
            setConfig("max_worker_threads", "2");
            setConfig("cache_size", "2000");
            setConfig("db_connection_pool_size", "5");
            setConfig("db_batch_size", "200");
            break;
    }
}

void PerformanceConfig::validateConfig() {
    // Validate numeric values
    auto validateInt = [](const std::string& key, int min_val, int max_val) {
        auto it = config_.find(key);
        if (it != config_.end()) {
            try {
                int value = std::stoi(it->second);
                if (value < min_val || value > max_val) {
                    std::cerr << "Warning: " << key << " value " << value 
                              << " is outside valid range [" << min_val << ", " << max_val << "]" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid value for " << key << ": " << it->second << std::endl;
            }
        }
    };
    
    validateInt("max_connections", 1, 1000);
    validateInt("max_active_connections", 1, 10000);
    validateInt("connection_timeout_ms", 1000, 300000);
    validateInt("metadata_timeout_ms", 5000, 600000);
    validateInt("max_worker_threads", 1, 32);
    validateInt("cache_size", 100, 100000);
    validateInt("db_connection_pool_size", 1, 50);
    validateInt("db_batch_size", 10, 10000);
}

void PerformanceConfig::logConfigChange(const std::string& key, const std::string& value) {
    if (performance_monitoring_enabled_) {
        std::cout << "[PERF_CONFIG] " << key << " = " << value << std::endl;
    }
}

} // namespace dht_crawler
