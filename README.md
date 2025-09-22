# DHT Crawler

A comprehensive DHT (Distributed Hash Table) crawler that discovers torrents and peers from the BitTorrent DHT network, storing metadata in MySQL database with advanced concurrent processing capabilities.

## 🚀 Features

### Core Functionality
- **DHT Network Crawling**: Discovers torrents and peers from the BitTorrent DHT network
- **MySQL Storage**: Stores discovered torrent metadata and peer information in MySQL database
- **Concurrent Processing**: Uses worker pool for parallel DHT query processing (4 workers by default)
- **Metadata Fetching**: Automatically fetches and stores comprehensive torrent metadata
- **Cross-Platform**: Supports Linux, macOS, and Windows builds
- **Multiple Operation Modes**: 
  - Normal mode: Continuous DHT crawling
  - Metadata-only mode: Fetch metadata for specific torrent hashes
  - Concurrent mode: Parallel processing (default)
  - Sequential mode: Single-threaded processing (legacy)

### Advanced Features
- **Enhanced Metadata Management**: Comprehensive torrent metadata extraction and storage
- **Persistent Metadata Downloader**: Unlimited queue with priority-based processing
- **Concurrent DHT Manager**: Multi-threaded worker pool for high-performance crawling
- **Hash Format Support**: Handles hex, base32, and binary hash formats
- **Content Type Detection**: Automatically categorizes torrents by file types
- **Peer Discovery**: Tracks peer information and client details
- **Graceful Shutdown**: Proper cleanup and statistics reporting
- **Signal Handling**: Responds to Ctrl+C, Ctrl+Z, and SIGTERM signals
- **Debug Mode**: Detailed logging and verbose output for troubleshooting
- **Performance Monitoring**: Real-time statistics and rate calculations

## 📋 Requirements

### Libraries
- **libtorrent-rasterbar**: For DHT network access and BitTorrent protocol support
- **libmysqlclient**: For MySQL database connectivity
- **Google Test** (optional): For running unit and integration tests

### Installation Commands

#### macOS
```bash
# Required libraries
brew install libtorrent-rasterbar mysql

# Optional: For testing
brew install googletest
```

#### Linux (Ubuntu/Debian)
```bash
# Required libraries
sudo apt install -y libtorrent-rasterbar2.0 libtorrent-rasterbar-dev libmysqlclient21 libmysqlclient-dev

# Optional: For testing
sudo apt install -y libgtest-dev
```

#### Linux (CentOS/RHEL)
```bash
# Required libraries
sudo yum install libtorrent-rasterbar-devel mysql-devel

# Optional: For testing
sudo yum install gtest-devel
```

## 🔨 Building

### Using CMake
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Using the provided scripts
```bash
# Build release version
./scripts/build_release.sh

# Build for specific platforms
./scripts/build_linux_x86.sh
./scripts/build_windows_x86.sh
```

### Building with Tests
```bash
mkdir build && cd build
cmake .. -DENABLE_TESTING=ON
make -j$(nproc)

# Run tests
./tests/run_tests.sh
```

## 🎯 Usage

### Basic Usage
```bash
./dht_crawler --user admin --password secret --database torrents
```

### Remote MySQL Server
```bash
./dht_crawler --server 192.168.10.100 --user keynetworks --password K3yn3tw0rk5 --database torrents --queries 5000
```

### Metadata-Only Mode
```bash
# For specific hashes
./dht_crawler --user admin --password secret --database torrents --metadata abc123def456,789xyz012

# From file
./dht_crawler --user admin --password secret --database torrents --metadata /path/to/hashes.txt
```

### Debug Mode
```bash
./dht_crawler --user admin --password secret --database torrents --debug --queries 100
```

### High-Performance Mode
```bash
./dht_crawler --user admin --password secret --database torrents --workers 8
```

### Sequential Mode (Legacy)
```bash
./dht_crawler --user admin --password secret --database torrents --sequential
```

## ⚙️ Command Line Options

### Required Options
- `--user USER`: MySQL username for database connection
- `--password PASS`: MySQL password for database connection  
- `--database DB`: MySQL database name to store torrent data

### Optional Options
- `--server HOST`: MySQL server hostname (default: localhost)
- `--port PORT`: MySQL server port (default: 3306)
- `--queries NUM`: Maximum number of DHT queries to perform
- `--metadata HASHES`: Comma-delimited torrent hashes for metadata-only mode
- `--debug`: Enable detailed debug logging and verbose output
- `--sequential`: Disable concurrent DHT worker pool (use sequential mode)
- `--workers NUM`: Number of concurrent DHT workers (default: 4, max: 16)
- `--help`: Show help message and exit
- `--test-missing-libs`: Show help with simulated missing libraries (for testing)

## 🗄️ Database Schema

The tool automatically creates the following tables:

