/*
 * Enhanced Metadata Fetching with Tracker Support
 * Alternative approach when DHT fails
 */

// Add this to your enhanced_metadata_manager.hpp

class TrackerBasedMetadataFetcher {
private:
    // Popular public trackers that usually work
    std::vector<std::string> m_popular_trackers = {
        "udp://tracker.opentrackr.org:1337/announce",
        "udp://tracker.openbittorrent.com:6969/announce", 
        "udp://open.stealth.si:80/announce",
        "udp://exodus.desync.com:6969/announce",
        "udp://tracker.torrent.eu.org:451/announce",
        "udp://explodie.org:6969/announce",
        "udp://tracker1.bt.moack.co.kr:80/announce",
        "udp://tracker.theoks.net:6969/announce",
        "udp://opentracker.i2p.rocks:6969/announce",
        "udp://tracker.internetwarriors.net:1337/announce"
    };

public:
    // Enhanced magnet link creation with trackers
    std::string create_enhanced_magnet(const std::string& info_hash) {
        std::string magnet = "magnet:?xt=urn:btih:" + info_hash;
        
        // Add multiple trackers for better peer discovery
        for (const auto& tracker : m_popular_trackers) {
            magnet += "&tr=" + url_encode(tracker);
        }
        
        return magnet;
    }
    
    // URL encoding for tracker URLs
    std::string url_encode(const std::string& value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (char c : value) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else {
                escaped << std::uppercase;
                escaped << '%' << std::setw(2) << int((unsigned char)c);
                escaped << std::nouppercase;
            }
        }

        return escaped.str();
    }
};

// Modified process_single_request function
bool PersistentMetadataDownloader::process_single_request(const std::string& info_hash, int priority, const std::string& source) {
    try {
        // Convert hash format if needed
        std::string hex_hash = convert_hash_to_hex(info_hash);
        if (hex_hash.empty()) {
            log("Invalid hash format: " + info_hash.substr(0, 8) + "...");
            return false;
        }
        
        // *** NEW: Create enhanced magnet with multiple trackers ***
        TrackerBasedMetadataFetcher tracker_fetcher;
        std::string enhanced_magnet = tracker_fetcher.create_enhanced_magnet(hex_hash);
        
        log("Enhanced magnet created: " + enhanced_magnet.substr(0, 100) + "...");
        
        // Create torrent parameters
        lt::add_torrent_params params;
        
        // Parse enhanced magnet URI
        lt::error_code ec;
        params = lt::parse_magnet_uri(enhanced_magnet, ec);
        if (ec) {
            log("Failed to parse enhanced magnet URI: " + ec.message());
            return false;
        }
        
        params.save_path = ".";
        params.flags |= lt::torrent_flags::auto_managed;
        params.flags |= lt::torrent_flags::duplicate_is_error;
        
        // Use seed_mode for metadata-only downloading
        params.flags |= lt::torrent_flags::seed_mode;
        
        // *** NEW: Enhanced session settings for better connectivity ***
        // Force enable DHT even if it's having issues
        params.flags |= lt::torrent_flags::enable_dht;
        
        // Add torrent to session
        auto handle = m_session->add_torrent(params);
        
        if (handle.is_valid()) {
            m_active_tracker.add_request(info_hash, std::chrono::steady_clock::now(), priority, source, handle);
            log("Started ENHANCED metadata request for: " + info_hash.substr(0, 8) + "... (trackers: " + std::to_string(m_popular_trackers.size()) + ")");
            return true;
        } else {
            log("Failed to add enhanced torrent for metadata: " + info_hash.substr(0, 8) + "...");
            return false;
        }
    } catch (const std::exception& e) {
        log("Exception in enhanced metadata request for " + info_hash.substr(0, 8) + "...: " + e.what());
        return false;
    }
}
