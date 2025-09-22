#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <memory>
#include <chrono>

#ifndef DISABLE_LIBTORRENT
#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>

class MetadataWorkerPool {
public:
    struct MetadataRequest {
        std::string info_hash;
        int priority;
        std::string source;
        std::chrono::steady_clock::time_point queued_time;
        
        MetadataRequest(const std::string& hash, int prio = 1, const std::string& src = "UNKNOWN")
            : info_hash(hash), priority(prio), source(src), queued_time(std::chrono::steady_clock::now()) {}
    };

    struct WorkerStats {
        std::atomic<int> requests_processed{0};
        std::atomic<int> requests_successful{0};
        std::atomic<int> requests_failed{0};
        std::atomic<int> requests_timeout{0};
        std::chrono::steady_clock::time_point start_time;
        
        WorkerStats() : start_time(std::chrono::steady_clock::now()) {}
    };

private:
    // Worker pool configuration
    int m_num_workers;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_shutdown{false};
    
    // Thread-safe request queue
    std::queue<MetadataRequest> m_request_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Track pending metadata requests
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_pending_requests;
    mutable std::mutex m_pending_mutex;
    
    // Statistics
    std::vector<std::unique_ptr<WorkerStats>> m_worker_stats;
    std::atomic<int> m_total_queued{0};
    std::atomic<int> m_total_processed{0};
    
    // Dependencies
    lt::session* m_session;
    std::function<void(const std::string&)> m_log_callback;
    
    // Configuration
    int m_request_timeout_seconds;

public:
    MetadataWorkerPool(lt::session* session, 
                      int num_workers = 10,
                      int request_timeout = 20,
                      std::function<void(const std::string&)> log_callback = nullptr)
        : m_num_workers(num_workers)
        , m_session(session)
        , m_log_callback(log_callback)
        , m_request_timeout_seconds(request_timeout)
    {
        // Initialize worker stats
        m_worker_stats.resize(m_num_workers);
        for (int i = 0; i < m_num_workers; ++i) {
            m_worker_stats[i] = std::make_unique<WorkerStats>();
        }
        
        // Start worker threads
        start_workers();
        
        log("Started metadata worker pool with " + std::to_string(m_num_workers) + " workers");
    }
    
    ~MetadataWorkerPool() {
        shutdown();
    }
    
    // Add a metadata request to the queue
    bool queue_request(const std::string& info_hash, int priority = 1, const std::string& source = "UNKNOWN") {
        if (m_shutdown) {
            return false;
        }
        
        // Check if already active
        {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            if (m_pending_requests.find(info_hash) != m_pending_requests.end()) {
                log("Request already active for: " + info_hash.substr(0, 8) + "...");
                return true;
            }
        }
        
        // Add to queue
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_request_queue.emplace(info_hash, priority, source);
            m_total_queued++;
        }
        
        m_queue_cv.notify_one();
        
        log("Queued metadata request for: " + info_hash.substr(0, 8) + "... (priority: " + 
            std::to_string(priority) + ", source: " + source + ", queue size: " + 
            std::to_string(get_queue_size()) + ")");
        
        return true;
    }
    
    // Get current queue size
    size_t get_queue_size() const {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        return m_request_queue.size();
    }
    
    // Get number of active requests
    size_t get_active_requests() const {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        return m_pending_requests.size();
    }
    
    // Get total statistics
    void get_stats(int& total_queued, int& total_processed, int& total_successful, 
                  int& total_failed, int& total_timeout) const {
        total_queued = m_total_queued.load();
        total_processed = m_total_processed.load();
        
        total_successful = 0;
        total_failed = 0;
        total_timeout = 0;
        
        for (const auto& stats : m_worker_stats) {
            total_successful += stats->requests_successful.load();
            total_failed += stats->requests_failed.load();
            total_timeout += stats->requests_timeout.load();
        }
    }
    
    // Handle metadata received alert from main alert loop
    void handle_metadata_received(const std::string& info_hash) {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        auto it = m_pending_requests.find(info_hash);
        if (it != m_pending_requests.end()) {
            m_pending_requests.erase(it);
            log("Metadata received for: " + info_hash.substr(0, 8) + "...");
            
            // Update statistics
            for (auto& stats : m_worker_stats) {
                stats->requests_successful++;
            }
        }
    }
    
    // Get pending requests for timeout cleanup
    std::vector<std::string> get_timed_out_requests() {
        std::vector<std::string> timed_out;
        auto now = std::chrono::steady_clock::now();
        
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        for (auto it = m_pending_requests.begin(); it != m_pending_requests.end();) {
            if (now - it->second > std::chrono::seconds(m_request_timeout_seconds)) {
                timed_out.push_back(it->first);
                it = m_pending_requests.erase(it);
            } else {
                ++it;
            }
        }
        
        return timed_out;
    }
    
    // Clean up timed out requests
    void cleanup_timeouts() {
        auto timed_out = get_timed_out_requests();
        for (const auto& hash : timed_out) {
            log("Cleaned up timed out request for: " + hash.substr(0, 8) + "...");
            
            // Update statistics
            for (auto& stats : m_worker_stats) {
                stats->requests_timeout++;
            }
        }
    }
    
    // Shutdown the worker pool
    void shutdown() {
        if (m_shutdown) return;
        
        m_shutdown = true;
        m_queue_cv.notify_all();
        
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        // Clean up remaining pending requests
        {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            m_pending_requests.clear();
        }
        
        log("Metadata worker pool shutdown complete");
    }

