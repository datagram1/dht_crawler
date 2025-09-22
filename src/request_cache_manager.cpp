#include "request_cache_manager.hpp"
#include <algorithm>
#include <chrono>

namespace dht_crawler {

RequestCacheManager::RequestCacheManager(const CacheConfig& config) : config_(config), should_stop_(false) {
    cache_monitor_thread_ = std::thread(&RequestCacheManager::cacheMonitorLoop, this);
}

RequestCacheManager::~RequestCacheManager() {
    stop();
    if (cache_monitor_thread_.joinable()) {
        cache_monitor_thread_.join();
    }
}

void RequestCacheManager::cacheMonitorLoop() {
    while (!should_stop_) {
        std::unique_lock<std::mutex> lock(monitor_mutex_);
        
        // Wait for timeout or stop signal
        cache_monitor_condition_.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return should_stop_;
        });
        
        if (should_stop_) {
            break;
        }
        
        lock.unlock();
        
        // Check for expired requests
        checkExpiredRequests();
        
        // Clean up expired requests
        cleanupExpiredRequests();
    }
}

void RequestCacheManager::checkExpiredRequests() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = active_requests_.begin(); it != active_requests_.end();) {
        if (now >= it->second.expires_at) {
            // Request expired
            it->second.status = RequestStatus::EXPIRED;
            
            // Move to expired requests
            expired_requests_[it->first] = it->second;
            it = active_requests_.erase(it);
        } else {
            ++it;
        }
    }
}

void RequestCacheManager::cleanupExpiredRequests() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = expired_requests_.begin(); it != expired_requests_.end();) {
        if (now - it->second.expires_at > std::chrono::milliseconds(config_.cleanup_interval)) {
            it = expired_requests_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string RequestCacheManager::addRequest(const std::string& request_type,
                                          const std::string& target_id,
                                          const std::map<std::string, std::string>& parameters) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    std::string request_id = generateRequestId();
    
    RequestInfo request_info;
    request_info.id = request_id;
    request_info.type = request_type;
    request_info.target_id = target_id;
    request_info.parameters = parameters;
    request_info.status = RequestStatus::PENDING;
    request_info.created_at = std::chrono::steady_clock::now();
    request_info.expires_at = request_info.created_at + std::chrono::milliseconds(config_.request_timeout);
    request_info.retry_count = 0;
    request_info.max_retries = config_.max_retries;
    
    active_requests_[request_id] = request_info;
    
    // Notify monitor thread
    cache_monitor_condition_.notify_one();
    
    return request_id;
}

RequestCacheManager::RequestStatus RequestCacheManager::getRequestStatus(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        return it->second.status;
    }
    
    // Check expired requests
    auto expired_it = expired_requests_.find(request_id);
    if (expired_it != expired_requests_.end()) {
        return expired_it->second.status;
    }
    
    return RequestStatus::NOT_FOUND;
}

bool RequestCacheManager::markRequestComplete(const std::string& request_id, const std::string& result_data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        it->second.status = RequestStatus::COMPLETED;
        it->second.result_data = result_data;
        it->second.completed_at = std::chrono::steady_clock::now();
        
        // Move to completed requests
        completed_requests_[request_id] = it->second;
        active_requests_.erase(it);
        
        return true;
    }
    
    return false;
}

bool RequestCacheManager::markRequestFailed(const std::string& request_id, const std::string& error_message) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        it->second.status = RequestStatus::FAILED;
        it->second.error_message = error_message;
        it->second.failed_at = std::chrono::steady_clock::now();
        it->second.retry_count++;
        
        // Check if we should retry
        if (it->second.retry_count < it->second.max_retries) {
            // Reset status to pending for retry
            it->second.status = RequestStatus::PENDING;
            it->second.expires_at = std::chrono::steady_clock::now() + 
                                   std::chrono::milliseconds(config_.request_timeout);
        } else {
            // Move to failed requests
            failed_requests_[request_id] = it->second;
            active_requests_.erase(it);
        }
        
        return true;
    }
    
    return false;
}

