/*
 * Enhanced Metadata Transfer Extension for DHT Crawler
 * Simplified version for libtorrent 2.0
 * 
 * Copyright (c) 2006, Arvid Norberg
 * Enhanced for qBittorrent DHT Crawler
 */

#pragma once

#ifndef DISABLE_LIBTORRENT
#include <libtorrent/session.hpp>
#include <libtorrent/torrent.hpp>
#include <libtorrent/peer_connection.hpp>
#include <libtorrent/bt_peer_connection.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/extensions.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/error_code.hpp>
#endif

#include <vector>
#include <utility>
#include <numeric>
#include <memory>
#include <chrono>
#include <atomic>
#include <functional>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <set>

namespace dht_crawler {

// Utility function to convert hash formats
std::string convert_hash_to_hex(const std::string& hash) {
    // If it's already hex (40 characters), return as is
    if (hash.length() == 40) {
        // Check if it's valid hex
        bool is_hex = true;
        for (char c : hash) {
            if (!std::isxdigit(c)) {
                is_hex = false;
                break;
            }
        }
        if (is_hex) {
            return hash;
        }
    }
    
    // If it's base32 (32 characters), convert to hex
    if (hash.length() == 32) {
        // Simple base32 to hex conversion for SHA1 hashes
        // This is a basic implementation - for production use, consider a proper base32 library
        const std::string base32_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        std::string result;
        
        try {
            // Convert base32 to binary, then to hex
            std::string binary;
            for (char c : hash) {
                size_t pos = base32_chars.find(std::toupper(c));
                if (pos == std::string::npos) {
                    return ""; // Invalid base32 character
                }
                
                // Convert to 5-bit binary
                for (int i = 4; i >= 0; i--) {
                    binary += ((pos >> i) & 1) ? '1' : '0';
                }
            }
            
            // Convert binary to hex
            for (size_t i = 0; i < binary.length(); i += 4) {
                std::string nibble = binary.substr(i, 4);
                int value = 0;
                for (int j = 0; j < 4; j++) {
                    if (nibble[j] == '1') {
                        value |= (1 << (3 - j));
                    }
                }
                result += "0123456789abcdef"[value];
            }
            
            return result;
        } catch (...) {
            return "";
        }
    }
    
    // Handle binary hash (20 bytes = 40 hex chars)
    if (hash.length() == 20) {
        std::string result;
        for (unsigned char c : hash) {
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", c);
            result += hex;
        }
        return result;
    }
    
    return ""; // Invalid format
}

// Enhanced metadata manager for better tracking and logging
#ifndef DISABLE_LIBTORRENT
class MetadataManager {
public:
    MetadataManager(std::function<void(const std::string&)> log_callback = nullptr)
        : m_log_callback(log_callback)
        , m_total_requests(0)
        , m_successful_requests(0)
        , m_failed_requests(0)
        , m_timeout_requests(0)
    {
    }

    void log_metadata_request(const std::string& info_hash) {
        m_total_requests++;
        std::ostringstream oss;
        oss << "Metadata request #" << m_total_requests << " for hash: " << info_hash.substr(0, 8) << "...";
        log(oss.str());
    }

    void log_metadata_success(const std::string& info_hash, size_t metadata_size) {
        m_successful_requests++;
        std::ostringstream oss;
        oss << "Metadata success #" << m_successful_requests << " for hash: " << info_hash.substr(0, 8) << "..."
            << ", size: " << metadata_size << " bytes";
        log(oss.str());
    }

    void log_metadata_failure(const std::string& info_hash, const std::string& reason) {
        m_failed_requests++;
        std::ostringstream oss;
        oss << "Metadata failure #" << m_failed_requests << " for hash: " << info_hash.substr(0, 8) << "..."
            << ", reason: " << reason;
        log(oss.str());
    }

    void log_metadata_timeout(const std::string& info_hash) {
        m_timeout_requests++;
        std::ostringstream oss;
        oss << "Metadata timeout #" << m_timeout_requests << " for hash: " << info_hash.substr(0, 8) << "...";
        log(oss.str());
    }

    // Statistics
    int get_total_requests() const { return m_total_requests; }
    int get_successful_requests() const { return m_successful_requests; }
    int get_failed_requests() const { return m_failed_requests; }
    int get_timeout_requests() const { return m_timeout_requests; }
    
    double get_success_rate() const {
        return m_total_requests > 0 ? (double)m_successful_requests / m_total_requests : 0.0;
    }