private:
    void start_workers() {
        for (int i = 0; i < m_num_workers; ++i) {
            m_workers.emplace_back(&MetadataWorkerPool::worker_thread, this, i);
        }
    }
    
    void worker_thread(int worker_id) {
        log("Worker " + std::to_string(worker_id) + " started");
        
        auto& stats = *m_worker_stats[worker_id];
        
        while (!m_shutdown) {
            MetadataRequest request("", 0, "");
            bool got_request = false;
            
            // Wait for a request
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_queue_cv.wait(lock, [this] { return !m_request_queue.empty() || m_shutdown; });
                
                if (!m_shutdown && !m_request_queue.empty()) {
                    request = m_request_queue.front();
                    m_request_queue.pop();
                    got_request = true;
                }
            }
            
            if (!got_request) {
                continue;
            }
            
            // Process the request by adding it to libtorrent session
            process_metadata_request(worker_id, request, stats);
        }
        
        log("Worker " + std::to_string(worker_id) + " stopped");
    }
    
    void process_metadata_request(int worker_id, const MetadataRequest& request, WorkerStats& stats) {
        try {
            // Convert hash format if needed
            std::string hex_hash = convert_hash_to_hex(request.info_hash);
            if (hex_hash.empty()) {
                log("Worker " + std::to_string(worker_id) + ": Invalid hash format: " + 
                    request.info_hash.substr(0, 8) + "...");
                stats.requests_failed++;
                return;
            }
            
            // Create magnet link
            std::string magnet = "magnet:?xt=urn:btih:" + hex_hash;
            
            // Create torrent parameters
            lt::add_torrent_params params;
            lt::error_code ec;
            params = lt::parse_magnet_uri(magnet, ec);
            if (ec) {
                log("Worker " + std::to_string(worker_id) + ": Failed to parse magnet URI: " + ec.message());
                stats.requests_failed++;
                return;
            }
            
            params.save_path = ".";
            params.flags |= lt::torrent_flags::auto_managed;
            params.flags |= lt::torrent_flags::duplicate_is_error;
            params.flags |= lt::torrent_flags::seed_mode;  // Allow metadata download, no data upload
            
            // Add torrent to session
            auto handle = m_session->add_torrent(params);
            
            if (handle.is_valid()) {
                // Track this request as pending
                {
                    std::lock_guard<std::mutex> lock(m_pending_mutex);
                    m_pending_requests[request.info_hash] = std::chrono::steady_clock::now();
                }
                
                stats.requests_processed++;
                m_total_processed++;
                
                log("Worker " + std::to_string(worker_id) + ": Queued metadata request for " + 
                    request.info_hash.substr(0, 8) + "... (priority: " + std::to_string(request.priority) + 
                    ", source: " + request.source + ")");
                
                // Don't wait here - let the main alert loop handle metadata reception
                // The handle will be cleaned up by the main alert loop when metadata is received
                
            } else {
                log("Worker " + std::to_string(worker_id) + ": Failed to add torrent for " + 
                    request.info_hash.substr(0, 8) + "...");
                stats.requests_failed++;
            }
            
        } catch (const std::exception& e) {
            log("Worker " + std::to_string(worker_id) + ": Exception processing " + 
                request.info_hash.substr(0, 8) + "...: " + e.what());
            stats.requests_failed++;
        }
    }
    
    std::string convert_hash_to_hex(const std::string& hash) {
        if (hash.length() == 40) {
            // Already hex format
            return hash;
        } else if (hash.length() == 32) {
            // Base32 format - convert to hex
            // This is a simplified conversion - in practice you'd use a proper base32 decoder
            return hash; // For now, assume it's already hex
        }
        return "";
    }
    
    void log(const std::string& message) {
        if (m_log_callback) {
            m_log_callback(message);
        }
    }
};

#else // DISABLE_LIBTORRENT

// Stub class for when libtorrent is disabled
class MetadataWorkerPool {
public:
    struct MetadataRequest {
        std::string info_hash;
        int priority;
        std::string source;
        std::chrono::steady_clock::time_point queued_time;
        
        MetadataRequest(const std::string& hash, int prio = 1, const std::string& src = "UNKNOWN")
            : info_hash(hash), priority(prio), source(src), queued_time(std::chrono::steady_clock::now()) {}
    };

    struct WorkerStats {
        std::atomic<int> requests_processed{0};
        std::atomic<int> requests_successful{0};
        std::atomic<int> requests_failed{0};
        std::atomic<int> requests_timeout{0};
        std::chrono::steady_clock::time_point start_time;
        
        WorkerStats() : start_time(std::chrono::steady_clock::now()) {}
    };

    MetadataWorkerPool(void* session = nullptr, int num_workers = 10, int request_timeout = 20, std::function<void(const std::string&)> log_callback = nullptr) {
        (void)session; // Suppress unused parameter warning
        (void)num_workers; // Suppress unused parameter warning
        (void)request_timeout; // Suppress unused parameter warning
        if (log_callback) {
            log_callback("MetadataWorkerPool: libtorrent disabled, using stub implementation");
        }
    }
    
    ~MetadataWorkerPool() = default;
    
    bool queue_request(const std::string& info_hash, int priority = 1, const std::string& source = "UNKNOWN") {
        (void)info_hash; // Suppress unused parameter warning
        (void)priority; // Suppress unused parameter warning
        (void)source; // Suppress unused parameter warning
        return false; // Always fail when libtorrent is disabled
    }
    
    void shutdown() {}
    
    std::vector<WorkerStats> get_worker_stats() const {
        return {};
    }
    
    int get_total_queued() const { return 0; }
    int get_total_processed() const { return 0; }
    int get_active_requests() const { return 0; }
    int get_queue_size() const { return 0; }
    
    void cleanup_timeouts() {}
    
    std::string convert_hash_to_hex(const std::string& hash) {
        return hash;
    }
    
    void log(const std::string& message) {
        (void)message; // Suppress unused parameter warning
    }
};

#endif // DISABLE_LIBTORRENT