### discovered_torrents
Main torrent metadata and statistics including:
- **Basic Info**: `info_hash`, `name`, `size`, `num_files`
- **Metadata**: `comment`, `created_by`, `creation_date`, `encoding`
- **Technical Details**: `piece_length`, `num_pieces`, `trackers`, `private_torrent`
- **Statistics**: `seeders_count`, `leechers_count`, `download_speed`
- **Content Classification**: `content_type`, `language`, `category`
- **Tracking**: `source`, `metadata_received`, `timed_out`, `discovered_at`, `last_seen_at`

### discovered_peers  
Peer information for each torrent including:
- **Connection Info**: `peer_address`, `peer_port`, `peer_id`
- **Client Details**: `client_name`, `client_version`, `connection_type`
- **Tracking**: `source`, `discovered_at`, `last_seen_at`

### torrent_files
Individual file information within torrents including:
- **File Details**: `file_path`, `file_size`, `file_hash`
- **Organization**: `file_index`, `torrent_hash`

## 🎮 Control

- **Ctrl+C**: Graceful shutdown with statistics summary
- **Ctrl+Z**: Pause (use `fg` to resume)
- **SIGTERM**: Clean shutdown signal

## 🐳 Docker Support

Docker configurations are provided for Linux x86 builds:

```bash
# Build Docker image
docker build -f Dockerfile.linux-x86 -t dht_crawler .

# Run with docker-compose
docker-compose -f docker-compose.linux-x86.yml up
```

## 🧪 Testing

The project includes comprehensive unit and integration tests:

### Running Tests
```bash
# Run all tests
./tests/run_tests.sh

# Run specific test suites
./build/test/unit_tests
./build/test/integration_tests

# Run with CTest
cd build && ctest --verbose
```

### Test Coverage
- **Unit Tests**: Core functionality, data structures, and algorithms
- **Integration Tests**: End-to-end workflows and system interactions
- **Mock Tests**: Database and network operations without external dependencies

## 📊 Performance

### Concurrent Mode (Default)
- **Worker Pool**: 4 concurrent DHT workers
- **Query Rate**: ~100-500 queries/second
- **Metadata Processing**: 50 concurrent metadata requests
- **Memory Usage**: ~50-100MB typical

### Sequential Mode (Legacy)
- **Single Thread**: Sequential DHT query processing
- **Query Rate**: ~10-50 queries/second
- **Metadata Processing**: 10 concurrent metadata requests
- **Memory Usage**: ~20-50MB typical

## 🔧 Configuration

### MySQL Configuration
```sql
-- Recommended MySQL settings for optimal performance
SET innodb_buffer_pool_size = 1G;
SET innodb_log_file_size = 256M;
SET max_connections = 200;
SET query_cache_size = 64M;
```

### System Requirements
- **CPU**: 2+ cores recommended for concurrent mode
- **RAM**: 512MB minimum, 2GB+ recommended
- **Storage**: 1GB+ for database growth
- **Network**: Stable internet connection for DHT access

## 🚨 Troubleshooting

### Common Issues

#### Library Not Found
```bash
# Check library installation
pkg-config --exists libtorrent-rasterbar && echo "libtorrent OK" || echo "libtorrent MISSING"
pkg-config --exists mysqlclient && echo "MySQL OK" || echo "MySQL MISSING"
```

#### Database Connection Issues
- Verify MySQL server is running
- Check username/password credentials
- Ensure database exists
- Verify network connectivity

#### Performance Issues
- Enable debug mode to see detailed statistics
- Reduce worker count if system is overloaded
- Check MySQL performance settings
- Monitor system resources

### Debug Mode Output
```bash
# Enable debug mode for detailed logging
./dht_crawler --user admin --password secret --database torrents --debug
```

## 📈 Statistics

The crawler provides real-time statistics including:
- **DHT Queries**: Total queries sent and rate
- **Torrent Discovery**: Number of torrents found
- **Peer Discovery**: Number of peers discovered
- **Metadata Fetching**: Success rate and processing statistics
- **Performance Metrics**: Query rates and processing times

## 🔒 Security Considerations

- **Database Security**: Use strong passwords and restrict database access
- **Network Security**: Consider firewall rules for MySQL access
- **Data Privacy**: Be aware of local data retention policies
- **Resource Limits**: Monitor system resources to prevent overload

## 📝 License

This project is open source. Please check the license file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

### Development Setup
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Run the test suite
6. Submit a pull request

## 📚 Additional Resources

- [BitTorrent DHT Protocol](https://en.wikipedia.org/wiki/Distributed_hash_table)
- [libtorrent Documentation](https://libtorrent.org/)
- [MySQL Documentation](https://dev.mysql.com/doc/)
- [Google Test Documentation](https://google.github.io/googletest/)

## 🆘 Support

For support and questions:
- Check the troubleshooting section above
- Review the debug output for error details
- Open an issue on the project repository
- Check the project documentation

---

**Note**: This tool is designed for research and educational purposes. Please ensure compliance with local laws and regulations regarding BitTorrent usage and data collection.
