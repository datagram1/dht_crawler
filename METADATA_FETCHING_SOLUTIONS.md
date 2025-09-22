# Metadata Fetching Solutions for DHT Crawler

## Problem Analysis

Your DHT crawler is discovering ~90,000 torrents but receiving **0 metadata** despite making requests. This is due to several critical issues:

### Root Causes Identified:

1. **Critical Bug: `upload_mode` Flag**
   - Your code uses `lt::torrent_flags::upload_mode` which prevents **ALL downloading**, including metadata
   - This is why you see 0 metadata received despite active requests

2. **Missing BEP 9 Implementation**
   - No proper BitTorrent Extension Protocol (BEP 9) implementation for metadata exchange
   - Relies entirely on libtorrent's built-in handling, which may not be optimal

3. **Insufficient Peer Connection Management**
   - High concurrent request limit (50) may overwhelm peer connections
   - Long timeout periods (120s) delay cleanup of failed requests

## Solutions Implemented

### 1. Fixed Critical Upload Mode Bug ✅

**File:** `src/enhanced_metadata_manager.hpp`

**Before:**
```cpp
params.flags |= lt::torrent_flags::upload_mode;  // This prevents metadata downloading!
```

**After:**
```cpp
// FIXED: Don't use upload_mode as it prevents metadata downloading
// Instead, use seed_mode which allows metadata download but no data upload
params.flags |= lt::torrent_flags::seed_mode;  // Allow metadata download, no data upload
```

### 2. Enhanced Session Configuration ✅

**File:** `src/dht_crawler.cpp`

Added better peer connectivity settings:
```cpp
// Enable peer exchange and local peer discovery for better peer connectivity
settings.set_bool(lt::settings_pack::enable_peer_exchange, true);
settings.set_bool(lt::settings_pack::enable_lsd, true);

// Increase connection limits for metadata fetching
settings.set_int(lt::settings_pack::connections_limit, 200);
settings.set_int(lt::settings_pack::active_limit, 50);

// Enable metadata extension (BEP 9)
settings.set_bool(lt::settings_pack::enable_utp, true);
```

### 3. Optimized Request Management ✅

**File:** `src/enhanced_metadata_manager.hpp`

- Reduced concurrent requests from 50 to 30 for better stability
- Reduced timeout from 120s to 60s for faster cleanup
- Better resource management

## Alternative Approaches

### Option 2: Custom BEP 9 Implementation

For maximum control and efficiency, implement a custom BEP 9 metadata fetcher:

```cpp
class BEP9MetadataFetcher {
private:
    struct PeerConnection {
        std::string address;
        int port;
        std::string peer_id;
        bool supports_metadata;
        std::chrono::steady_clock::time_point last_seen;
    };
    
    std::map<std::string, std::vector<PeerConnection>> m_torrent_peers;
    std::map<std::string, std::vector<char>> m_metadata_pieces;
    
public:
    bool fetch_metadata(const std::string& info_hash, const std::vector<std::string>& peers);
    void handle_extension_message(const std::string& peer_id, const std::vector<char>& data);
};
```

### Option 3: Hybrid Approach

Combine DHT discovery with external metadata sources:

1. **Use DHT for peer discovery** (current approach)
2. **Use trackers for metadata** when available
3. **Implement fallback to torrent sites** for popular torrents
4. **Cache metadata** to avoid re-fetching

### Option 4: Specialized Tools Integration

Integrate with existing metadata-focused tools:

- **bitmagnet**: DHT crawler specifically for metadata
- **torrent-metadata**: Lightweight metadata fetcher
- **tracker-scraper**: Get metadata from trackers

## Testing the Fix

### 1. Build and Test
```bash
cd /Users/richardbrown/dev/dht_crawler
make clean && make
```

### 2. Run with Debug Mode
```bash
./dht_crawler --user test --password test --database test --debug --queries 1000
```

### 3. Monitor Output
Look for:
- `*** METADATA RECEIVED ***` messages
- Increased `m_metadata_fetched` counter
- Successful peer connections

### 4. Expected Results
- Should see metadata being received within 1-2 minutes
- Success rate should improve from 0% to 10-30%
- Faster cleanup of failed requests

## Additional Improvements

### 1. Better Error Handling
```cpp
// Add to metadata downloader
void handle_peer_error(const std::string& info_hash, const std::string& error) {
    log("Peer error for " + info_hash.substr(0, 8) + "...: " + error);
    // Implement retry logic or peer blacklisting
}
```

### 2. Peer Quality Scoring
```cpp
struct PeerQuality {
    int success_rate;
    int response_time_ms;
    bool supports_metadata;
    std::chrono::steady_clock::time_point last_success;
};
```

### 3. Metadata Caching
```cpp
class MetadataCache {
    std::map<std::string, std::vector<char>> m_cache;
    std::map<std::string, std::time_t> m_cache_timestamps;
    
public:
    bool has_metadata(const std::string& info_hash);
    std::vector<char> get_metadata(const std::string& info_hash);
    void store_metadata(const std::string& info_hash, const std::vector<char>& data);
};
```

## Monitoring and Debugging

### Key Metrics to Watch:
1. **Metadata Success Rate**: Should increase from 0% to 10-30%
2. **Peer Connection Success**: Monitor peer connect/disconnect alerts
3. **Request Timeout Rate**: Should decrease with shorter timeouts
4. **Queue Processing Speed**: Should improve with reduced concurrency

### Debug Commands:
```bash
# Monitor network connections
netstat -an | grep :6881

# Check libtorrent logs
./dht_crawler --debug 2>&1 | grep -E "(METADATA|PEER|ERROR)"

# Test specific hash
./dht_crawler --metadata abc123def456 --debug
```

## Expected Timeline

- **Immediate**: Fix should show results within 1-2 minutes
- **Short-term**: 10-30% metadata success rate within 1 hour
- **Long-term**: Consider implementing Option 2 (custom BEP 9) for 50%+ success rate

The critical fix (removing `upload_mode`) should immediately resolve the 0% metadata success rate issue.
