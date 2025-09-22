#include "timeout_manager.hpp"
#include <algorithm>
#include <chrono>

namespace dht_crawler {

TimeoutManager::TimeoutManager(const TimeoutConfig& config) : config_(config), should_stop_(false) {
    timeout_monitor_thread_ = std::thread(&TimeoutManager::timeoutMonitorLoop, this);
}

TimeoutManager::~TimeoutManager() {
    stop();
    if (timeout_monitor_thread_.joinable()) {
        timeout_monitor_thread_.join();
    }
}

void TimeoutManager::timeoutMonitorLoop() {
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
        
        // Check for expired timeouts
        checkExpiredTimeouts();
        
        // Clean up expired timeouts
        cleanupExpiredTimeouts();
    }
}

void TimeoutManager::checkExpiredTimeouts() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = active_timeouts_.begin(); it != active_timeouts_.end();) {
        if (now >= it->second.expires_at) {
            // Timeout expired
            if (it->second.callback) {
                it->second.callback(it->first);
            }
            
            // Move to expired timeouts
            expired_timeouts_[it->first] = it->second;
            it = active_timeouts_.erase(it);
        } else {
            ++it;
        }
    }
}

void TimeoutManager::cleanupExpiredTimeouts() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = expired_timeouts_.begin(); it != expired_timeouts_.end();) {
        if (now - it->second.expires_at > std::chrono::milliseconds(config_.cleanup_interval)) {
            it = expired_timeouts_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string TimeoutManager::startTimeout(const std::string& timeout_id, 
                                       std::chrono::milliseconds duration,
                                       TimeoutCallback callback) {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    TimeoutInfo timeout_info;
    timeout_info.id = timeout_id;
    timeout_info.duration = duration;
    timeout_info.expires_at = now + duration;
    timeout_info.callback = callback;
    timeout_info.created_at = now;
    timeout_info.status = TimeoutStatus::ACTIVE;
    
    active_timeouts_[timeout_id] = timeout_info;
    
    // Notify monitor thread
    monitor_condition_.notify_one();
    
    return timeout_id;
}

bool TimeoutManager::stopTimeout(const std::string& timeout_id) {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    auto it = active_timeouts_.find(timeout_id);
    if (it != active_timeouts_.end()) {
        it->second.status = TimeoutStatus::CANCELLED;
        active_timeouts_.erase(it);
        return true;
    }
    
    return false;
}

bool TimeoutManager::checkTimeout(const std::string& timeout_id) {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    auto it = active_timeouts_.find(timeout_id);
    if (it != active_timeouts_.end()) {
        auto now = std::chrono::steady_clock::now();
        return now >= it->second.expires_at;
    }
    
    return false;
}

bool TimeoutManager::resetTimeout(const std::string& timeout_id, std::chrono::milliseconds new_duration) {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    auto it = active_timeouts_.find(timeout_id);
    if (it != active_timeouts_.end()) {
        auto now = std::chrono::steady_clock::now();
        it->second.duration = new_duration;
        it->second.expires_at = now + new_duration;
        return true;
    }
    
    return false;
}

std::chrono::milliseconds TimeoutManager::getRemainingTime(const std::string& timeout_id) {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    auto it = active_timeouts_.find(timeout_id);
    if (it != active_timeouts_.end()) {
        auto now = std::chrono::steady_clock::now();
        auto remaining = it->second.expires_at - now;
        return std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
    }
    
    return std::chrono::milliseconds(0);
}

bool TimeoutManager::isTimedOut(const std::string& timeout_id) {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    auto it = active_timeouts_.find(timeout_id);
    if (it != active_timeouts_.end()) {
        auto now = std::chrono::steady_clock::now();
        return now >= it->second.expires_at;
    }
    
    return false;
}

TimeoutManager::TimeoutInfo TimeoutManager::getTimeoutInfo(const std::string& timeout_id) {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    auto it = active_timeouts_.find(timeout_id);
    if (it != active_timeouts_.end()) {
        return it->second;
    }
    
    // Check expired timeouts
    auto expired_it = expired_timeouts_.find(timeout_id);
    if (expired_it != expired_timeouts_.end()) {
        return expired_it->second;
    }
    
    return TimeoutInfo{}; // Return empty info if not found
}

std::vector<std::string> TimeoutManager::getActiveTimeoutIds() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    std::vector<std::string> ids;
    for (const auto& pair : active_timeouts_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::vector<std::string> TimeoutManager::getExpiredTimeoutIds() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    
    std::vector<std::string> ids;
    for (const auto& pair : expired_timeouts_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

int TimeoutManager::getActiveTimeoutCount() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    return static_cast<int>(active_timeouts_.size());
}

int TimeoutManager::getExpiredTimeoutCount() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    return static_cast<int>(expired_timeouts_.size());
}

int TimeoutManager::getTotalTimeoutCount() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    return static_cast<int>(active_timeouts_.size() + expired_timeouts_.size());
}

void TimeoutManager::clearExpiredTimeouts() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    expired_timeouts_.clear();
}

void TimeoutManager::clearAllTimeouts() {
    std::lock_guard<std::mutex> lock(timeouts_mutex_);
    active_timeouts_.clear();
    expired_timeouts_.clear();
}

void TimeoutManager::updateConfig(const TimeoutConfig& config) {
    config_ = config;
}

TimeoutManager::TimeoutConfig TimeoutManager::getConfig() const {
    return config_;
}

void TimeoutManager::start() {
    should_stop_ = false;
    if (!timeout_monitor_thread_.joinable()) {
        timeout_monitor_thread_ = std::thread(&TimeoutManager::timeoutMonitorLoop, this);
    }
}

void TimeoutManager::stop() {
    should_stop_ = true;
    monitor_condition_.notify_all();
}

bool TimeoutManager::isRunning() const {
    return !should_stop_ && timeout_monitor_thread_.joinable();
}

std::map<std::string, std::string> TimeoutManager::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["active_timeouts"] = std::to_string(getActiveTimeoutCount());
    status["expired_timeouts"] = std::to_string(getExpiredTimeoutCount());
    status["total_timeouts"] = std::to_string(getTotalTimeoutCount());
    status["is_running"] = isRunning() ? "true" : "false";
    
    return status;
}

} // namespace dht_crawler
