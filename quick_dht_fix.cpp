// Quick fix: Add this to your setupMetadataOnlyMode() function in dht_crawler.cpp

void setupMetadataOnlyMode() {
    // ... existing code to load hashes ...
    
    // *** NEW: Force DHT peer discovery ***
    std::cout << "FORCING DHT peer discovery for " << m_metadata_hash_list.size() << " hashes..." << std::endl;
    
    for (const std::string& hash : m_metadata_hash_list) {
        if (hash.length() == 40) {
            try {
                // Convert hex to binary hash
                lt::sha1_hash binary_hash;
                for (int i = 0; i < 20; ++i) {
                    std::string byte_str = hash.substr(i * 2, 2);
                    binary_hash[i] = static_cast<unsigned char>(std::stoul(byte_str, nullptr, 16));
                }
                
                // *** FORCE DHT QUERIES ***
                std::cout << "DHT query for: " << hash.substr(0, 8) << "..." << std::endl;
                m_session->dht_get_peers(binary_hash);
                m_total_queries++; // Increment counter
                
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                
            } catch (const std::exception& e) {
                std::cerr << "Hash conversion error: " << e.what() << std::endl;
            }
        }
    }
    
    std::cout << "DHT queries sent: " << m_total_queries << std::endl;
    std::cout << "Waiting 30 seconds for peer responses..." << std::endl;
    
    // Wait for peer discovery
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < 30) {
        processAlerts(); // Process DHT responses
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (m_peers_found > 0) {
            std::cout << "Peers found: " << m_peers_found << std::endl;
        }
    }
    
    std::cout << "Peer discovery phase complete. Torrents: " << m_torrents_found << ", Peers: " << m_peers_found << std::endl;
}