bool RequestCacheManager::isRequestCached(const std::string& request_type, const std::string& target_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check active requests
    for (const auto& pair : active_requests_) {
        if (pair.second.type == request_type && pair.second.target_id == target_id) {
            return true;
        }
    }
    
    // Check completed requests
    for (const auto& pair : completed_requests_) {
        if (pair.second.type == request_type && pair.second.target_id == target_id) {
            return true;
        }
    }
    
    return false;
}

RequestCacheManager::RequestInfo RequestCacheManager::getRequestInfo(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = active_requests_.find(request_id);
    if (it != active_requests_.end()) {
        return it->second;
    }
    
    // Check completed requests
    auto completed_it = completed_requests_.find(request_id);
    if (completed_it != completed_requests_.end()) {
        return completed_it->second;
    }
    
    // Check failed requests
    auto failed_it = failed_requests_.find(request_id);
    if (failed_it != failed_requests_.end()) {
        return failed_it->second;
    }
    
    // Check expired requests
    auto expired_it = expired_requests_.find(request_id);
    if (expired_it != expired_requests_.end()) {
        return expired_it->second;
    }
    
    return RequestInfo{}; // Return empty info if not found
}

std::vector<std::string> RequestCacheManager::getActiveRequestIds() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    std::vector<std::string> ids;
    for (const auto& pair : active_requests_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::vector<std::string> RequestCacheManager::getCompletedRequestIds() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    std::vector<std::string> ids;
    for (const auto& pair : completed_requests_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::vector<std::string> RequestCacheManager::getFailedRequestIds() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    std::vector<std::string> ids;
    for (const auto& pair : failed_requests_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::vector<std::string> RequestCacheManager::getExpiredRequestIds() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    std::vector<std::string> ids;
    for (const auto& pair : expired_requests_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

int RequestCacheManager::getActiveRequestCount() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return static_cast<int>(active_requests_.size());
}

int RequestCacheManager::getCompletedRequestCount() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return static_cast<int>(completed_requests_.size());
}

int RequestCacheManager::getFailedRequestCount() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return static_cast<int>(failed_requests_.size());
}

int RequestCacheManager::getExpiredRequestCount() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return static_cast<int>(expired_requests_.size());
}

int RequestCacheManager::getTotalRequestCount() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return static_cast<int>(active_requests_.size() + completed_requests_.size() + 
                           failed_requests_.size() + expired_requests_.size());
}

std::string RequestCacheManager::generateRequestId() {
    static std::atomic<int> counter(0);
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "REQ_" << timestamp << "_" << counter.fetch_add(1);
    return ss.str();
}

void RequestCacheManager::clearExpiredRequests() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    expired_requests_.clear();
}

void RequestCacheManager::clearAllRequests() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    active_requests_.clear();
    completed_requests_.clear();
    failed_requests_.clear();
    expired_requests_.clear();
}

void RequestCacheManager::updateConfig(const CacheConfig& config) {
    config_ = config;
}

RequestCacheManager::CacheConfig RequestCacheManager::getConfig() const {
    return config_;
}

void RequestCacheManager::start() {
    should_stop_ = false;
    if (!cache_monitor_thread_.joinable()) {
        cache_monitor_thread_ = std::thread(&RequestCacheManager::cacheMonitorLoop, this);
    }
}

void RequestCacheManager::stop() {
    should_stop_ = true;
    cache_monitor_condition_.notify_all();
}

bool RequestCacheManager::isRunning() const {
    return !should_stop_ && cache_monitor_thread_.joinable();
}

std::map<std::string, std::string> RequestCacheManager::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["active_requests"] = std::to_string(getActiveRequestCount());
    status["completed_requests"] = std::to_string(getCompletedRequestCount());
    status["failed_requests"] = std::to_string(getFailedRequestCount());
    status["expired_requests"] = std::to_string(getExpiredRequestCount());
    status["total_requests"] = std::to_string(getTotalRequestCount());
    status["is_running"] = isRunning() ? "true" : "false";
    
    return status;
}

} // namespace dht_crawler
