/*
 * Improved DHT Crawling Strategy Based on mldht's "Good Citizen" Approach
 * 
 * Key insights from the8472/mldht:
 * 1. Rate limiting is crucial to avoid getting blocked
 * 2. Focus on incoming traffic observation rather than aggressive querying
 * 3. Respect DHT node resources and bandwidth
 * 4. Use passive observation techniques
 */

#pragma once

#ifndef DISABLE_LIBTORRENT
#include <libtorrent/session.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/alert_types.hpp>
#endif

#include <vector>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <functional>
#include <atomic>
#include <queue>
#include <mutex>

#ifndef DISABLE_LIBTORRENT

namespace dht_crawler {

// Rate-limited DHT query manager
class RateLimitedDHTManager {
public:
    RateLimitedDHTManager(lt::session* session, 
                          std::function<void(const std::string&)> log_callback = nullptr)
        : m_session(session)
        , m_log_callback(log_callback)
        , m_queries_per_second(10)  // Conservative rate limit
        , m_burst_limit(50)         // Max queries in burst
        , m_burst_window_ms(5000)   // 5 second window
        , m_total_queries(0)
        , m_blocked_queries(0)
    {
    }

    // Send DHT query with rate limiting
    bool send_query(const lt::sha1_hash& target, const std::string& query_type) {
        auto now = std::chrono::steady_clock::now();
        
        // Clean old timestamps
        while (!m_query_timestamps.empty() && 
               now - m_query_timestamps.front() > std::chrono::seconds(1)) {
            m_query_timestamps.pop();
        }
        
        // Check rate limit
        if (m_query_timestamps.size() >= static_cast<size_t>(m_queries_per_second)) {
            m_blocked_queries++;
            log("Rate limit exceeded, blocking query");
            return false;
        }
        
        // Check burst limit
        auto burst_start = now - std::chrono::milliseconds(m_burst_window_ms);
        int burst_count = 0;
        
        // Create a copy of the queue to iterate over
        auto temp_queue = m_query_timestamps;
        while (!temp_queue.empty()) {
            auto timestamp = temp_queue.front();
            temp_queue.pop();
            if (timestamp > burst_start) {
                burst_count++;
            }
        }
        
        if (burst_count >= m_burst_limit) {
            m_blocked_queries++;
            log("Burst limit exceeded, blocking query");
            return false;
        }
        
        // Send query
        try {
            if (query_type == "get_peers") {
                m_session->dht_get_peers(target);
            } else if (query_type == "get_item") {
                m_session->dht_get_item(target);
            }
            
            m_query_timestamps.push(now);
            m_total_queries++;
            return true;
            
        } catch (const std::exception& e) {
            log("Error sending DHT query: " + std::string(e.what()));
            return false;
        }
    }

    void set_rate_limit(int queries_per_second) {
        m_queries_per_second = queries_per_second;
        log("Rate limit set to " + std::to_string(queries_per_second) + " queries/second");
    }

    void set_burst_limit(int burst_limit, int window_ms) {
        m_burst_limit = burst_limit;
        m_burst_window_ms = window_ms;
        log("Burst limit set to " + std::to_string(burst_limit) + " queries per " + 
            std::to_string(window_ms) + "ms");
    }

    // Statistics
    int get_total_queries() const { return m_total_queries; }
    int get_blocked_queries() const { return m_blocked_queries; }
    double get_block_rate() const {
        return m_total_queries > 0 ? (double)m_blocked_queries / m_total_queries : 0.0;
    }

    void print_statistics() const {
        log("=== RATE LIMITING STATISTICS ===");
        log("Total queries sent: " + std::to_string(m_total_queries));
        log("Blocked queries: " + std::to_string(m_blocked_queries));
        log("Block rate: " + std::to_string(get_block_rate() * 100) + "%");
        log("Current rate limit: " + std::to_string(m_queries_per_second) + " queries/sec");
        log("Burst limit: " + std::to_string(m_burst_limit) + " queries per " + 
            std::to_string(m_burst_window_ms) + "ms");
        log("================================");
    }

private:
    void log(const std::string& message) const {
        if (m_log_callback) {
            m_log_callback("[RateLimiter] " + message);
        }
    }

    lt::session* m_session;
    std::function<void(const std::string&)> m_log_callback;
    