    void print_statistics() const {
        std::ostringstream oss;
        oss << "\n=== ENHANCED METADATA STATISTICS ===" << std::endl;
        oss << "Total requests: " << m_total_requests << std::endl;
        oss << "Successful requests: " << m_successful_requests << std::endl;
        oss << "Failed requests: " << m_failed_requests << std::endl;
        oss << "Timeout requests: " << m_timeout_requests << std::endl;
        oss << "Success rate: " << std::fixed << std::setprecision(2) 
            << (get_success_rate() * 100.0) << "%" << std::endl;
        oss << "=====================================" << std::endl;
        log(oss.str());
    }

private:
    void log(const std::string& message) const {
        if (m_log_callback) {
            m_log_callback("[MetadataManager] " + message);
        }
    }

    std::function<void(const std::string&)> m_log_callback;
    std::atomic<int> m_total_requests;
    std::atomic<int> m_successful_requests;
    std::atomic<int> m_failed_requests;
    std::atomic<int> m_timeout_requests;
};

// Queue entry for metadata requests
struct QueueEntry {
    std::string hash;
    int priority; // Higher number = higher priority
    std::string source; // DHT_PEERS, DHT_ANNOUNCE, DHT_ITEM, etc.
    std::chrono::steady_clock::time_point queued_time;
    
    QueueEntry(const std::string& h, int p, const std::string& s) 
        : hash(h), priority(p), source(s), queued_time(std::chrono::steady_clock::now()) {}
};

// Active request tracker - keeps track of currently processing requests
class ActiveRequestTracker {
public:
    struct RequestInfo {
        std::chrono::steady_clock::time_point request_time;
        int priority; // Higher number = higher priority
        std::string source; // DHT_PEERS, DHT_ANNOUNCE, DHT_ITEM, etc.
        lt::torrent_handle handle; // Keep the libtorrent handle
    };

    ActiveRequestTracker() = default;

    void add_request(const std::string& hash, const std::chrono::steady_clock::time_point& request_time, 
                    int priority = 1, const std::string& source = "UNKNOWN", lt::torrent_handle handle = lt::torrent_handle()) {
        m_requests[hash] = {request_time, priority, source, handle};
    }

    void remove_request(const std::string& hash) {
        m_requests.erase(hash);
    }

    bool has_request(const std::string& hash) const {
        return m_requests.find(hash) != m_requests.end();
    }

    std::vector<std::string> get_timed_out_requests(int timeout_seconds = 120) const {
        std::vector<std::string> timed_out;
        auto now = std::chrono::steady_clock::now();
        
        for (const auto& pair : m_requests) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - pair.second.request_time).count();
            if (elapsed >= timeout_seconds) {
                timed_out.push_back(pair.first);
            }
        }
        
        return timed_out;
    }

    size_t get_active_requests() const {
        return m_requests.size();
    }

    int get_available_slots(int max_concurrent) const {
        return std::max(0, max_concurrent - static_cast<int>(m_requests.size()));
    }

    void clear() {
        m_requests.clear();
    }

    const std::map<std::string, RequestInfo>& get_requests() const {
        return m_requests;
    }

private:
    std::map<std::string, RequestInfo> m_requests;
};

// Unlimited queue for metadata requests
class MetadataRequestQueue {
public:
    MetadataRequestQueue() = default;

    void enqueue(const std::string& hash, int priority = 1, const std::string& source = "UNKNOWN") {
        // Check if already in queue or processing
        if (m_queue_set.find(hash) != m_queue_set.end()) {
            return; // Already queued
        }
        
        m_queue.push_back(QueueEntry(hash, priority, source));
        m_queue_set.insert(hash);
    }

    bool dequeue(std::string& hash, int& priority, std::string& source) {
        if (m_queue.empty()) {
            return false;
        }
        
        // Find highest priority item (simple implementation - could be optimized)
        auto best_it = m_queue.begin();
        for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
            if (it->priority > best_it->priority) {
                best_it = it;
            }
        }
        
        hash = best_it->hash;
        priority = best_it->priority;
        source = best_it->source;
        
