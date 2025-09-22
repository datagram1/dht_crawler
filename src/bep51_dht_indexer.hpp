/*
 * BEP 51: DHT Infohash Indexing Implementation
 * 
 * This implements the BitTorrent Enhancement Proposal 51 for DHT infohash indexing,
 * which allows querying DHT nodes for actual infohashes instead of random generation.
 * 
 * Reference: https://github.com/the8472/bep51
 */

#pragma once

#ifndef DISABLE_LIBTORRENT
#include <libtorrent/session.hpp>
#include <libtorrent/sha1_hash.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/kademlia/dht_storage.hpp>
#endif

#include <vector>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <functional>
#include <atomic>

#ifndef DISABLE_LIBTORRENT

namespace dht_crawler {

struct BEP51Response {
    std::vector<std::string> infohashes;  // 20-byte binary infohashes
    std::vector<std::string> nodes;       // DHT nodes
    int interval;                         // Refresh interval in seconds
    int num_total;                        // Total number of infohashes in storage
};

class BEP51DHTIndexer {
public:
    BEP51DHTIndexer(lt::session* session, 
                    std::function<void(const std::string&)> log_callback = nullptr)
        : m_session(session)
        , m_log_callback(log_callback)
        , m_total_queries(0)
        , m_successful_queries(0)
        , m_infohashes_collected(0)
    {
    }

    // Send sample_infohashes query to a DHT node
    bool query_sample_infohashes(const std::string& node_id, const std::string& target_id = "") {
        if (!m_session) {
            log("Session not available for BEP51 query");
            return false;
        }

        try {
            // Create BEP51 query message
            lt::entry query;
            query["y"] = "q";
            query["q"] = "sample_infohashes";
            query["t"] = generate_transaction_id();
            
            lt::entry& args = query["a"];
            args["id"] = node_id;
            if (!target_id.empty()) {
                args["target"] = target_id;
            } else {
                // Use random target for broader sampling
                args["target"] = generate_random_id();
            }

            // Send query via DHT using the correct API
            m_session->dht_put_item(query);
            
            m_total_queries++;
            log("Sent BEP51 query to node: " + node_id.substr(0, 8) + "...");
            return true;
            
        } catch (const std::exception& e) {
            log("Error sending BEP51 query: " + std::string(e.what()));
            return false;
        }
    }

    // Process BEP51 response
    void handle_bep51_response(const std::string& node_id, const BEP51Response& response) {
        m_successful_queries++;
        m_infohashes_collected += response.infohashes.size();
        
        log("Received BEP51 response from " + node_id.substr(0, 8) + "...: " + 
            std::to_string(response.infohashes.size()) + " infohashes, " +
            std::to_string(response.nodes.size()) + " nodes");
        
        // Store infohashes for metadata fetching
        for (const auto& infohash : response.infohashes) {
            m_collected_infohashes.insert(infohash);
        }
        
        // Store nodes for further queries
        for (const auto& node : response.nodes) {
            if (node.length() >= 20) {  // Valid node ID length
                m_known_nodes.insert(node.substr(0, 20));
            }
        }
    }

    // Get collected infohashes
    std::vector<std::string> get_collected_infohashes() const {
        return std::vector<std::string>(m_collected_infohashes.begin(), m_collected_infohashes.end());
    }

    // Get known DHT nodes
    std::vector<std::string> get_known_nodes() const {
        return std::vector<std::string>(m_known_nodes.begin(), m_known_nodes.end());
    }

    // Statistics
    int get_total_queries() const { return m_total_queries; }
    int get_successful_queries() const { return m_successful_queries; }
    int get_infohashes_collected() const { return m_infohashes_collected; }
    size_t get_unique_infohashes() const { return m_collected_infohashes.size(); }
    size_t get_known_nodes_count() const { return m_known_nodes.size(); }

    void print_statistics() const {
        log("=== BEP51 DHT INDEXER STATISTICS ===");
        log("Total queries sent: " + std::to_string(m_total_queries));
        log("Successful queries: " + std::to_string(m_successful_queries));
        log("Infohashes collected: " + std::to_string(m_infohashes_collected));
        log("Unique infohashes: " + std::to_string(m_collected_infohashes.size()));
        log("Known DHT nodes: " + std::to_string(m_known_nodes.size()));
        log("Success rate: " + std::to_string(
            m_total_queries > 0 ? (m_successful_queries * 100 / m_total_queries) : 0) + "%");
        log("===================================");
    }

private:
    void log(const std::string& message) const {
        if (m_log_callback) {
            m_log_callback("[BEP51Indexer] " + message);
        }
    }

    std::string generate_transaction_id() {
        static std::atomic<int> counter(0);
        return std::to_string(counter++);
    }

    std::string generate_random_id() {
        std::string id(20, 0);
        for (int i = 0; i < 20; ++i) {
            id[i] = static_cast<char>(rand() % 256);
        }
        return id;
    }

    lt::session* m_session;
    std::function<void(const std::string&)> m_log_callback;
    
    std::atomic<int> m_total_queries;
    std::atomic<int> m_successful_queries;
    std::atomic<int> m_infohashes_collected;
    
    std::set<std::string> m_collected_infohashes;
    std::set<std::string> m_known_nodes;
};

} // namespace dht_crawler

#endif // DISABLE_LIBTORRENT