    std::queue<std::chrono::steady_clock::time_point> m_query_timestamps;
    std::mutex m_timestamps_mutex;
    
    int m_queries_per_second;
    int m_burst_limit;
    int m_burst_window_ms;
    
    std::atomic<int> m_total_queries;
    std::atomic<int> m_blocked_queries;
};

// Passive observation manager - learns from incoming DHT traffic
class PassiveObservationManager {
public:
    struct ObservedInfo {
        std::string infohash;
        std::string source;  // "incoming_query", "peer_reply", "announce"
        std::chrono::steady_clock::time_point observed_time;
        int peer_count;
        std::vector<std::string> peer_addresses;
    };

    PassiveObservationManager(std::function<void(const std::string&)> log_callback = nullptr)
        : m_log_callback(log_callback)
        , m_total_observations(0)
        , m_unique_infohashes(0)
    {
    }

    // Record observed infohash from incoming DHT traffic
    void record_observation(const std::string& infohash, const std::string& source, 
                          int peer_count = 0, const std::vector<std::string>& peers = {}) {
        std::lock_guard<std::mutex> lock(m_observations_mutex);
        
        ObservedInfo info;
        info.infohash = infohash;
        info.source = source;
        info.observed_time = std::chrono::steady_clock::now();
        info.peer_count = peer_count;
        info.peer_addresses = peers;
        
        m_observations.push_back(info);
        m_total_observations++;
        
        // Track unique infohashes
        if (m_unique_infohashes_set.find(infohash) == m_unique_infohashes_set.end()) {
            m_unique_infohashes_set.insert(infohash);
            m_unique_infohashes++;
        }
        
        // Keep only recent observations (last 1000)
        if (m_observations.size() > 1000) {
            m_observations.erase(m_observations.begin());
        }
        
        log("Observed infohash: " + infohash.substr(0, 8) + "... from " + source + 
            " with " + std::to_string(peer_count) + " peers");
    }

    // Get high-value infohashes (those with many peers)
    std::vector<std::string> get_high_value_infohashes(int min_peers = 2) const {
        std::lock_guard<std::mutex> lock(m_observations_mutex);
        
        std::vector<std::string> result;
        for (const auto& obs : m_observations) {
            if (obs.peer_count >= min_peers) {
                result.push_back(obs.infohash);
            }
        }
        return result;
    }

    // Get recently observed infohashes
    std::vector<std::string> get_recent_infohashes(int minutes = 5) const {
        std::lock_guard<std::mutex> lock(m_observations_mutex);
        
        auto cutoff = std::chrono::steady_clock::now() - std::chrono::minutes(minutes);
        std::vector<std::string> result;
        
        for (const auto& obs : m_observations) {
            if (obs.observed_time > cutoff) {
                result.push_back(obs.infohash);
            }
        }
        return result;
    }

    // Statistics
    int get_total_observations() const { return m_total_observations; }
    int get_unique_infohashes() const { return m_unique_infohashes; }

    void print_statistics() const {
        log("=== PASSIVE OBSERVATION STATISTICS ===");
        log("Total observations: " + std::to_string(m_total_observations));
        log("Unique infohashes: " + std::to_string(m_unique_infohashes));
        
        // Count by source
        std::map<std::string, int> source_counts;
        {
            std::lock_guard<std::mutex> lock(m_observations_mutex);
            for (const auto& obs : m_observations) {
                source_counts[obs.source]++;
            }
        }
        
        log("Observations by source:");
        for (const auto& pair : source_counts) {
            log("  " + pair.first + ": " + std::to_string(pair.second));
        }
        log("=====================================");
    }

private:
    void log(const std::string& message) const {
        if (m_log_callback) {
            m_log_callback("[PassiveObs] " + message);
        }
    }

    std::function<void(const std::string&)> m_log_callback;
    
    mutable std::mutex m_observations_mutex;
    std::vector<ObservedInfo> m_observations;
    std::set<std::string> m_unique_infohashes_set;
    