        m_queue_set.erase(hash);
        m_queue.erase(best_it);
        return true;
    }

    bool is_empty() const {
        return m_queue.empty();
    }

    size_t size() const {
        return m_queue.size();
    }

    bool contains(const std::string& hash) const {
        return m_queue_set.find(hash) != m_queue_set.end();
    }

    void clear() {
        m_queue.clear();
        m_queue_set.clear();
    }

    // Get queue statistics
    struct QueueStats {
        size_t total_queued;
        size_t high_priority_count;
        size_t medium_priority_count;
        size_t low_priority_count;
        std::chrono::seconds oldest_queue_time;
    };

    QueueStats get_stats() const {
        QueueStats stats = {};
        stats.total_queued = m_queue.size();
        
        auto now = std::chrono::steady_clock::now();
        auto oldest_time = now;
        
        for (const auto& entry : m_queue) {
            if (entry.priority >= 3) {
                stats.high_priority_count++;
            } else if (entry.priority >= 2) {
                stats.medium_priority_count++;
            } else {
                stats.low_priority_count++;
            }
            
            if (entry.queued_time < oldest_time) {
                oldest_time = entry.queued_time;
            }
        }
        
        stats.oldest_queue_time = std::chrono::duration_cast<std::chrono::seconds>(now - oldest_time);
        return stats;
    }

private:
    std::vector<QueueEntry> m_queue;
    std::set<std::string> m_queue_set; // For fast lookup
};

// Persistent metadata downloader with unlimited queue
class PersistentMetadataDownloader {
public:
    PersistentMetadataDownloader(lt::session* session, 
                                std::function<void(const std::string&)> log_callback = nullptr)
        : m_session(session)
        , m_log_callback(log_callback)
        , m_max_concurrent_requests(50) // Reasonable concurrent limit for libtorrent
        , m_request_timeout_seconds(120) // 2 minutes timeout
        , m_success_count(0)
        , m_failure_count(0)
        , m_timeout_count(0)
        , m_total_queued(0)
        , m_total_processed(0)
    {
    }

    // Always succeeds - adds to unlimited queue
    bool request_metadata(const std::string& info_hash, int priority = 1, const std::string& source = "UNKNOWN") {
        if (!m_session) {
            log("Session not available for metadata request");
            return false;
        }

        // Check if already active or queued
        if (m_active_tracker.has_request(info_hash) || m_queue.contains(info_hash)) {
            log("Metadata request already active/queued for: " + info_hash.substr(0, 8) + "...");
            return true; // Consider this a success since it's already being handled
        }

        // Add to unlimited queue
        m_queue.enqueue(info_hash, priority, source);
        m_total_queued++;
        
        log("Queued metadata request for: " + info_hash.substr(0, 8) + "... (priority: " + std::to_string(priority) + 
            ", source: " + source + ", queue size: " + std::to_string(m_queue.size()) + ")");
        
        // Try to process the queue immediately
        process_queue();
        
        return true; // Always succeeds - hash is now queued
    }

    // Process items from the queue into active requests
    void process_queue() {
        while (!m_queue.is_empty() && m_active_tracker.get_active_requests() < static_cast<size_t>(m_max_concurrent_requests)) {
            std::string hash;
            int priority;
            std::string source;
            
            if (!m_queue.dequeue(hash, priority, source)) {
                break;
            }
            
            if (process_single_request(hash, priority, source)) {
                m_total_processed++;
            } else {
                // If processing failed, we could re-queue with lower priority or skip
                // For now, we'll skip failed requests to avoid infinite loops
                log("Failed to process queued request for: " + hash.substr(0, 8) + "... (skipping)");
            }
        }
    }

    // Process a single metadata request
    bool process_single_request(const std::string& info_hash, int priority, const std::string& source) {
        try {
            // Convert hash format if needed (base32 to hex)
            std::string hex_hash = convert_hash_to_hex(info_hash);
            if (hex_hash.empty()) {
                log("Invalid hash format: " + info_hash.substr(0, 8) + "... (length: " + std::to_string(info_hash.length()) + ")");
                return false;
            }
            
            // Create magnet link
            std::string magnet = "magnet:?xt=urn:btih:" + hex_hash;
            
            // Create torrent parameters
            lt::add_torrent_params params;
            
            // Try to parse magnet URI first
            lt::error_code ec;
            params = lt::parse_magnet_uri(magnet, ec);
            if (ec) {
                log("Failed to parse magnet URI: " + ec.message());
                return false;
            }
            
            params.save_path = ".";  // Current directory
            params.flags |= lt::torrent_flags::auto_managed;
            params.flags |= lt::torrent_flags::duplicate_is_error;
            params.flags |= lt::torrent_flags::upload_mode;  // Don't upload, just download metadata
            
            // Add torrent to session
            auto handle = m_session->add_torrent(params);
            
            if (handle.is_valid()) {
                m_active_tracker.add_request(info_hash, std::chrono::steady_clock::now(), priority, source, handle);
                log("Started processing metadata request for: " + info_hash.substr(0, 8) + "... (priority: " + std::to_string(priority) + 
                    ", source: " + source + ", active: " + std::to_string(m_active_tracker.get_active_requests()) + ")");
                return true;
            } else {
                log("Failed to add torrent for metadata: " + info_hash.substr(0, 8) + "... (handle invalid)");
                return false;
            }
        } catch (const std::exception& e) {
            log("Exception processing metadata request for " + info_hash.substr(0, 8) + "...: " + e.what());
            return false;
        }
    }

