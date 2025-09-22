# Enhanced Metadata Extraction Using libtorrent Examples

## Overview

This document describes how the libtorrent examples from https://www.libtorrent.org/examples.html have been integrated into the DHT crawler to provide comprehensive metadata extraction capabilities.

## Key Improvements

### 1. **dump_torrent Example Integration**

The `dump_torrent` example provided the foundation for extracting comprehensive torrent metadata. Key features integrated:

#### Enhanced Metadata Structure
```cpp
struct EnhancedTorrentMetadata {
    std::string info_hash;
    std::string name;
    size_t total_size;
    int num_files;
    int num_pieces;
    int piece_length;
    std::string comment;
    std::string created_by;
    std::time_t creation_date;
    std::string magnet_link;
    std::vector<std::string> trackers;
    std::vector<int> tracker_tiers;
    std::vector<std::string> file_names;
    std::vector<size_t> file_sizes;
    std::vector<size_t> file_offsets;
    std::vector<std::string> file_flags; // executable, hidden, symlink, pad
    std::vector<std::string> web_seeds;
    bool private_torrent;
    std::string encoding;
};
```

#### Comprehensive File Information
- **File paths**: Complete file hierarchy
- **File sizes**: Individual file sizes
- **File offsets**: Position within the torrent
- **File flags**: Executable, hidden, symlink, pad file detection
- **File timestamps**: Modification times (when available)

#### Enhanced Tracker Information
- **Tracker URLs**: All tracker URLs
- **Tracker tiers**: Proper tier handling for tracker priority
- **Web seeds**: HTTP/FTP seed URLs
- **Announce lists**: Complete announce URL lists

### 2. **make_torrent Example Insights**

Understanding torrent creation helped improve metadata parsing:

#### Torrent Structure Understanding
- Piece hashing and organization
- File storage structure
- Metadata encoding
- Tracker tier organization

### 3. **simple_client Example Patterns**

Basic session management patterns were applied:

#### Proper Session Handling
- Correct torrent handle management
- Proper alert handling
- Clean resource management

## Implementation Details

### Enhanced Metadata Extraction Function

```cpp
EnhancedTorrentMetadata extract_comprehensive_metadata(const lt::torrent_info& ti, const std::string& info_hash) {
    EnhancedTorrentMetadata metadata;
    metadata.info_hash = info_hash;
    
    // Basic torrent information
    metadata.name = ti.name();
    metadata.total_size = ti.total_size();
    metadata.num_files = ti.num_files();
    metadata.num_pieces = ti.num_pieces();
    metadata.piece_length = ti.piece_length();
    metadata.comment = ti.comment();
    metadata.created_by = ti.creator();
    metadata.creation_date = ti.creation_date();
    metadata.private_torrent = ti.priv();
    
    // Generate magnet link using libtorrent's make_magnet_uri
    lt::add_torrent_params atp;
    atp.info_hashes = ti.info_hashes();
    atp.name = ti.name();
    metadata.magnet_link = lt::make_magnet_uri(atp);
    
    // Extract tracker information with tiers
    const auto& trackers = ti.trackers();
    const auto& tracker_tiers = ti.tracker_tiers();
    
    for (size_t i = 0; i < trackers.size(); ++i) {
        metadata.trackers.push_back(trackers[i].url);
        if (i < tracker_tiers.size()) {
            metadata.tracker_tiers.push_back(tracker_tiers[i]);
        } else {
            metadata.tracker_tiers.push_back(0); // Default tier
        }
    }
    
    // Extract detailed file information
    const lt::file_storage& fs = ti.files();
    for (auto i : fs.file_range()) {
        metadata.file_names.push_back(fs.file_path(i));
        metadata.file_sizes.push_back(fs.file_size(i));
        metadata.file_offsets.push_back(fs.file_offset(i));
        
        // Extract file flags
        auto flags = fs.file_flags(i);
        std::string flag_str = "";
        if (flags & lt::file_storage::flag_pad_file) flag_str += "pad,";
        if (flags & lt::file_storage::flag_executable) flag_str += "exec,";
        if (flags & lt::file_storage::flag_hidden) flag_str += "hidden,";
        if (flags & lt::file_storage::flag_symlink) flag_str += "symlink,";
        
        if (!flag_str.empty()) {
            flag_str.pop_back(); // Remove trailing comma
        }
        metadata.file_flags.push_back(flag_str);
    }
    
    // Extract web seeds
    const auto& url_seeds = ti.web_seeds();
    for (const auto& seed : url_seeds) {
        metadata.web_seeds.push_back(seed.url);
    }
    
    return metadata;
}
```

### Integration with Main Crawler

The enhanced metadata extraction is integrated into the main `handleMetadataReceived` function:

