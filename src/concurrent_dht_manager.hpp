/*
 * Concurrent DHT Manager for Parallel Torrent Discovery
 * 
 * This class implements a worker pool pattern to parallelize DHT query generation
 * and sending, while maintaining thread-safe alert processing.
 */

#pragma once

#ifndef DISABLE_LIBTORRENT
#include <libtorrent/session.hpp>
#include <libtorrent/sha1_hash.hpp>
#endif

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <random>
#include <chrono>
#include <memory>
#include <functional>

#ifndef DISABLE_LIBTORRENT

struct DHTQuery {
    std::string hash_str;
    lt::sha1_hash random_hash;
    std::chrono::steady_clock::time_point queued_time;
    
    DHTQuery(const std::string& hash, const lt::sha1_hash& lt_hash) 
        : hash_str(hash), random_hash(lt_hash), queued_time(std::chrono::steady_clock::now()) {}
};

class ConcurrentDHTManager {
private:
    // Worker threads
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running;
    std::atomic<bool> m_shutdown_requested;
    
    // Query generation and processing
    std::queue<DHTQuery> m_query_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Thread-safe random number generation
    std::vector<std::unique_ptr<std::mt19937>> m_generators;
    std::vector<std::unique_ptr<std::uniform_int_distribution<>>> m_distributions;
    
    // Statistics (thread-safe)
    std::atomic<int> m_total_queries_sent;
    std::atomic<int> m_queries_generated;
    std::atomic<int> m_worker_count;
    
    // Configuration
    int m_num_workers;
    int m_query_delay_ms;
    
    // Callbacks for integration with main crawler
    std::function<void(const DHTQuery&)> m_query_callback;
    std::function<void()> m_progress_callback;
    
    // libtorrent session (shared, thread-safe)
    lt::session* m_session;
    
    // Hash tracking (thread-safe)
    std::mutex m_queried_hashes_mutex;
    std::set<std::string> m_queried_hashes;

public:
    ConcurrentDHTManager(lt::session* session, int num_workers = 4, int /* max_queue_size */ = 1000)
        : m_running(false)
        , m_shutdown_requested(false)
        , m_total_queries_sent(0)
        , m_queries_generated(0)
        , m_worker_count(0)
        , m_num_workers(num_workers)
        , m_query_delay_ms(10) // Reduced delay for concurrent processing
        , m_session(session)
    {
        // Initialize random number generators for each worker
        for (int i = 0; i < m_num_workers; ++i) {
            auto gen = std::make_unique<std::mt19937>(std::random_device{}());
            auto dis = std::make_unique<std::uniform_int_distribution<>>(0, 255);
            m_generators.push_back(std::move(gen));
            m_distributions.push_back(std::move(dis));
        }
    }
    
    ~ConcurrentDHTManager() {
        stop();
    }
    
    void start() {
        if (m_running) return;
        
        m_running = true;
        m_shutdown_requested = false;
        
        std::cout << "Starting " << m_num_workers << " DHT worker threads..." << std::endl;
        
        // Start worker threads
        for (int i = 0; i < m_num_workers; ++i) {
            m_workers.emplace_back(&ConcurrentDHTManager::worker_thread, this, i);
        }
        
        std::cout << "DHT worker pool started successfully" << std::endl;
    }
    
    void stop() {
        if (!m_running) return;
        
        std::cout << "Stopping DHT worker pool..." << std::endl;
        m_shutdown_requested = true;
        
        // Notify all workers to wake up
        m_queue_cv.notify_all();
        
        // Wait for all workers to finish
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        m_workers.clear();
        m_running = false;
        
        std::cout << "DHT worker pool stopped" << std::endl;
    }
    
    void set_query_callback(std::function<void(const DHTQuery&)> callback) {
        m_query_callback = callback;
    }
    
    void set_progress_callback(std::function<void()> callback) {
        m_progress_callback = callback;
    }
    
    void set_query_delay(int delay_ms) {
        m_query_delay_ms = delay_ms;
    }
    
    // Statistics
    int get_total_queries_sent() const { return m_total_queries_sent.load(); }
    int get_queries_generated() const { return m_queries_generated.load(); }
    int get_active_workers() const { return m_worker_count.load(); }
    size_t get_queue_size() const { 
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_queue_mutex));
        return m_query_queue.size();
    }
    
    void print_statistics() const {
        std::cout << "\n=== CONCURRENT DHT STATISTICS ===" << std::endl;
        std::cout << "Active workers: " << m_worker_count.load() << "/" << m_num_workers << std::endl;
        std::cout << "Queries generated: " << m_queries_generated.load() << std::endl;
        std::cout << "Queries sent: " << m_total_queries_sent.load() << std::endl;
        std::cout << "Queue size: " << get_queue_size() << std::endl;
        std::cout << "Query delay: " << m_query_delay_ms << "ms" << std::endl;
        std::cout << "=================================" << std::endl;
    }

private:
    void worker_thread(int worker_id) {
        m_worker_count++;
        
        std::cout << "DHT Worker " << worker_id << " started" << std::endl;
        
        while (m_running && !m_shutdown_requested) {
            try {
                // Generate and send DHT queries
                generate_and_send_queries(worker_id);
                
                // Small delay to prevent excessive CPU usage
                std::this_thread::sleep_for(std::chrono::milliseconds(m_query_delay_ms));
                
            } catch (const std::exception& e) {
                std::cerr << "Worker " << worker_id << " error: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        m_worker_count--;
        std::cout << "DHT Worker " << worker_id << " stopped" << std::endl;
    }
    
    void generate_and_send_queries(int worker_id) {
        // Generate multiple queries per worker iteration for efficiency
        const int queries_per_iteration = 5;
        
        for (int i = 0; i < queries_per_iteration && m_running && !m_shutdown_requested; ++i) {
            // Generate random hash
            lt::sha1_hash random_hash;
            auto& gen = *m_generators[worker_id];
            auto& dis = *m_distributions[worker_id];
            
            for (int j = 0; j < 20; ++j) {
                random_hash[j] = static_cast<unsigned char>(dis(gen));
            }
            
            std::string hash_str = random_hash.to_string();
            
            // Check if we've already queried this hash (thread-safe)
            {
                std::lock_guard<std::mutex> lock(m_queried_hashes_mutex);
                if (m_queried_hashes.find(hash_str) != m_queried_hashes.end()) {
                    continue; // Skip duplicate
                }
                m_queried_hashes.insert(hash_str);
            }
            
            // Create query object
            DHTQuery query(hash_str, random_hash);
            m_queries_generated++;
            
            // Send DHT queries (libtorrent session is thread-safe)
            try {
                m_session->dht_get_peers(random_hash);
                m_session->dht_get_item(random_hash);
                m_total_queries_sent += 2;
                
                // Notify callback if set
                if (m_query_callback) {
                    m_query_callback(query);
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Worker " << worker_id << " DHT query error: " << e.what() << std::endl;
            }
        }
        
        // Periodic progress callback
        if (m_progress_callback && (m_queries_generated.load() % 100 == 0)) {
            m_progress_callback();
        }
    }
};

#endif // DISABLE_LIBTORRENT