    void handle_metadata_received(const std::string& info_hash) {
        m_active_tracker.remove_request(info_hash);
        m_success_count++;
        log("Metadata received and request removed for: " + info_hash.substr(0, 8) + "... (success: " + std::to_string(m_success_count) + ")");
        
        // Process more items from the queue
        process_queue();
    }

    void cleanup_timed_out_requests() {
        auto timed_out = m_active_tracker.get_timed_out_requests(m_request_timeout_seconds);
        for (const auto& hash : timed_out) {
            // Remove the torrent from libtorrent session
            auto it = m_active_tracker.get_requests().find(hash);
            if (it != m_active_tracker.get_requests().end() && it->second.handle.is_valid()) {
                m_session->remove_torrent(it->second.handle);
            }
            
            m_active_tracker.remove_request(hash);
            m_timeout_count++;
            log("Removed timed out metadata request for: " + hash.substr(0, 8) + "... (timeouts: " + std::to_string(m_timeout_count) + ")");
        }
        
        // Process more items from the queue after cleanup
        process_queue();
    }

    void log_success() {
        m_success_count++;
    }

    void log_failure() {
        m_failure_count++;
    }

    int get_available_slots() const {
        return m_active_tracker.get_available_slots(m_max_concurrent_requests);
    }

    bool can_add_request() const {
        return true; // Always true now - we have unlimited queue
    }

    std::vector<std::string> get_timed_out_requests() const {
        return m_active_tracker.get_timed_out_requests(m_request_timeout_seconds);
    }

    void set_max_concurrent_requests(int max) {
        m_max_concurrent_requests = max;
    }

    void set_request_timeout(int timeout_seconds) {
        m_request_timeout_seconds = timeout_seconds;
    }

    size_t get_active_requests() const {
        return m_active_tracker.get_active_requests();
    }

    size_t get_queue_size() const {
        return m_queue.size();
    }

    size_t get_total_queued() const {
        return m_total_queued;
    }

    size_t get_total_processed() const {
        return m_total_processed;
    }

    // Force process queue (useful for periodic processing)
    void force_process_queue() {
        process_queue();
    }

    void print_status() const {
        auto queue_stats = m_queue.get_stats();
        log("Metadata Downloader Status:");
        log("  Active requests: " + std::to_string(m_active_tracker.get_active_requests()));
        log("  Max concurrent: " + std::to_string(m_max_concurrent_requests));
        log("  Available slots: " + std::to_string(get_available_slots()));
        log("  Queue size: " + std::to_string(queue_stats.total_queued));
        log("  Queue breakdown - High: " + std::to_string(queue_stats.high_priority_count) + 
            ", Medium: " + std::to_string(queue_stats.medium_priority_count) + 
            ", Low: " + std::to_string(queue_stats.low_priority_count));
        log("  Oldest queued: " + std::to_string(queue_stats.oldest_queue_time.count()) + "s ago");
        log("  Total queued: " + std::to_string(m_total_queued));
        log("  Total processed: " + std::to_string(m_total_processed));
        log("  Success count: " + std::to_string(m_success_count));
        log("  Failure count: " + std::to_string(m_failure_count));
        log("  Timeout count: " + std::to_string(m_timeout_count));
    }

private:
    void log(const std::string& message) const {
        if (m_log_callback) {
            m_log_callback("[MetadataDownloader] " + message);
        }
    }

    lt::session* m_session;
    std::function<void(const std::string&)> m_log_callback;
    ActiveRequestTracker m_active_tracker;
    MetadataRequestQueue m_queue;
    int m_max_concurrent_requests;
    int m_request_timeout_seconds;
    int m_success_count;
    int m_failure_count;
    int m_timeout_count;
    int m_total_queued;
    int m_total_processed;
};

#endif // DISABLE_LIBTORRENT

} // namespace dht_crawler
