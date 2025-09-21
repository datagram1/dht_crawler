# DHT Crawler

A comprehensive DHT (Distributed Hash Table) crawler that discovers torrents and peers from the BitTorrent DHT network, storing metadata in MySQL database.

## Features

- **DHT Network Crawling**: Discovers torrents and peers from the BitTorrent DHT network
- **MySQL Storage**: Stores discovered torrent metadata and peer information in MySQL database
- **Concurrent Processing**: Uses worker pool for parallel DHT query processing
- **Metadata Fetching**: Automatically fetches and stores torrent metadata
- **Cross-Platform**: Supports Linux, macOS, and Windows builds
- **Multiple Operation Modes**: 
  - Normal mode: Continuous DHT crawling
  - Metadata-only mode: Fetch metadata for specific torrent hashes
  - Concurrent mode: Parallel processing (default)
  - Sequential mode: Single-threaded processing (legacy)

## Requirements

### Libraries
- **libtorrent-rasterbar**: For DHT network access
- **libmysqlclient**: For MySQL database connectivity

### Installation Commands

#### macOS
```bash
brew install libtorrent-rasterbar mysql
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt install -y libtorrent-rasterbar2.0 libtorrent-rasterbar-dev libmysqlclient21 libmysqlclient-dev
```

#### Linux (CentOS/RHEL)
```bash
sudo yum install libtorrent-rasterbar-devel mysql-devel
```

## Building

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

## Usage

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

## Command Line Options

### Required Options
- `--user USER`: MySQL username for database connection
- `--password PASS`: MySQL password for database connection  
- `--database DB`: MySQL database name to store torrent data

### Optional Options
- `--server HOST`: MySQL server hostname (default: localhost)
- `--port PORT`: MySQL server port (default: 3306)
- `--queries NUM`: Maximum number of DHT queries to perform
- `--metadata HASHES`: Comma-delimited torrent hashes for metadata-only mode
- `--debug`: Enable detailed debug logging
- `--sequential`: Disable concurrent DHT worker pool
- `--workers NUM`: Number of concurrent DHT workers (default: 4)

## Database Schema

The tool automatically creates the following tables:

### discovered_torrents
Main torrent metadata and statistics including:
- Basic info (hash, name, size, files)
- Metadata (comment, creator, creation date)
- Technical details (piece length, trackers)
- Statistics (seeders, leechers, download speed)

### discovered_peers  
Peer information for each torrent including:
- Peer address and port
- Client information
- Discovery source and timestamps

### torrent_files
Individual file information within torrents including:
- File paths and sizes
- File hashes
- File indices

## Control

- **Ctrl+C**: Graceful shutdown
- **Ctrl+Z**: Pause (use `fg` to resume)

## Docker Support

Docker configurations are provided for Linux x86 builds:

```bash
# Build Docker image
docker build -f Dockerfile.linux-x86 -t dht_crawler .

# Run with docker-compose
docker-compose -f docker-compose.linux-x86.yml up
```

## License

This project is open source. Please check the license file for details.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.
