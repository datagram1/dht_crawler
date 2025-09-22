// Enhanced metadata-only mode that forces DHT peer discovery
// Add this to your dht_crawler.cpp in the setupMetadataOnlyMode() function

void DHTTorrentCrawler::setupMetadataOnlyMode() {
    std::cout << "\n=== METADATA-ONLY MODE ===" << std::endl;
    m_metadata_only_mode = true;
    
    // ... existing hash loading code ...
    
    std::cout << "Total hashes to fetch metadata for: " << m_metadata_hash_list.size() << std::endl;
    
    // *** NEW: Force DHT peer discovery for each hash ***
    std::cout << "Starting DHT peer discovery for metadata hashes..." << std::endl;
    
    for (const std::string& hash : m_metadata_hash_list) {
        // Convert hex hash to binary for DHT query
        if (hash.length() == 40) {
            try {
                lt::sha1_hash binary_hash;
                // Convert hex string to binary hash
                for (int i = 0; i < 20; ++i) {
                    std::string byte_str = hash.substr(i * 2, 2);
                    binary_hash[i] = static_cast<unsigned char>(std::stoul(byte_str, nullptr, 16));
                }
                
                // *** CRITICAL: Query DHT for peers BEFORE requesting metadata ***
                std::cout << "Querying DHT for peers: " << hash.substr(0, 8) << "..." << std::endl;
                m_session->dht_get_peers(binary_hash);
                
                // Also try dht_get_item for additional discovery
                m_session->dht_get_item(binary_hash);
                
                m_total_queries += 2;
                
                // Small delay between DHT queries to avoid overwhelming the network
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
            } catch (const std::exception& e) {
                std::cerr << "Error converting hash " << hash << ": " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Invalid hash length: " << hash << " (expected 40 chars)" << std::endl;
        }
    }
    
    std::cout << "DHT peer discovery initiated for " << m_metadata_hash_list.size() << " hashes" << std::endl;
    std::cout << "Waiting for peer responses before requesting metadata..." << std::endl;
}

// *** NEW: Modified startCrawling for metadata-only mode ***
void DHTTorrentCrawler::startCrawling(int max_queries) {
    // ... existing code ...
    
    if (m_metadata_only_mode) {
        if (m_verbose_mode) {
            std::cout << "\n=== Starting Enhanced Metadata-Only Mode ===" << std::endl;
            std::cout << "Phase 1: DHT peer discovery for " << m_metadata_hash_list.size() << " torrents..." << std::endl;
        }
        
        // *** PHASE 1: Wait for DHT peer discovery (30 seconds) ***
        auto discovery_start = std::chrono::steady_clock::now();
        int discovery_timeout = 30; // 30 seconds for peer discovery
        
        std::cout << "Phase 1: Discovering peers via DHT..." << std::endl;
        
        while (m_running && !m_shutdown_requested) {
            processAlerts(); // This will handle peer discovery responses
            
            auto elapsed = std::chrono::steady_clock::now() - discovery_start;
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            
            if (elapsed_seconds >= discovery_timeout) {
                std::cout << "Phase 1 complete: Peer discovery timeout reached" << std::endl;
                break;
            }
            
            // Show progress every 5 seconds
            if (elapsed_seconds % 5 == 0) {
                std::cout << "Peer discovery progress: " << elapsed_seconds << "s, " 
                          << "torrents found: " << m_torrents_found 
                          << ", peers found: " << m_peers_found << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        std::cout << "\n*** PHASE 1 RESULTS ***" << std::endl;
        std::cout << "Torrents discovered: " << m_torrents_found << std::endl;
        std::cout << "Peers discovered: " << m_peers_found << std::endl;
        
        if (m_peers_found == 0) {
            std::cout << "⚠️  WARNING: No peers found via DHT!" << std::endl;
            std::cout << "   This may indicate:" << std::endl;
            std::cout << "   - Network connectivity issues" << std::endl;
            std::cout << "   - DHT bootstrap problems" << std::endl;
            std::cout << "   - Dead/unpopular torrents" << std::endl;
            std::cout << "   - Firewall blocking DHT traffic" << std::endl;
            
            // Try alternative approach: add torrents anyway and see if libtorrent can find peers
            std::cout << "   Attempting metadata requests anyway..." << std::endl;
        }
        
        // *** PHASE 2: Request metadata for discovered and undiscovered torrents ***
        std::cout << "\nPhase 2: Requesting metadata..." << std::endl;
        
        // Request metadata for all hashes (both discovered and undiscovered)
        for (const std::string& hash : m_metadata_hash_list) {
            requestMetadataForHash(hash);
        }
        
        // *** PHASE 3: Wait for metadata responses ***
        auto metadata_start = std::chrono::steady_clock::now();
        int metadata_timeout = 300; // 5 minutes for metadata
        
        std::cout << "Phase 3: Waiting for metadata responses..." << std::endl;
        
        while (m_running && !m_shutdown_requested) {
            processAlerts();
            
            auto elapsed = std::chrono::steady_clock::now() - metadata_start;
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            
            if (m_debug_mode && elapsed_seconds % 10 == 0) {
                std::cout << "[DEBUG] Metadata wait: " << elapsed_seconds << "s, fetched: " 
                          << m_metadata_fetched << "/" << m_metadata_hash_list.size() << std::endl;
            }
            
            if (elapsed_seconds >= metadata_timeout) {
                std::cout << "\n*** METADATA TIMEOUT REACHED ***" << std::endl;
                break;
            }
            
            if (m_metadata_fetched >= static_cast<int>(m_metadata_hash_list.size())) {
                std::cout << "\n*** ALL METADATA FETCHED ***" << std::endl;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        gracefulShutdown();
        return;
    }
    
    // ... rest of normal crawling mode ...
}