    std::atomic<int> m_total_observations;
    std::atomic<int> m_unique_infohashes;
};

// Smart DHT crawler that learns from observations
class SmartDHTCrawler {
public:
    SmartDHTCrawler(lt::session* session, 
                   std::function<void(const std::string&)> log_callback = nullptr)
        : m_log_callback(log_callback)
        , m_rate_limiter(session, log_callback)
        , m_passive_observer(log_callback)
        , m_adaptive_rate(true)
        , m_base_rate(5)  // Start very conservatively
        , m_max_rate(20)  // Maximum rate
        , m_success_threshold(0.1)  // 10% success rate threshold
    {
    }

    // Adaptive rate adjustment based on success
    void adjust_rate_based_on_success(double success_rate) {
        if (!m_adaptive_rate) return;
        
        if (success_rate > m_success_threshold) {
            // Success rate is good, can increase rate slightly
            int current_rate = m_rate_limiter.get_total_queries();
            if (current_rate < m_max_rate) {
                m_rate_limiter.set_rate_limit(current_rate + 1);
                log("Success rate " + std::to_string(success_rate * 100) + 
                    "%, increasing rate to " + std::to_string(current_rate + 1));
            }
        } else if (success_rate < m_success_threshold / 2) {
            // Success rate is poor, decrease rate
            int current_rate = m_rate_limiter.get_total_queries();
            if (current_rate > m_base_rate) {
                m_rate_limiter.set_rate_limit(current_rate - 1);
                log("Success rate " + std::to_string(success_rate * 100) + 
                    "%, decreasing rate to " + std::to_string(current_rate - 1));
            }
        }
    }

    // Get high-value infohashes for metadata fetching
    std::vector<std::string> get_priority_infohashes() const {
        auto high_value = m_passive_observer.get_high_value_infohashes(3);
        auto recent = m_passive_observer.get_recent_infohashes(10);
        
        // Combine and deduplicate
        std::set<std::string> combined;
        for (const auto& hash : high_value) {
            combined.insert(hash);
        }
        for (const auto& hash : recent) {
            combined.insert(hash);
        }
        
        return std::vector<std::string>(combined.begin(), combined.end());
    }

    // Send smart queries based on observations
    void send_smart_queries() {
        auto priority_hashes = get_priority_infohashes();
        
        // Query high-priority infohashes first
        for (const auto& hash_str : priority_hashes) {
            if (hash_str.length() == 40) {  // Valid hex hash
                // Convert hex to binary
                lt::sha1_hash target;
                for (int i = 0; i < 20; ++i) {
                    std::string byte_str = hash_str.substr(i * 2, 2);
                    target[i] = static_cast<unsigned char>(std::stoi(byte_str, nullptr, 16));
                }
                
                if (m_rate_limiter.send_query(target, "get_peers")) {
                    log("Sent smart query for priority hash: " + hash_str.substr(0, 8) + "...");
                }
            }
        }
        
        // If no priority hashes, send a few random queries
        if (priority_hashes.empty()) {
            for (int i = 0; i < 3; ++i) {
                lt::sha1_hash random_hash;
                for (int j = 0; j < 20; ++j) {
                    random_hash[j] = static_cast<unsigned char>(rand() % 256);
                }
                
                if (m_rate_limiter.send_query(random_hash, "get_peers")) {
                    log("Sent random query (no priority hashes available)");
                }
            }
        }
    }

    // Record observation from incoming traffic
    void record_incoming_observation(const std::string& infohash, const std::string& source,
                                   int peer_count = 0, const std::vector<std::string>& peers = {}) {
        m_passive_observer.record_observation(infohash, source, peer_count, peers);
    }

    // Get managers for integration
    RateLimitedDHTManager& get_rate_limiter() { return m_rate_limiter; }
    PassiveObservationManager& get_passive_observer() { return m_passive_observer; }

    void print_statistics() const {
        m_rate_limiter.print_statistics();
        m_passive_observer.print_statistics();
    }

private:
    void log(const std::string& message) const {
        if (m_log_callback) {
            m_log_callback("[SmartCrawler] " + message);
        }
    }

    // lt::session* m_session; // Unused in current implementation
    std::function<void(const std::string&)> m_log_callback;
    
    RateLimitedDHTManager m_rate_limiter;
    PassiveObservationManager m_passive_observer;
    
    bool m_adaptive_rate;
    int m_base_rate;
    int m_max_rate;
    double m_success_threshold;
};

} // namespace dht_crawler

#endif // DISABLE_LIBTORRENT