```cpp
void handleMetadataReceived(lt::metadata_received_alert* alert) {
    // ... existing code ...
    
    // Extract comprehensive metadata using enhanced extraction
    auto enhanced_metadata = m_metadata_downloader->extract_comprehensive_metadata(*torrent_info, hash_str);
    
    // Use enhanced metadata extraction results
    torrent.magnet_link = enhanced_metadata.magnet_link;
    torrent.name = enhanced_metadata.name;
    torrent.size = enhanced_metadata.total_size;
    torrent.num_files = enhanced_metadata.num_files;
    torrent.num_pieces = enhanced_metadata.num_pieces;
    torrent.piece_length = enhanced_metadata.piece_length;
    torrent.comment = enhanced_metadata.comment;
    torrent.created_by = enhanced_metadata.created_by;
    torrent.creation_date = enhanced_metadata.creation_date;
    torrent.private_torrent = enhanced_metadata.private_torrent;
    
    // Enhanced file information with flags and offsets
    torrent.file_names = enhanced_metadata.file_names;
    torrent.file_sizes = enhanced_metadata.file_sizes;
    
    // Enhanced tracker information with tiers
    torrent.trackers = enhanced_metadata.trackers;
    torrent.announce_list = enhanced_metadata.trackers;
    
    // ... rest of processing ...
}
```

## Benefits

### 1. **Comprehensive Metadata**
- Complete file information including flags and offsets
- Proper tracker tier handling
- Web seed detection
- Creation metadata (comment, creator, date)

### 2. **Better Database Storage**
- More detailed torrent information
- Proper file organization data
- Complete tracker information
- Enhanced search capabilities

### 3. **Improved Analysis**
- Better content type detection
- File structure analysis
- Tracker reliability assessment
- Torrent quality metrics

### 4. **Enhanced User Experience**
- More informative torrent listings
- Better search and filtering
- Detailed file information
- Complete metadata display

## Database Schema Considerations

The enhanced metadata extraction provides additional data that can be stored in the database:

### Additional Fields to Consider
- `file_offsets`: Array of file offsets within torrent
- `file_flags`: Array of file flags (executable, hidden, etc.)
- `tracker_tiers`: Array of tracker tier numbers
- `web_seeds`: Array of web seed URLs
- `creation_date`: Torrent creation timestamp
- `created_by`: Software that created the torrent
- `piece_length`: Size of each piece
- `num_pieces`: Total number of pieces

### Example Database Update
```sql
ALTER TABLE torrents ADD COLUMN file_offsets TEXT;
ALTER TABLE torrents ADD COLUMN file_flags TEXT;
ALTER TABLE torrents ADD COLUMN tracker_tiers TEXT;
ALTER TABLE torrents ADD COLUMN web_seeds TEXT;
ALTER TABLE torrents ADD COLUMN creation_date BIGINT;
ALTER TABLE torrents ADD COLUMN created_by VARCHAR(255);
ALTER TABLE torrents ADD COLUMN piece_length INT;
ALTER TABLE torrents ADD COLUMN num_pieces INT;
```

## Usage Examples

### Basic Metadata Extraction
```cpp
// The enhanced extraction is automatically used when metadata is received
auto enhanced_metadata = m_metadata_downloader->extract_comprehensive_metadata(*torrent_info, hash_str);

// Access comprehensive metadata
std::cout << "Torrent: " << enhanced_metadata.name << std::endl;
std::cout << "Files: " << enhanced_metadata.num_files << std::endl;
std::cout << "Trackers: " << enhanced_metadata.trackers.size() << std::endl;
std::cout << "Web seeds: " << enhanced_metadata.web_seeds.size() << std::endl;
```

### File Analysis
```cpp
// Analyze file structure
for (size_t i = 0; i < enhanced_metadata.file_names.size(); ++i) {
    std::cout << "File: " << enhanced_metadata.file_names[i] << std::endl;
    std::cout << "  Size: " << enhanced_metadata.file_sizes[i] << " bytes" << std::endl;
    std::cout << "  Offset: " << enhanced_metadata.file_offsets[i] << std::endl;
    std::cout << "  Flags: " << enhanced_metadata.file_flags[i] << std::endl;
}
```

### Tracker Analysis
```cpp
// Analyze tracker tiers
for (size_t i = 0; i < enhanced_metadata.trackers.size(); ++i) {
    std::cout << "Tier " << enhanced_metadata.tracker_tiers[i] 
              << ": " << enhanced_metadata.trackers[i] << std::endl;
}
```

## Conclusion

The integration of libtorrent examples has significantly enhanced the DHT crawler's metadata extraction capabilities. The system now provides:

1. **Complete torrent information** including file details, tracker tiers, and creation metadata
2. **Enhanced database storage** with comprehensive metadata fields
3. **Better analysis capabilities** for torrent quality and content assessment
4. **Improved user experience** with detailed torrent information

This enhancement makes the DHT crawler a more powerful tool for torrent discovery and analysis, providing the same level of detail as dedicated torrent analysis tools.
