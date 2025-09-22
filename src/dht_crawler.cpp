#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <signal.h>
#include <atomic>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <unistd.h>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef DISABLE_LIBTORRENT
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#endif

#ifndef DISABLE_MYSQL
// MySQL includes
#include <mysql/mysql.h>
#endif

// Enhanced metadata transfer extension
#include "enhanced_metadata_manager.hpp"
#include "concurrent_dht_manager.hpp"
#include "bep51_dht_indexer.hpp"
#include "smart_dht_crawler.hpp"
#include "metadata_worker_pool.hpp"

struct DiscoveredTorrent {
    std::string info_hash;
    std::string name;
    size_t size;
    int num_files;
    std::vector<std::string> peers;
    std::vector<std::string> file_names;
    std::vector<size_t> file_sizes;
    std::string comment;
    std::string created_by;
    std::time_t creation_date;
    std::string encoding;
    size_t piece_length;
    int num_pieces;
    std::vector<std::string> trackers;
    bool private_torrent;
    std::string magnet_link;
    std::string announce_url;
    std::vector<std::string> announce_list;
    std::string content_type;
    std::string language;
    std::string category;
    int seeders_count;
    int leechers_count;
    size_t download_speed;
    std::chrono::steady_clock::time_point discovered_time;
    std::chrono::steady_clock::time_point last_seen_time;
    std::string source;
    bool metadata_received;
    bool timed_out;
};

struct MySQLConfig {
    std::string server;
    std::string user;
    std::string password;
    std::string database;
    int port = 3306;
    std::string metadata_hashes = ""; // Comma-delimited hashes for metadata-only mode
    bool debug_mode = false; // Enable debug logging
    bool verbose_mode = false; // Enable verbose output (default is counter mode)
    bool metadata_log_mode = false; // Enable metadata-only logging (suppress all other logs)
    bool concurrent_mode = true; // Enable concurrent DHT worker pool
    int num_workers = 4; // Number of concurrent workers
    bool bep51_mode = true; // Enable BEP51 DHT infohash indexing
};

#ifndef DISABLE_MYSQL
class MySQLConnection {
private:
    MYSQL* m_connection;
    MySQLConfig m_config;
    bool m_connected;

public:
    MySQLConnection(const MySQLConfig& config) : m_config(config), m_connected(false) {
        m_connection = mysql_init(nullptr);
    }

    ~MySQLConnection() {
        if (m_connection) {
            mysql_close(m_connection);
        }
    }

    bool connect() {
        try {
            if (!m_connection) {
                std::cerr << "MySQL initialization failed" << std::endl;
                logError("MySQLConnection::connect", "", -1, "MySQL initialization failed", "", "CRITICAL");
                return false;
            }

            if (!mysql_real_connect(m_connection, 
                                   m_config.server.c_str(),
                                   m_config.user.c_str(),
                                   m_config.password.c_str(),
                                   m_config.database.c_str(),
                                   m_config.port, nullptr, 0)) {
                std::string error_msg = mysql_error(m_connection);
                std::cerr << "MySQL connection failed: " << error_msg << std::endl;
                logError("MySQLConnection::connect", "", mysql_errno(m_connection), error_msg, "", "CRITICAL", 
                        "server=" + m_config.server + ", database=" + m_config.database);
                return false;
            }

            m_connected = true;
            std::cout << "Connected to MySQL database: " << m_config.database << std::endl;
            
            // Create tables if they don't exist
            createTables();
            return true;
        } catch (const std::exception& e) {
            logException("MySQLConnection::connect", "", e, "server=" + m_config.server + ", database=" + m_config.database);
            return false;
        }
    }

    void createTables() {
        if (!m_connected) return;

        // Create enhanced discovered_torrents table
        std::string createTorrentsTable = R"(
            CREATE TABLE IF NOT EXISTS discovered_torrents (
                id INT AUTO_INCREMENT PRIMARY KEY,
                info_hash VARCHAR(40) NOT NULL UNIQUE,
                name VARCHAR(500) NULL,
                size BIGINT DEFAULT 0 NULL,
                num_files INT DEFAULT 0 NULL,
                file_names TEXT NULL,
                file_sizes TEXT NULL,
                comment TEXT NULL,
                created_by VARCHAR(255) NULL,
                creation_date TIMESTAMP NULL,
                encoding VARCHAR(50) NULL,
                piece_length BIGINT DEFAULT 0 NULL,
                num_pieces INT DEFAULT 0 NULL,
                trackers TEXT NULL,
                private_torrent BOOLEAN DEFAULT FALSE NULL,
                source VARCHAR(50) DEFAULT 'DHT' NULL,
                metadata_received BOOLEAN DEFAULT FALSE NULL,
                timed_out BOOLEAN DEFAULT FALSE NULL,
                magnet_link TEXT NULL,
                announce_url VARCHAR(500) NULL,
                announce_list TEXT NULL,
                content_type VARCHAR(100) NULL,
                language VARCHAR(10) NULL,
                category VARCHAR(100) NULL,
                seeders_count INT DEFAULT 0 NULL,
                leechers_count INT DEFAULT 0 NULL,
                download_speed BIGINT DEFAULT 0 NULL,
                discovered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                last_seen_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL ON UPDATE CURRENT_TIMESTAMP,
                INDEX idx_info_hash (info_hash),
                INDEX idx_discovered_at (discovered_at),
                INDEX idx_source (source),
                INDEX idx_metadata_received (metadata_received),
                INDEX idx_timed_out (timed_out),
                INDEX idx_name (name(100)),
                INDEX idx_size (size),
                INDEX idx_num_files (num_files),
                INDEX idx_content_type (content_type),
                INDEX idx_category (category),
                INDEX idx_last_seen (last_seen_at)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        )";

        if (mysql_query(m_connection, createTorrentsTable.c_str())) {
            std::cerr << "Error creating torrents table: " << mysql_error(m_connection) << std::endl;
        } else {
            std::cout << "Created/verified enhanced discovered_torrents table" << std::endl;
        }

        // Create enhanced discovered_peers table
        std::string createPeersTable = R"(
            CREATE TABLE IF NOT EXISTS discovered_peers (
                id INT AUTO_INCREMENT PRIMARY KEY,
                torrent_hash VARCHAR(40) NOT NULL,
                peer_address VARCHAR(45) NOT NULL,
                peer_port INT NOT NULL,
                peer_id VARCHAR(40) NULL,
                client_name VARCHAR(100) NULL,
                client_version VARCHAR(50) NULL,
                connection_type VARCHAR(20) NULL,
                source VARCHAR(50) DEFAULT 'DHT' NULL,
                discovered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                last_seen_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                UNIQUE KEY unique_peer (torrent_hash, peer_address, peer_port),
                INDEX idx_torrent_hash (torrent_hash),
                INDEX idx_peer_address (peer_address),
                INDEX idx_discovered_at (discovered_at),
                INDEX idx_source (source),
                INDEX idx_client_name (client_name),
                FOREIGN KEY (torrent_hash) REFERENCES discovered_torrents(info_hash) ON DELETE CASCADE
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        )";

        if (mysql_query(m_connection, createPeersTable.c_str())) {
            std::cerr << "Error creating peers table: " << mysql_error(m_connection) << std::endl;
        } else {
            std::cout << "Created/verified enhanced discovered_peers table" << std::endl;
        }

        // Create torrent_files table
        std::string createFilesTable = R"(
            CREATE TABLE IF NOT EXISTS torrent_files (
                id INT AUTO_INCREMENT PRIMARY KEY,
                torrent_hash VARCHAR(40) NOT NULL,
                file_index INT NOT NULL,
                file_path VARCHAR(1000) NOT NULL,
                file_size BIGINT NOT NULL,
                file_hash VARCHAR(40) NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                UNIQUE KEY unique_file (torrent_hash, file_index),
                INDEX idx_torrent_hash (torrent_hash),
                INDEX idx_file_path (file_path(100)),
                INDEX idx_file_size (file_size)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        )";

        if (mysql_query(m_connection, createFilesTable.c_str())) {
            std::cerr << "Error creating files table: " << mysql_error(m_connection) << std::endl;
        } else {
            std::cout << "Created/verified torrent_files table" << std::endl;
        }

        // Create error log table
        std::string createLogTable = R"(
            CREATE TABLE IF NOT EXISTS log (
                id INT AUTO_INCREMENT PRIMARY KEY,
                timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                function_name VARCHAR(255) NOT NULL,
                caller_function VARCHAR(255) NULL,
                error_code INT NULL,
                error_message TEXT NULL,
                stack_trace TEXT NULL,
                severity ENUM('DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL') DEFAULT 'ERROR' NULL,
                thread_id VARCHAR(50) NULL,
                process_id INT NULL,
                additional_data TEXT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP NULL,
                INDEX idx_timestamp (timestamp),
                INDEX idx_function_name (function_name),
                INDEX idx_caller_function (caller_function),
                INDEX idx_error_code (error_code),
                INDEX idx_severity (severity),
                INDEX idx_thread_id (thread_id),
                INDEX idx_created_at (created_at)
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
        )";

        if (mysql_query(m_connection, createLogTable.c_str())) {
            std::cerr << "Error creating log table: " << mysql_error(m_connection) << std::endl;
        } else {
            std::cout << "Created/verified log table" << std::endl;
        }
    }

    bool storeTorrent(const DiscoveredTorrent& torrent) {
        try {
            if (!m_connected) {
                logError("MySQLConnection::storeTorrent", "", -1, "Not connected to database", "", "WARNING");
                return false;
            }

        // Convert file names to comma-separated string
        std::string file_names_str;
        for (size_t i = 0; i < torrent.file_names.size(); ++i) {
            if (i > 0) file_names_str += ", ";
            file_names_str += torrent.file_names[i];
        }

        // Convert file sizes to JSON array
        std::string file_sizes_str = "[";
        for (size_t i = 0; i < torrent.file_sizes.size(); ++i) {
            if (i > 0) file_sizes_str += ", ";
            file_sizes_str += std::to_string(torrent.file_sizes[i]);
        }
        file_sizes_str += "]";

        // Convert trackers to JSON array
        std::string trackers_str = "[";
        for (size_t i = 0; i < torrent.trackers.size(); ++i) {
            if (i > 0) trackers_str += ", ";
            trackers_str += "\"" + escapeString(torrent.trackers[i]) + "\"";
        }
        trackers_str += "]";

        // Convert announce list to JSON array
        std::string announce_list_str = "[";
        for (size_t i = 0; i < torrent.announce_list.size(); ++i) {
            if (i > 0) announce_list_str += ", ";
            announce_list_str += "\"" + escapeString(torrent.announce_list[i]) + "\"";
        }
        announce_list_str += "]";

        // Format creation date
        std::string creation_date_str = "NULL";
        if (torrent.creation_date > 0) {
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&torrent.creation_date));
            creation_date_str = "'" + std::string(buffer) + "'";
        }

        // Insert or update torrent with all metadata
        std::string query = "INSERT INTO discovered_torrents ("
                           "info_hash, name, size, num_files, file_names, file_sizes, "
                           "comment, created_by, creation_date, encoding, piece_length, num_pieces, "
                           "trackers, private_torrent, source, metadata_received, timed_out, magnet_link, "
                           "announce_url, announce_list, content_type, language, category, "
                           "seeders_count, leechers_count, download_speed, last_seen_at"
                           ") VALUES ("
                           "'" + escapeString(torrent.info_hash) + "', "
                           "'" + escapeString(torrent.name) + "', "
                           + std::to_string(torrent.size) + ", "
                           + std::to_string(torrent.num_files) + ", "
                           "'" + escapeString(file_names_str) + "', "
                           "'" + escapeString(file_sizes_str) + "', "
                           "'" + escapeString(torrent.comment) + "', "
                           "'" + escapeString(torrent.created_by) + "', "
                           + creation_date_str + ", "
                           "'" + escapeString(torrent.encoding) + "', "
                           + std::to_string(torrent.piece_length) + ", "
                           + std::to_string(torrent.num_pieces) + ", "
                           "'" + escapeString(trackers_str) + "', "
                           + (torrent.private_torrent ? "TRUE" : "FALSE") + ", "
                           "'" + escapeString(torrent.source) + "', "
                           + (torrent.metadata_received ? "TRUE" : "FALSE") + ", "
                           + (torrent.timed_out ? "TRUE" : "FALSE") + ", "
                           "'" + escapeString(torrent.magnet_link) + "', "
                           "'" + escapeString(torrent.announce_url) + "', "
                           "'" + escapeString(announce_list_str) + "', "
                           "'" + escapeString(torrent.content_type) + "', "
                           "'" + escapeString(torrent.language) + "', "
                           "'" + escapeString(torrent.category) + "', "
                           + std::to_string(torrent.seeders_count) + ", "
                           + std::to_string(torrent.leechers_count) + ", "
                           + std::to_string(torrent.download_speed) + ", "
                           "CURRENT_TIMESTAMP"
                           ") ON DUPLICATE KEY UPDATE "
                           "name = VALUES(name), "
                           "size = VALUES(size), "
                           "num_files = VALUES(num_files), "
                           "file_names = VALUES(file_names), "
                           "file_sizes = VALUES(file_sizes), "
                           "comment = VALUES(comment), "
                           "created_by = VALUES(created_by), "
                           "creation_date = VALUES(creation_date), "
                           "encoding = VALUES(encoding), "
                           "piece_length = VALUES(piece_length), "
                           "num_pieces = VALUES(num_pieces), "
                           "trackers = VALUES(trackers), "
                           "private_torrent = VALUES(private_torrent), "
                           "metadata_received = VALUES(metadata_received), "
                           "magnet_link = VALUES(magnet_link), "
                           "announce_url = VALUES(announce_url), "
                           "announce_list = VALUES(announce_list), "
                           "content_type = VALUES(content_type), "
                           "language = VALUES(language), "
                           "category = VALUES(category), "
                           "seeders_count = VALUES(seeders_count), "
                           "leechers_count = VALUES(leechers_count), "
                           "download_speed = VALUES(download_speed), "
                           "last_seen_at = CURRENT_TIMESTAMP, "
                           "updated_at = CURRENT_TIMESTAMP";

        if (mysql_query(m_connection, query.c_str())) {
            std::string error_msg = mysql_error(m_connection);
            std::cerr << "Error storing torrent: " << error_msg << std::endl;
            logError("MySQLConnection::storeTorrent", "", mysql_errno(m_connection), error_msg, "", "ERROR", 
                    "info_hash=" + torrent.info_hash);
            return false;
        }

        // Store peers with enhanced information
        for (const auto& peer : torrent.peers) {
            size_t colon_pos = peer.find(':');
            if (colon_pos != std::string::npos) {
                std::string address = peer.substr(0, colon_pos);
                std::string port_str = peer.substr(colon_pos + 1);
                
                std::string peerQuery = "INSERT IGNORE INTO discovered_peers (torrent_hash, peer_address, peer_port, source) "
                                      "VALUES ('" + escapeString(torrent.info_hash) + "', "
                                      "'" + escapeString(address) + "', "
                                      + port_str + ", "
                                      "'" + escapeString(torrent.source) + "')";

                if (mysql_query(m_connection, peerQuery.c_str())) {
                    std::cerr << "Error storing peer: " << mysql_error(m_connection) << std::endl;
                }
            }
        }

        return true;
        } catch (const std::exception& e) {
            logException("MySQLConnection::storeTorrent", "", e, "info_hash=" + torrent.info_hash);
            return false;
        }
    }

    bool isConnected() const {
        return m_connected;
    }
    
    const MySQLConfig& getConfig() const {
        return m_config;
    }

private:
    std::string escapeString(const std::string& str) {
        if (!m_connection) return str;
        
        char* escaped = new char[str.length() * 2 + 1];
        mysql_real_escape_string(m_connection, escaped, str.c_str(), str.length());
        std::string result(escaped);
        delete[] escaped;
        return result;
    }

public:
    bool markTorrentTimedOut(const std::string& info_hash) {
        try {
            if (!m_connected) {
                logError("MySQLConnection::markTorrentTimedOut", "", -1, "Not connected to database", "", "WARNING");
                return false;
            }

            std::string query = "UPDATE discovered_torrents SET timed_out = TRUE WHERE info_hash = '" + escapeString(info_hash) + "'";
            
            if (mysql_query(m_connection, query.c_str())) {
                std::string error_msg = mysql_error(m_connection);
                std::cerr << "MySQL error updating timed_out: " << error_msg << std::endl;
                logError("MySQLConnection::markTorrentTimedOut", "", mysql_errno(m_connection), error_msg, "", "ERROR", 
                        "info_hash=" + info_hash);
                return false;
            }
            
            return true;
        } catch (const std::exception& e) {
            logException("MySQLConnection::markTorrentTimedOut", "", e, "info_hash=" + info_hash);
            return false;
        }
    }

    // Error logging functionality
    bool logError(const std::string& function_name, 
                  const std::string& caller_function = "",
                  int error_code = 0,
                  const std::string& error_message = "",
                  const std::string& stack_trace = "",
                  const std::string& severity = "ERROR",
                  const std::string& additional_data = "") {
        if (!m_connected) return false;

        // Get thread ID
        std::string thread_id = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        
        // Get process ID
        int process_id = getpid();

        std::string query = "INSERT INTO log ("
                           "function_name, caller_function, error_code, error_message, "
                           "stack_trace, severity, thread_id, process_id, additional_data"
                           ") VALUES ("
                           "'" + escapeString(function_name) + "', "
                           "'" + escapeString(caller_function) + "', "
                           + std::to_string(error_code) + ", "
                           "'" + escapeString(error_message) + "', "
                           "'" + escapeString(stack_trace) + "', "
                           "'" + escapeString(severity) + "', "
                           "'" + escapeString(thread_id) + "', "
                           + std::to_string(process_id) + ", "
                           "'" + escapeString(additional_data) + "'"
                           ")";

        if (mysql_query(m_connection, query.c_str())) {
            std::cerr << "MySQL error logging error: " << mysql_error(m_connection) << std::endl;
            return false;
        }
        
        return true;
    }

    bool logException(const std::string& function_name,
                      const std::string& caller_function,
                      const std::exception& e,
                      const std::string& additional_data = "") {
        return logError(function_name, caller_function, -1, e.what(), "", "CRITICAL", additional_data);
    }
};

#endif // DISABLE_MYSQL

#ifndef DISABLE_LIBTORRENT
class DHTTorrentCrawler {
private:
    std::unique_ptr<lt::session> m_session;
    std::unique_ptr<MySQLConnection> m_mysql;
    std::map<std::string, DiscoveredTorrent> m_discovered_torrents;
    std::set<std::string> m_queried_hashes;
    std::set<std::string> m_metadata_requested;
    std::random_device m_rd;
    std::mt19937 m_gen;
    std::uniform_int_distribution<> m_dis;
    std::atomic<bool> m_running;
    std::atomic<bool> m_shutdown_requested;
    int m_total_queries;
    int m_torrents_found;
    int m_peers_found;
    int m_metadata_fetched;
    bool m_metadata_only_mode;
    std::vector<std::string> m_metadata_hash_list;
    bool m_debug_mode;
    bool m_verbose_mode;
    bool m_metadata_log_mode;
    
    // Enhanced metadata management
    std::unique_ptr<dht_crawler::MetadataManager> m_metadata_manager;
    std::unique_ptr<dht_crawler::PersistentMetadataDownloader> m_metadata_downloader;
    std::unique_ptr<MetadataWorkerPool> m_metadata_worker_pool;
    std::function<void(const std::string&)> m_log_callback;
    
    // Concurrent DHT management
    std::unique_ptr<ConcurrentDHTManager> m_concurrent_dht;
    std::atomic<bool> m_use_concurrent_mode;
    
    // BEP51 DHT Indexing
    std::unique_ptr<dht_crawler::BEP51DHTIndexer> m_bep51_indexer;
    std::atomic<bool> m_use_bep51_mode;
    
    // Smart DHT Crawling (rate-limited, observation-based)
    std::unique_ptr<dht_crawler::SmartDHTCrawler> m_smart_crawler;
    std::atomic<bool> m_use_smart_mode;

public:
    DHTTorrentCrawler(const MySQLConfig& config) 
        : m_gen(m_rd()), m_dis(0, 255), m_running(false), m_shutdown_requested(false),
          m_total_queries(0), m_torrents_found(0), m_peers_found(0), m_metadata_fetched(0),
          m_metadata_only_mode(false), m_debug_mode(config.debug_mode), m_verbose_mode(config.verbose_mode), m_metadata_log_mode(config.metadata_log_mode), 
          m_use_concurrent_mode(config.concurrent_mode), m_use_bep51_mode(config.bep51_mode), m_use_smart_mode(true) {  // Enable smart mode by default
        
        m_mysql = std::make_unique<MySQLConnection>(config);
        
        // Initialize logging callback
        m_log_callback = [this](const std::string& message) {
            // In metadata_log_mode, only show metadata-related logs
            if (m_metadata_log_mode) {
                // Check if this is a metadata-related log message
                bool is_metadata_log = (
                    message.find("Metadata") != std::string::npos ||
                    message.find("metadata") != std::string::npos ||
                    message.find("METADATA") != std::string::npos ||
                    message.find("[MetadataManager]") != std::string::npos ||
                    message.find("[MetadataDownloader]") != std::string::npos ||
                    message.find("MetadataWorkerPool") != std::string::npos ||
                    message.find("*** METADATA") != std::string::npos ||
                    message.find("Metadata request") != std::string::npos ||
                    message.find("Metadata success") != std::string::npos ||
                    message.find("Metadata failure") != std::string::npos ||
                    message.find("Metadata timeout") != std::string::npos ||
                    message.find("Queued metadata") != std::string::npos ||
                    message.find("Metadata received") != std::string::npos ||
                    message.find("Extracted comprehensive metadata") != std::string::npos ||
                    message.find("Cleaned up timed out request") != std::string::npos ||
                    (message.find("Worker") != std::string::npos && message.find("metadata") != std::string::npos)
                );
                
                if (is_metadata_log) {
                    std::cout << "[METADATA_LOG] " << message << std::endl;
                }
            } else if (m_debug_mode) {
                std::cout << "[DEBUG] " << message << std::endl;
            }
        };
        
        // Initialize metadata manager
        m_metadata_manager = std::make_unique<dht_crawler::MetadataManager>(m_log_callback);
        
        // Initialize libtorrent session
        lt::session_params params;
        lt::settings_pack& settings = params.settings;
        
        settings.set_bool(lt::settings_pack::enable_dht, true);
        settings.set_int(lt::settings_pack::dht_announce_interval, 15);
        settings.set_int(lt::settings_pack::dht_bootstrap_nodes, 0);
        
        // Configure comprehensive alert mask to catch all connection-related alerts
        settings.set_int(lt::settings_pack::alert_mask, 
            lt::alert_category::dht | 
            lt::alert_category::peer | 
            lt::alert_category::status |
            lt::alert_category::connect |
            lt::alert_category::error);
        
        // Configure port binding for peer connections
        // Use a specific port for port forwarding (default BitTorrent port range)
        settings.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881"); // Listen on all interfaces, port 6881
        
        // Configure connection settings for better metadata fetching
        settings.set_int(lt::settings_pack::handshake_timeout, 10);
        settings.set_int(lt::settings_pack::peer_timeout, 120);
        
        // Enable local peer discovery for better peer connectivity
        settings.set_bool(lt::settings_pack::enable_lsd, true);
        
        // Increase connection limits for metadata fetching
        settings.set_int(lt::settings_pack::connections_limit, 200);
        settings.set_int(lt::settings_pack::active_limit, 1000); // Increased for unlimited metadata requests
        
        // Enable UTP for better connectivity
        settings.set_bool(lt::settings_pack::enable_outgoing_utp, true);
        settings.set_bool(lt::settings_pack::enable_incoming_utp, true);
        
        // Enable UPnP and NAT-PMP for automatic port forwarding
        settings.set_bool(lt::settings_pack::enable_upnp, true);
        settings.set_bool(lt::settings_pack::enable_natpmp, true);
        
        // Configure for better connectivity with port forwarding
        settings.set_bool(lt::settings_pack::enable_dht, true);
        settings.set_int(lt::settings_pack::dht_announce_interval, 15);
        
        // Increase connection limits for better peer connectivity
        settings.set_int(lt::settings_pack::connections_limit, 500); // Higher limit for port forwarding
        settings.set_int(lt::settings_pack::active_limit, 1000); // Much higher limit for unlimited metadata requests
        
        m_session = std::make_unique<lt::session>(params);
        
        // Initialize persistent metadata downloader
        m_metadata_downloader = std::make_unique<dht_crawler::PersistentMetadataDownloader>(
            m_session.get(), m_log_callback);
        
        // Initialize metadata worker pool (10 workers, 20s timeout)
        m_metadata_worker_pool = std::make_unique<MetadataWorkerPool>(m_session.get(), 10, 20, m_log_callback);
        
        // Initialize concurrent DHT manager
        m_concurrent_dht = std::make_unique<ConcurrentDHTManager>(m_session.get(), config.num_workers, 1000);
        
        // Initialize BEP51 DHT indexer
        m_bep51_indexer = std::make_unique<dht_crawler::BEP51DHTIndexer>(m_session.get(), m_log_callback);
        
        // Initialize smart DHT crawler
        m_smart_crawler = std::make_unique<dht_crawler::SmartDHTCrawler>(m_session.get(), m_log_callback);
        
        // Set up concurrent DHT callbacks
        m_concurrent_dht->set_query_callback([this](const DHTQuery& query) {
            // Optional: Track individual queries if needed
            if (m_debug_mode) {
                std::cout << "[DEBUG] DHT query sent: " << query.hash_str.substr(0, 8) << "..." << std::endl;
            }
        });
        
        m_concurrent_dht->set_progress_callback([this]() {
            // Periodic progress reporting
            if (m_debug_mode) {
                m_concurrent_dht->print_statistics();
            }
        });
        
        // Persistent metadata downloader is configured with defaults
    }

    bool initialize() {
        try {
            std::cout << "Initializing DHT Torrent Crawler..." << std::endl;
            
            // Connect to MySQL (skip for testing)
            if (!m_mysql->connect()) {
                std::cout << "MySQL connection failed - running in test mode without database storage" << std::endl;
                m_mysql->logError("DHTTorrentCrawler::initialize", "", -1, "MySQL connection failed", "", "WARNING", "Running in test mode");
                // Continue without MySQL for testing
            }
        
        // Check port forwarding status
        std::cout << "Checking port forwarding status..." << std::endl;
        std::cout << "Configured to listen on: 0.0.0.0:6881" << std::endl;
        
        // Wait for DHT bootstrap
        std::cout << "Waiting for DHT bootstrap..." << std::endl;
        bool dht_ready = false;
        int bootstrap_wait = 0;
        
        while (!dht_ready && bootstrap_wait < 30) {
            std::vector<lt::alert*> alerts;
            m_session->pop_alerts(&alerts);
            
            for (lt::alert* alert : alerts) {
                if (alert->type() == lt::dht_bootstrap_alert::alert_type) {
                    dht_ready = true;
                    std::cout << "*** DHT Bootstrap completed! ***" << std::endl;
                    break;
                }
            }
            
            if (!dht_ready) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                bootstrap_wait++;
                std::cout << "Bootstrap wait: " << bootstrap_wait << "s" << std::endl;
            }
        }
        
        if (!dht_ready) {
            std::cout << "DHT bootstrap timeout, continuing anyway..." << std::endl;
        }
        
        // Check if we're in metadata-only mode
        if (!m_mysql->getConfig().metadata_hashes.empty()) {
            setupMetadataOnlyMode();
        }
        
        return true;
        } catch (const std::exception& e) {
            m_mysql->logException("DHTTorrentCrawler::initialize", "", e);
            return false;
        }
    }
    
    void setupMetadataOnlyMode() {
        std::cout << "\n=== METADATA-ONLY MODE ===" << std::endl;
        m_metadata_only_mode = true;
        
        // Check if input is a filename (contains .txt or .csv)
        std::string hashes_str = m_mysql->getConfig().metadata_hashes;
        
        if (hashes_str.find(".txt") != std::string::npos || hashes_str.find(".csv") != std::string::npos) {
            // Read hashes from file
            std::ifstream file(hashes_str);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open file: " << hashes_str << std::endl;
                return;
            }
            
            std::string line;
            while (std::getline(file, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                
                if (!line.empty()) {
                    m_metadata_hash_list.push_back(line);
                    std::cout << "Added hash for metadata fetch: " << line << std::endl;
                }
            }
            file.close();
        } else {
            // Parse comma-delimited hashes
            std::istringstream ss(hashes_str);
            std::string hash;
            
            while (std::getline(ss, hash, ',')) {
                // Trim whitespace
                hash.erase(0, hash.find_first_not_of(" \t\r\n"));
                hash.erase(hash.find_last_not_of(" \t\r\n") + 1);
                
                if (!hash.empty()) {
                    m_metadata_hash_list.push_back(hash);
                    std::cout << "Added hash for metadata fetch: " << hash << std::endl;
                }
            }
        }
        
        std::cout << "Total hashes to fetch metadata for: " << m_metadata_hash_list.size() << std::endl;
        std::cout << "Starting metadata-only mode..." << std::endl;
    }

    void startCrawling(int max_queries = -1) {
        if (!m_mysql->isConnected()) {
            std::cout << "MySQL not connected, running in test mode without database storage" << std::endl;
        }
        
        m_running = true;
        
        if (m_metadata_only_mode) {
            if (m_verbose_mode) {
                std::cout << "\n=== Starting Metadata-Only Mode ===" << std::endl;
                std::cout << "Fetching metadata for " << m_metadata_hash_list.size() << " torrents..." << std::endl;
            }
            
            // Request metadata for all specified hashes
            for (const std::string& hash : m_metadata_hash_list) {
                requestMetadataForHash(hash);
            }
            
            // Wait for metadata to arrive
            auto start_time = std::chrono::steady_clock::now();
            int timeout_seconds = 300; // 5 minutes timeout
            
            while (m_running && !m_shutdown_requested) {
                processAlerts();
                
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                
                if (m_debug_mode && elapsed_seconds % 10 == 0) {
                    std::cout << "[DEBUG] Metadata wait: " << elapsed_seconds << "s, fetched: " << m_metadata_fetched << "/" << m_metadata_hash_list.size() << std::endl;
                }
                
                if (elapsed_seconds >= timeout_seconds) {
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
        
        if (m_verbose_mode) {
            std::cout << "\n=== Starting DHT Torrent Discovery ===" << std::endl;
            if (max_queries > 0) {
                std::cout << "Max queries: " << max_queries << std::endl;
            } else {
                std::cout << "Running indefinitely (press Ctrl+C to stop)" << std::endl;
            }
        }
        
        auto start_time = std::chrono::steady_clock::now();
        int progress_counter = 0;
        
        if (m_use_concurrent_mode) {
            if (m_verbose_mode) {
                std::cout << "Using concurrent DHT worker pool (4 workers)" << std::endl;
            }
            
            // Start concurrent DHT manager
            m_concurrent_dht->start();
            
            // Main processing loop - now focuses on alert processing and metadata management
            while (m_running && !m_shutdown_requested && 
                   (max_queries < 0 || m_concurrent_dht->get_total_queries_sent() < max_queries)) {
                
                // Process alerts from libtorrent (this is the main bottleneck now)
                processAlerts();
                
                // Request metadata for some discovered torrents (every 5 iterations for faster processing)
                if (progress_counter % 5 == 0) {
                    requestMetadataForDiscoveredTorrents();
                }
                
                // Use BEP51 to get real infohashes (every 30 iterations)
                if (progress_counter % 30 == 0) {
                    queryBEP51Infohashes();
                }
                
                // Use smart crawling for better infohash discovery (every 20 iterations)
                if (progress_counter % 20 == 0 && m_smart_crawler && m_use_smart_mode) {
                    m_smart_crawler->send_smart_queries();
                }
                
                // Clean up timed out metadata requests (every 3 iterations for frequent cleanup)
                if (progress_counter % 3 == 0) {
                    m_metadata_worker_pool->cleanup_timeouts();
                    // Mark timed out torrents in database
                    auto timed_out_requests = m_metadata_worker_pool->get_timed_out_requests();
                    for (const auto& hash : timed_out_requests) {
                        m_mysql->markTorrentTimedOut(hash);
                        
                        // Enhanced timeout logging for metadata_log_mode
                        if (m_metadata_log_mode) {
                            std::cout << "[METADATA_LOG] *** METADATA TIMEOUT ***" << std::endl;
                            std::cout << "[METADATA_LOG] Hash: " << hash << std::endl;
                            std::cout << "[METADATA_LOG] Reason: Request timed out after 20 seconds" << std::endl;
                            std::cout << "[METADATA_LOG] Status: No metadata received from peers" << std::endl;
                            std::cout << "[METADATA_LOG] ------------------------" << std::endl;
                        }
                    }
                }
                
                // Adjust concurrent limit dynamically (every 50 iterations)
                if (progress_counter % 50 == 0) {
                    m_metadata_downloader->adjust_concurrent_limit();
                }
                
                progress_counter++;
                
                // Progress update every 100 iterations
                if (progress_counter % 100 == 0) {
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                    
                    if (m_verbose_mode) {
                        std::cout << "\n*** PROGRESS UPDATE (CONCURRENT MODE) ***" << std::endl;
                        std::cout << "Queries sent: " << m_concurrent_dht->get_total_queries_sent() << std::endl;
                        std::cout << "Queries generated: " << m_concurrent_dht->get_queries_generated() << std::endl;
                        std::cout << "Active workers: " << m_concurrent_dht->get_active_workers() << std::endl;
                        std::cout << "Torrents found: " << m_torrents_found << std::endl;
                        std::cout << "Peers found: " << m_peers_found << std::endl;
                        std::cout << "Metadata fetched: " << m_metadata_fetched << std::endl;
                    std::cout << "Metadata queue: " << m_metadata_worker_pool->get_queue_size() << " pending (WORKER POOL)" << std::endl;
                    std::cout << "Active metadata requests: " << m_metadata_worker_pool->get_active_requests() << " (10 workers)" << std::endl;
                        std::cout << "Elapsed time: " << elapsed_seconds << " seconds" << std::endl;
                        std::cout << "Rate: " << (m_concurrent_dht->get_total_queries_sent() / (elapsed_seconds + 1)) << " queries/sec" << std::endl;
                        
                        // Port forwarding status
                        std::cout << "Listening on: 0.0.0.0:6881 (configured)" << std::endl;
                        
                        // Print concurrent DHT statistics
                        m_concurrent_dht->print_statistics();
                    } else {
                        // Simple counter display
                        std::cout << "\rHashes found - " << m_torrents_found << std::flush;
                    }
                }
                
                // Reduced delay since workers are handling query generation
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            // Stop concurrent DHT manager
            m_concurrent_dht->stop();
            
        } else {
            // Fallback to original sequential mode
            if (m_verbose_mode) {
                std::cout << "Using sequential DHT mode (legacy)" << std::endl;
            }
            
            int query_count = 0;
            while (m_running && !m_shutdown_requested && (max_queries < 0 || query_count < max_queries)) {
                // Generate random info hash
                lt::sha1_hash random_hash;
                for (int i = 0; i < 20; ++i) {
                    random_hash[i] = static_cast<unsigned char>(m_dis(m_gen));
                }
                
                std::string hash_str = random_hash.to_string();
                
                // Skip if we've already queried this hash
                if (m_queried_hashes.find(hash_str) != m_queried_hashes.end()) {
                    continue;
                }
                m_queried_hashes.insert(hash_str);
                
                if (query_count % 50 == 0) {
                    std::cout << "\n--- Query " << (query_count + 1) << " ---" << std::endl;
                    std::cout << "Hash: " << hash_str << std::endl;
                }
                
                // Query DHT for peers
                try {
                    m_session->dht_get_peers(random_hash);
                    m_session->dht_get_item(random_hash);
                    m_total_queries += 2;
                } catch (const std::exception& e) {
                    std::cerr << "Error sending DHT queries: " << e.what() << std::endl;
                }
                
                // Process alerts
                processAlerts();
                
                // Request metadata for some discovered torrents (every 10 queries for faster processing)
                if (query_count % 10 == 0) {
                    requestMetadataForDiscoveredTorrents();
                }
                
                // Clean up timed out metadata requests (every 5 queries for frequent cleanup)
                if (query_count % 5 == 0) {
                    m_metadata_worker_pool->cleanup_timeouts();
                    // Mark timed out torrents in database
                    auto timed_out_requests = m_metadata_worker_pool->get_timed_out_requests();
                    for (const auto& hash : timed_out_requests) {
                        m_mysql->markTorrentTimedOut(hash);
                        
                        // Enhanced timeout logging for metadata_log_mode
                        if (m_metadata_log_mode) {
                            std::cout << "[METADATA_LOG] *** METADATA TIMEOUT ***" << std::endl;
                            std::cout << "[METADATA_LOG] Hash: " << hash << std::endl;
                            std::cout << "[METADATA_LOG] Reason: Request timed out after 20 seconds" << std::endl;
                            std::cout << "[METADATA_LOG] Status: No metadata received from peers" << std::endl;
                            std::cout << "[METADATA_LOG] ------------------------" << std::endl;
                        }
                    }
                }
                
                // Adjust concurrent limit dynamically (every 25 queries)
                if (query_count % 25 == 0) {
                    m_metadata_downloader->adjust_concurrent_limit();
                }
                
                query_count++;
                
                // Progress update every 100 queries
                if (query_count % 100 == 0) {
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                    
                    if (m_verbose_mode) {
                        std::cout << "\n*** PROGRESS UPDATE (SEQUENTIAL MODE) ***" << std::endl;
                        std::cout << "Queries sent: " << m_total_queries << std::endl;
                        std::cout << "Torrents found: " << m_torrents_found << std::endl;
                        std::cout << "Peers found: " << m_peers_found << std::endl;
                        std::cout << "Metadata fetched: " << m_metadata_fetched << std::endl;
                    std::cout << "Metadata queue: " << m_metadata_worker_pool->get_queue_size() << " pending (WORKER POOL)" << std::endl;
                    std::cout << "Active metadata requests: " << m_metadata_worker_pool->get_active_requests() << " (10 workers)" << std::endl;
                        std::cout << "Elapsed time: " << elapsed_seconds << " seconds" << std::endl;
                        std::cout << "Rate: " << (m_total_queries / (elapsed_seconds + 1)) << " queries/sec" << std::endl;
                    } else {
                        // Simple counter display
                        std::cout << "\rHashes found - " << m_torrents_found << std::flush;
                    }
                }
                
                // Small delay between queries
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        m_running = false;
        
        // Final summary
        auto total_elapsed = std::chrono::steady_clock::now() - start_time;
        auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(total_elapsed).count();
        
        if (m_verbose_mode) {
            std::cout << "\n=== CRAWLING COMPLETE ===" << std::endl;
            if (m_use_concurrent_mode) {
                std::cout << "Mode: Concurrent DHT Worker Pool" << std::endl;
                std::cout << "Total queries sent: " << m_concurrent_dht->get_total_queries_sent() << std::endl;
                std::cout << "Total queries generated: " << m_concurrent_dht->get_queries_generated() << std::endl;
                std::cout << "Active workers: " << m_concurrent_dht->get_active_workers() << std::endl;
            } else {
                std::cout << "Mode: Sequential DHT" << std::endl;
                std::cout << "Total queries sent: " << m_total_queries << std::endl;
            }
            std::cout << "Total torrents found: " << m_torrents_found << std::endl;
            std::cout << "Total peers found: " << m_peers_found << std::endl;
            std::cout << "Total metadata fetched: " << m_metadata_fetched << std::endl;
            std::cout << "Metadata queue size: " << m_metadata_downloader->get_queue_size() << std::endl;
            std::cout << "Total metadata queued: " << m_metadata_downloader->get_total_queued() << std::endl;
            std::cout << "Total metadata processed: " << m_metadata_downloader->get_total_processed() << std::endl;
            std::cout << "Total elapsed time: " << total_seconds << " seconds" << std::endl;
            
            int total_queries = m_use_concurrent_mode ? m_concurrent_dht->get_total_queries_sent() : m_total_queries;
            std::cout << "Average rate: " << (total_queries / (total_seconds + 1)) << " queries/sec" << std::endl;
        } else {
            // Simple final counter
            std::cout << "\nHashes found - " << m_torrents_found << std::endl;
        }
    }

    void stop() {
        std::cout << "\n*** SHUTDOWN REQUESTED ***" << std::endl;
        m_shutdown_requested = true;
        m_running = false;
    }
    
    void setConcurrentMode(bool enabled) {
        m_use_concurrent_mode = enabled;
        std::cout << "Concurrent DHT mode: " << (enabled ? "ENABLED" : "DISABLED") << std::endl;
    }
    
    bool isConcurrentMode() const {
        return m_use_concurrent_mode.load();
    }
    
    void setBEP51Mode(bool enabled) {
        m_use_bep51_mode = enabled;
        std::cout << "BEP51 DHT indexing mode: " << (enabled ? "ENABLED" : "DISABLED") << std::endl;
    }
    
    bool isBEP51Mode() const {
        return m_use_bep51_mode.load();
    }
    
    // Use BEP51 to get real infohashes instead of random generation
    void queryBEP51Infohashes() {
        if (!m_bep51_indexer || !m_use_bep51_mode) {
            return;
        }
        
        // Query known DHT nodes for infohashes
        auto known_nodes = m_bep51_indexer->get_known_nodes();
        if (known_nodes.empty()) {
            // If no known nodes, use some well-known DHT bootstrap nodes
            std::vector<std::string> bootstrap_nodes = {
                "router.bittorrent.com",
                "dht.transmissionbt.com", 
                "router.utorrent.com"
            };
            
            for (const auto& node : bootstrap_nodes) {
                // Convert to node ID (simplified - in practice you'd resolve these)
                std::string node_id(20, 0);
                for (int i = 0; i < 20; ++i) {
                    node_id[i] = static_cast<char>(rand() % 256);
                }
                m_bep51_indexer->query_sample_infohashes(node_id);
                (void)node; // Suppress unused variable warning
            }
        } else {
            // Query up to 10 known nodes
            int queries = 0;
            for (const auto& node_id : known_nodes) {
                if (queries >= 10) break;
                m_bep51_indexer->query_sample_infohashes(node_id);
                queries++;
            }
        }
        
        // Process collected infohashes for metadata fetching
        auto infohashes = m_bep51_indexer->get_collected_infohashes();
        for (const auto& infohash : infohashes) {
            // Convert binary infohash to hex string
            std::string hex_hash;
            for (unsigned char c : infohash) {
                char hex[3];
                snprintf(hex, sizeof(hex), "%02x", c);
                hex_hash += hex;
            }
            
            // Queue for metadata fetching
            if (m_metadata_requested.find(hex_hash) == m_metadata_requested.end()) {
                if (m_metadata_downloader->request_metadata(hex_hash, 5, "BEP51")) { // Highest priority
                    m_metadata_requested.insert(hex_hash);
                    std::cout << "BEP51: Queued metadata request for: " << hex_hash << std::endl;
                }
            }
        }
        
        if (m_debug_mode) {
            m_bep51_indexer->print_statistics();
        }
    }
    
    void gracefulShutdown() {
        std::cout << "\n*** GRACEFUL SHUTDOWN INITIATED ***" << std::endl;
        std::cout << "Saving final statistics..." << std::endl;
        
        // Save final statistics
        std::cout << "Final Statistics:" << std::endl;
        std::cout << "- Total queries sent: " << m_total_queries << std::endl;
        std::cout << "- Total torrents found: " << m_torrents_found << std::endl;
        std::cout << "- Total peers found: " << m_peers_found << std::endl;
        std::cout << "- Total metadata fetched: " << m_metadata_fetched << std::endl;
        
        // Print enhanced metadata statistics
        if (m_metadata_manager) {
            m_metadata_manager->print_statistics();
        }
        
        // Print metadata worker pool status
        if (m_metadata_worker_pool) {
            int total_queued, total_processed, total_successful, total_failed, total_timeout;
            m_metadata_worker_pool->get_stats(total_queued, total_processed, total_successful, total_failed, total_timeout);
            std::cout << "Metadata Worker Pool Statistics:" << std::endl;
            std::cout << "- Total queued: " << total_queued << std::endl;
            std::cout << "- Total processed: " << total_processed << std::endl;
            std::cout << "- Successful: " << total_successful << std::endl;
            std::cout << "- Failed: " << total_failed << std::endl;
            std::cout << "- Timeouts: " << total_timeout << std::endl;
            std::cout << "- Active requests: " << m_metadata_worker_pool->get_active_requests() << std::endl;
            std::cout << "- Queue size: " << m_metadata_worker_pool->get_queue_size() << std::endl;
        }
        
        // Clean up metadata worker pool
        if (m_metadata_worker_pool) {
            std::cout << "Shutting down metadata worker pool..." << std::endl;
            m_metadata_worker_pool->shutdown();
            m_metadata_worker_pool.reset();
        }
        
        std::cout << "*** SHUTDOWN COMPLETE ***" << std::endl;
    }

private:
    void processAlerts() {
        std::vector<lt::alert*> alerts;
        m_session->pop_alerts(&alerts);
        
        if (m_debug_mode && !alerts.empty()) {
            std::cout << "[DEBUG] Processing " << alerts.size() << " alerts" << std::endl;
        }
        
        for (lt::alert* alert : alerts) {
            if (m_debug_mode) {
                std::cout << "[DEBUG] Alert type: " << alert->type() << " - " << alert->message() << std::endl;
            }
            // Handle DHT get peers reply
            if (alert->type() == lt::dht_get_peers_reply_alert::alert_type) {
                auto* reply_alert = lt::alert_cast<lt::dht_get_peers_reply_alert>(alert);
                if (reply_alert && !reply_alert->peers().empty()) {
                    handlePeerReply(reply_alert);
                }
            }
            
            // Handle DHT announce
            if (alert->type() == lt::dht_announce_alert::alert_type) {
                auto* announce_alert = lt::alert_cast<lt::dht_announce_alert>(alert);
                if (announce_alert) {
                    handleAnnounce(announce_alert);
                }
            }
            
            // Handle DHT immutable item
            if (alert->type() == lt::dht_immutable_item_alert::alert_type) {
                auto* item_alert = lt::alert_cast<lt::dht_immutable_item_alert>(alert);
                if (item_alert) {
                    handleImmutableItem(item_alert);
                }
            }
            
            // Handle peer connect alerts
            if (alert->type() == lt::peer_connect_alert::alert_type) {
                auto* peer_alert = lt::alert_cast<lt::peer_connect_alert>(alert);
                if (peer_alert) {
                    std::cout << "[DEBUG] *** PEER CONNECTED *** " << peer_alert->endpoint << std::endl;
                    if (m_debug_mode) {
                        std::cout << "[DEBUG] Peer connection details: " << peer_alert->message() << std::endl;
                    }
                }
            }
            
            // Handle peer disconnect alerts
            if (alert->type() == lt::peer_disconnected_alert::alert_type) {
                auto* peer_alert = lt::alert_cast<lt::peer_disconnected_alert>(alert);
                if (peer_alert) {
                    std::cout << "[DEBUG] *** PEER DISCONNECTED *** " << peer_alert->endpoint << " - " << peer_alert->message() << std::endl;
                }
            }
            
            // Handle connection errors
            if (alert->type() == lt::peer_error_alert::alert_type) {
                auto* error_alert = lt::alert_cast<lt::peer_error_alert>(alert);
                if (error_alert) {
                    std::cout << "[DEBUG] *** PEER ERROR *** " << error_alert->endpoint << " - " << error_alert->message() << std::endl;
                }
            }
            
            // Handle torrent connection alerts
            if (alert->type() == lt::add_torrent_alert::alert_type) {
                auto* torrent_alert = lt::alert_cast<lt::add_torrent_alert>(alert);
                if (torrent_alert) {
                    std::cout << "[DEBUG] *** TORRENT ADDED *** " << torrent_alert->message() << std::endl;
                }
            }
            
            // Handle state change alerts
            if (alert->type() == lt::state_changed_alert::alert_type) {
                auto* state_alert = lt::alert_cast<lt::state_changed_alert>(alert);
                if (state_alert && m_debug_mode) {
                    std::cout << "[DEBUG] State changed: " << state_alert->message() << std::endl;
                }
            }
            
            // Handle metadata received
            if (alert->type() == lt::metadata_received_alert::alert_type) {
                auto* metadata_alert = lt::alert_cast<lt::metadata_received_alert>(alert);
                if (metadata_alert) {
                    handleMetadataReceived(metadata_alert);
                }
            }
        }
    }
    
    void handlePeerReply(lt::dht_get_peers_reply_alert* alert) {
        std::string hash_binary = alert->info_hash.to_string();
        std::string hash_str;
        for (unsigned char c : hash_binary) {
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", c);
            hash_str += hex;
        }
        
        DiscoveredTorrent torrent;
        torrent.info_hash = hash_str;
        torrent.name = "Unknown Torrent";
        torrent.size = 0;
        torrent.num_files = 0;
        torrent.source = "DHT_PEERS";
        torrent.metadata_received = false;
        torrent.timed_out = false;
        torrent.discovered_time = std::chrono::steady_clock::now();
        torrent.last_seen_time = std::chrono::steady_clock::now();
        
        // Initialize all metadata fields
        torrent.comment = "";
        torrent.created_by = "";
        torrent.creation_date = 0;
        torrent.encoding = "";
        torrent.piece_length = 0;
        torrent.num_pieces = 0;
        torrent.private_torrent = false;
        torrent.magnet_link = "magnet:?xt=urn:btih:" + hash_str;
        torrent.announce_url = "";
        torrent.content_type = "";
        torrent.language = "";
        torrent.category = "";
        torrent.seeders_count = 0;
        torrent.leechers_count = 0;
        torrent.download_speed = 0;
        
        // Store peer information
        for (const auto& peer : alert->peers()) {
            std::string peer_str = peer.address().to_string() + ":" + std::to_string(peer.port());
            torrent.peers.push_back(peer_str);
            m_peers_found++;
        }
        
        // Record observation for smart crawling
        if (m_smart_crawler && m_use_smart_mode) {
            m_smart_crawler->record_incoming_observation(hash_str, "peer_reply", 
                                                       torrent.peers.size(), torrent.peers);
        }
        
        // Store in memory and database
        m_discovered_torrents[hash_str] = torrent;
        if (m_mysql->isConnected() && m_mysql->storeTorrent(torrent)) {
            m_torrents_found++;
            std::cout << "Stored torrent with " << torrent.peers.size() << " peers: " << hash_str << std::endl;
        } else {
            m_torrents_found++;
            std::cout << "Found torrent with " << torrent.peers.size() << " peers: " << hash_str << " (test mode)" << std::endl;
        }
        
        // Automatically queue for metadata fetching if we haven't already requested it
        if (m_metadata_requested.find(hash_str) == m_metadata_requested.end()) {
            if (m_metadata_downloader->request_metadata(hash_str, 3, "DHT_PEERS")) { // High priority for peer-discovered torrents
                m_metadata_requested.insert(hash_str);
                std::cout << "Auto-queued metadata request for: " << hash_str << std::endl;
            } else {
                std::cout << "Failed to auto-queue metadata for: " << hash_str << std::endl;
                if (m_debug_mode) {
                    std::cout << "[DEBUG] Active metadata requests: " << m_metadata_downloader->get_active_requests() << std::endl;
                    std::cout << "[DEBUG] Available slots: " << m_metadata_downloader->get_available_slots() << std::endl;
                    m_metadata_downloader->print_status();
                }
            }
        } else {
            if (m_debug_mode) {
                std::cout << "[DEBUG] Metadata already requested for peer torrent: " << hash_str << std::endl;
            }
        }
    }
    
    void handleAnnounce(lt::dht_announce_alert* alert) {
        std::string hash_binary = alert->info_hash.to_string();
        std::string hash_str;
        for (unsigned char c : hash_binary) {
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", c);
            hash_str += hex;
        }
        
        DiscoveredTorrent torrent;
        torrent.info_hash = hash_str;
        torrent.name = "Announced Torrent";
        torrent.size = 0;
        torrent.num_files = 0;
        torrent.source = "DHT_ANNOUNCE";
        torrent.metadata_received = false;
        torrent.timed_out = false;
        torrent.discovered_time = std::chrono::steady_clock::now();
        torrent.last_seen_time = std::chrono::steady_clock::now();
        
        // Initialize all metadata fields
        torrent.comment = "";
        torrent.created_by = "";
        torrent.creation_date = 0;
        torrent.encoding = "";
        torrent.piece_length = 0;
        torrent.num_pieces = 0;
        torrent.private_torrent = false;
        torrent.magnet_link = "magnet:?xt=urn:btih:" + hash_str;
        torrent.announce_url = "";
        torrent.content_type = "";
        torrent.language = "";
        torrent.category = "";
        torrent.seeders_count = 0;
        torrent.leechers_count = 0;
        torrent.download_speed = 0;
        
        // Store in memory and database
        m_discovered_torrents[hash_str] = torrent;
        if (m_mysql->isConnected() && m_mysql->storeTorrent(torrent)) {
            m_torrents_found++;
            std::cout << "Stored announced torrent: " << hash_str << std::endl;
        } else {
            m_torrents_found++;
            std::cout << "Found announced torrent: " << hash_str << " (test mode)" << std::endl;
        }
        
        // Automatically queue for metadata fetching if we haven't already requested it
        if (m_metadata_requested.find(hash_str) == m_metadata_requested.end()) {
            if (m_metadata_downloader->request_metadata(hash_str, 2, "DHT_ANNOUNCE")) { // Medium priority for announced torrents
                m_metadata_requested.insert(hash_str);
                std::cout << "Auto-queued metadata request for announced torrent: " << hash_str << std::endl;
            } else {
                std::cout << "Failed to auto-queue metadata for announced torrent: " << hash_str << std::endl;
                if (m_debug_mode) {
                    std::cout << "[DEBUG] Active metadata requests: " << m_metadata_downloader->get_active_requests() << std::endl;
                    std::cout << "[DEBUG] Available slots: " << m_metadata_downloader->get_available_slots() << std::endl;
                    m_metadata_downloader->print_status();
                }
            }
        } else {
            if (m_debug_mode) {
                std::cout << "[DEBUG] Metadata already requested for announced torrent: " << hash_str << std::endl;
            }
        }
    }
    
    void handleImmutableItem(lt::dht_immutable_item_alert* alert) {
        std::string hash_binary = alert->target.to_string();
        std::string hash_str;
        for (unsigned char c : hash_binary) {
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", c);
            hash_str += hex;
        }
        
        DiscoveredTorrent torrent;
        torrent.info_hash = hash_str;
        torrent.name = "DHT Item";
        torrent.size = 0;
        torrent.num_files = 0;
        torrent.source = "DHT_ITEM";
        torrent.metadata_received = false;
        torrent.timed_out = false;
        torrent.discovered_time = std::chrono::steady_clock::now();
        torrent.last_seen_time = std::chrono::steady_clock::now();
        
        // Initialize all metadata fields
        torrent.comment = "";
        torrent.created_by = "";
        torrent.creation_date = 0;
        torrent.encoding = "";
        torrent.piece_length = 0;
        torrent.num_pieces = 0;
        torrent.private_torrent = false;
        torrent.magnet_link = "magnet:?xt=urn:btih:" + hash_str;
        torrent.announce_url = "";
        torrent.content_type = "";
        torrent.language = "";
        torrent.category = "";
        torrent.seeders_count = 0;
        torrent.leechers_count = 0;
        torrent.download_speed = 0;
        
        // Store in memory and database
        m_discovered_torrents[hash_str] = torrent;
        if (m_mysql->isConnected() && m_mysql->storeTorrent(torrent)) {
            m_torrents_found++;
            std::cout << "Stored DHT item: " << hash_str << std::endl;
        } else {
            m_torrents_found++;
            std::cout << "Found DHT item: " << hash_str << " (test mode)" << std::endl;
        }
        
        // Automatically queue for metadata fetching if we haven't already requested it
        if (m_metadata_requested.find(hash_str) == m_metadata_requested.end()) {
            if (m_metadata_downloader->request_metadata(hash_str, 1, "DHT_ITEM")) { // Lower priority for DHT items
                m_metadata_requested.insert(hash_str);
                std::cout << "Auto-queued metadata request for DHT item: " << hash_str << std::endl;
            } else {
                std::cout << "Failed to auto-queue metadata for DHT item: " << hash_str << std::endl;
                if (m_debug_mode) {
                    std::cout << "[DEBUG] Active metadata requests: " << m_metadata_downloader->get_active_requests() << std::endl;
                    std::cout << "[DEBUG] Available slots: " << m_metadata_downloader->get_available_slots() << std::endl;
                    m_metadata_downloader->print_status();
                }
            }
        } else {
            if (m_debug_mode) {
                std::cout << "[DEBUG] Metadata already requested for DHT item: " << hash_str << std::endl;
            }
        }
    }
    
    void requestMetadataForHash(const std::string& hash) {
        if (m_debug_mode) {
            std::cout << "[DEBUG] Requesting metadata for hash: " << hash << std::endl;
        }
        
        // Log metadata request
        m_metadata_manager->log_metadata_request(hash);
        
        // Use enhanced metadata downloader
        if (m_metadata_downloader->request_metadata(hash, 4, "MANUAL")) { // Highest priority for manual requests
            m_metadata_requested.insert(hash);
            std::cout << "Requesting metadata for hash: " << hash << std::endl;
            
            if (m_debug_mode) {
                std::cout << "[DEBUG] Metadata request added successfully" << std::endl;
                std::cout << "[DEBUG] Active metadata requests: " << m_metadata_downloader->get_active_requests() << std::endl;
            }
        } else {
            std::cerr << "Failed to request metadata for " << hash << std::endl;
            m_metadata_manager->log_metadata_failure(hash, "Failed to add torrent to session");
            m_metadata_downloader->log_failure();
            // Mark as requested even if failed to avoid repeated attempts
            m_metadata_requested.insert(hash);
        }
    }
    
    void requestMetadataForDiscoveredTorrents() {
        // Only show debug output every 20 iterations to reduce spam
        static int debug_counter = 0;
        debug_counter++;
        
        // Show debug output less frequently when not in debug mode
        int debug_interval = m_debug_mode ? 5 : 10;
        
        // Always show a simple debug message to verify function is called
        if (debug_counter % debug_interval == 0) {
            // Get current time for timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);
            
            // Format timestamp
            char timestamp[32];
            std::strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tm);
            
            // Single line debug output with timestamp
            std::cout << "[" << timestamp << "] METADATA: torrents=" << m_discovered_torrents.size() 
                      << " requested=" << m_metadata_requested.size() 
                      << " active=" << m_metadata_worker_pool->get_active_requests()
                      << " queue=" << m_metadata_worker_pool->get_queue_size() << std::endl;
        }
        
        // Request metadata for up to 20 discovered torrents that don't have metadata yet
        int requested = 0;
        for (auto& pair : m_discovered_torrents) {
            if (requested >= 20) break; // Increased batch size for worker pool
            
            DiscoveredTorrent& torrent = pair.second;
            if (!torrent.metadata_received && m_metadata_requested.find(torrent.info_hash) == m_metadata_requested.end()) {
                // Use worker pool for metadata requests with priority based on source
                int priority = 1; // Default priority
                if (torrent.source == "DHT_PEERS") priority = 3; // High priority for peer-discovered
                else if (torrent.source == "DHT_ANNOUNCE") priority = 2; // Medium priority for announced
                
                if (m_metadata_worker_pool->queue_request(torrent.info_hash, priority, torrent.source)) {
                    m_metadata_requested.insert(torrent.info_hash);
                    requested++;
                    std::cout << "Queued metadata request for: " << torrent.info_hash << " (priority: " << priority << ")" << std::endl;
                } else {
                    std::cerr << "Failed to queue metadata request for " << torrent.info_hash << std::endl;
                    m_metadata_manager->log_metadata_failure(torrent.info_hash, "Failed to queue request");
                    // Mark as requested even if failed to avoid repeated attempts
                    m_metadata_requested.insert(torrent.info_hash);
                }
            }
        }
        
        // Only show detailed debug output every debug_interval iterations
        if (debug_counter % debug_interval == 0) {
            // Get current time for timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);
            
            // Format timestamp
            char timestamp[32];
            std::strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tm);
            
            std::cout << "[" << timestamp << "] METADATA: queued=" << requested 
                      << " worker_queue=" << m_metadata_worker_pool->get_queue_size() 
                      << " worker_active=" << m_metadata_worker_pool->get_active_requests() << std::endl;
        }
    }
    
    void handleMetadataReceived(lt::metadata_received_alert* alert) {
        try {
            auto torrent_info = alert->handle.torrent_file();
            if (!torrent_info) {
                std::cout << "Metadata alert received but torrent_info is null" << std::endl;
                m_mysql->logError("DHTTorrentCrawler::handleMetadataReceived", "", -1, "torrent_info is null", "", "WARNING");
                return;
            }
            
            std::cout << "*** METADATA RECEIVED ***" << std::endl;
            
            // Get info hash
            auto info_hash = alert->handle.info_hash();
            std::string hash_str;
            // Convert info_hash_t to hex string
            for (int i = 0; i < 20; ++i) {
                char hex[3];
                snprintf(hex, sizeof(hex), "%02x", static_cast<unsigned char>(info_hash[i]));
                hash_str += hex;
            }
            
            // Log successful metadata reception
            m_metadata_manager->log_metadata_success(hash_str, torrent_info->total_size());
            
            // Notify enhanced metadata downloader
            m_metadata_downloader->handle_metadata_received(hash_str);
            m_metadata_downloader->log_success();
            
            // Notify metadata worker pool
            m_metadata_worker_pool->handle_metadata_received(hash_str);
            
            // Extract comprehensive metadata using enhanced extraction (inspired by dump_torrent example)
            auto enhanced_metadata = m_metadata_downloader->extract_comprehensive_metadata(*torrent_info, hash_str);
            
            // Update torrent information
            DiscoveredTorrent torrent;
            
            if (m_metadata_only_mode) {
                // In metadata-only mode, create a new torrent record
                torrent.info_hash = hash_str;
                torrent.source = "METADATA_ONLY";
                torrent.metadata_received = false;
                torrent.timed_out = false;
                torrent.discovered_time = std::chrono::steady_clock::now();
                torrent.last_seen_time = std::chrono::steady_clock::now();
            } else {
                // In normal mode, find existing torrent
                auto it = m_discovered_torrents.find(hash_str);
                if (it != m_discovered_torrents.end()) {
                    torrent = it->second;
                } else {
                    std::cout << "Warning: Received metadata for unknown torrent: " << hash_str << std::endl;
                    return;
                }
            }
            
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
            torrent.metadata_received = true;
            torrent.last_seen_time = std::chrono::steady_clock::now();
            
            // Enhanced file information with flags and offsets
            torrent.file_names = enhanced_metadata.file_names;
            torrent.file_sizes = enhanced_metadata.file_sizes;
            
            // Enhanced tracker information with tiers
            torrent.trackers = enhanced_metadata.trackers;
            torrent.announce_list = enhanced_metadata.trackers; // Use trackers as announce_list
            
            // Set announce URL from first tracker
            if (!enhanced_metadata.trackers.empty()) {
                torrent.announce_url = enhanced_metadata.trackers[0];
            }
            
            // Log enhanced metadata extraction
            std::cout << "Enhanced metadata extracted: " << enhanced_metadata.num_files << " files, " 
                      << enhanced_metadata.trackers.size() << " trackers, " 
                      << enhanced_metadata.web_seeds.size() << " web seeds" << std::endl;
                
                // Determine content type based on file extensions
                torrent.content_type = determineContentType(torrent.file_names);
                
                // Update in database
                if (m_mysql->isConnected() && m_mysql->storeTorrent(torrent)) {
                    m_metadata_fetched++;
                    
                    // Enhanced metadata logging for metadata_log_mode
                    if (m_metadata_log_mode) {
                        std::cout << "[METADATA_LOG] *** METADATA DISCOVERED ***" << std::endl;
                        std::cout << "[METADATA_LOG] Hash: " << hash_str << std::endl;
                        std::cout << "[METADATA_LOG] Name: " << torrent.name << std::endl;
                        std::cout << "[METADATA_LOG] Size: " << torrent.size << " bytes (" << formatBytes(torrent.size) << ")" << std::endl;
                        std::cout << "[METADATA_LOG] Files: " << torrent.num_files << std::endl;
                        std::cout << "[METADATA_LOG] Content Type: " << torrent.content_type << std::endl;
                        
                        // Show file details
                        if (!torrent.file_names.empty()) {
                            std::cout << "[METADATA_LOG] File Details:" << std::endl;
                            for (size_t i = 0; i < std::min(torrent.file_names.size(), size_t(5)); ++i) {
                                std::cout << "[METADATA_LOG]   " << (i+1) << ". " << torrent.file_names[i];
                                if (i < torrent.file_sizes.size()) {
                                    std::cout << " (" << formatBytes(torrent.file_sizes[i]) << ")";
                                }
                                std::cout << std::endl;
                            }
                            if (torrent.file_names.size() > 5) {
                                std::cout << "[METADATA_LOG]   ... and " << (torrent.file_names.size() - 5) << " more files" << std::endl;
                            }
                        }
                        
                        if (!torrent.comment.empty()) {
                            std::cout << "[METADATA_LOG] Comment: " << torrent.comment.substr(0, 200) << (torrent.comment.length() > 200 ? "..." : "") << std::endl;
                        }
                        std::cout << "[METADATA_LOG] Trackers: " << torrent.trackers.size() << std::endl;
                        std::cout << "[METADATA_LOG] Magnet: " << torrent.magnet_link << std::endl;
                        std::cout << "[METADATA_LOG] ------------------------" << std::endl;
                    } else {
                        std::cout << "*** METADATA RECEIVED ***" << std::endl;
                        std::cout << "Hash: " << hash_str << std::endl;
                        std::cout << "Name: " << torrent.name << std::endl;
                        std::cout << "Size: " << torrent.size << " bytes (" << formatBytes(torrent.size) << ")" << std::endl;
                        std::cout << "Files: " << torrent.num_files << std::endl;
                        std::cout << "Content Type: " << torrent.content_type << std::endl;
                        std::cout << "Magnet Link: " << torrent.magnet_link << std::endl;
                        if (!torrent.file_names.empty()) {
                            std::cout << "First file: " << torrent.file_names[0] << std::endl;
                        }
                        if (!torrent.comment.empty()) {
                            std::cout << "Comment: " << torrent.comment.substr(0, 100) << (torrent.comment.length() > 100 ? "..." : "") << std::endl;
                        }
                        std::cout << "Trackers: " << torrent.trackers.size() << std::endl;
                        std::cout << "------------------------" << std::endl;
                    }
                } else {
                    m_metadata_fetched++;
                    
                    // Enhanced metadata logging for metadata_log_mode (test mode)
                    if (m_metadata_log_mode) {
                        std::cout << "[METADATA_LOG] *** METADATA DISCOVERED (TEST MODE) ***" << std::endl;
                        std::cout << "[METADATA_LOG] Hash: " << hash_str << std::endl;
                        std::cout << "[METADATA_LOG] Name: " << torrent.name << std::endl;
                        std::cout << "[METADATA_LOG] Size: " << torrent.size << " bytes (" << formatBytes(torrent.size) << ")" << std::endl;
                        std::cout << "[METADATA_LOG] Files: " << torrent.num_files << std::endl;
                        std::cout << "[METADATA_LOG] Content Type: " << torrent.content_type << std::endl;
                        
                        // Show file details
                        if (!torrent.file_names.empty()) {
                            std::cout << "[METADATA_LOG] File Details:" << std::endl;
                            for (size_t i = 0; i < std::min(torrent.file_names.size(), size_t(5)); ++i) {
                                std::cout << "[METADATA_LOG]   " << (i+1) << ". " << torrent.file_names[i];
                                if (i < torrent.file_sizes.size()) {
                                    std::cout << " (" << formatBytes(torrent.file_sizes[i]) << ")";
                                }
                                std::cout << std::endl;
                            }
                            if (torrent.file_names.size() > 5) {
                                std::cout << "[METADATA_LOG]   ... and " << (torrent.file_names.size() - 5) << " more files" << std::endl;
                            }
                        }
                        
                        if (!torrent.comment.empty()) {
                            std::cout << "[METADATA_LOG] Comment: " << torrent.comment.substr(0, 200) << (torrent.comment.length() > 200 ? "..." : "") << std::endl;
                        }
                        std::cout << "[METADATA_LOG] Trackers: " << torrent.trackers.size() << std::endl;
                        std::cout << "[METADATA_LOG] Magnet: " << torrent.magnet_link << std::endl;
                        std::cout << "[METADATA_LOG] ------------------------" << std::endl;
                    } else {
                        std::cout << "*** METADATA RECEIVED (TEST MODE) ***" << std::endl;
                        std::cout << "Hash: " << hash_str << std::endl;
                        std::cout << "Name: " << torrent.name << std::endl;
                        std::cout << "Size: " << torrent.size << " bytes (" << formatBytes(torrent.size) << ")" << std::endl;
                        std::cout << "Files: " << torrent.num_files << std::endl;
                        std::cout << "Content Type: " << torrent.content_type << std::endl;
                        std::cout << "Magnet Link: " << torrent.magnet_link << std::endl;
                        if (!torrent.file_names.empty()) {
                            std::cout << "First file: " << torrent.file_names[0] << std::endl;
                        }
                        if (!torrent.comment.empty()) {
                            std::cout << "Comment: " << torrent.comment.substr(0, 100) << (torrent.comment.length() > 100 ? "..." : "") << std::endl;
                        }
                        std::cout << "Trackers: " << torrent.trackers.size() << std::endl;
                        std::cout << "------------------------" << std::endl;
                    }
                }
            
            // Remove torrent from session to free resources
            m_session->remove_torrent(alert->handle);
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing metadata: " << e.what() << std::endl;
            m_mysql->logException("DHTTorrentCrawler::handleMetadataReceived", "", e);
        }
    }
    
    std::string determineContentType(const std::vector<std::string>& file_names) {
        if (file_names.empty()) return "unknown";
        
        // Check file extensions to determine content type
        std::set<std::string> extensions;
        for (const auto& filename : file_names) {
            size_t dot_pos = filename.find_last_of('.');
            if (dot_pos != std::string::npos) {
                std::string ext = filename.substr(dot_pos + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                extensions.insert(ext);
            }
        }
        
        // Video files
        std::set<std::string> video_exts = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "3gp", "mpg", "mpeg"};
        for (const auto& ext : extensions) {
            if (video_exts.count(ext)) return "video";
        }
        
        // Audio files
        std::set<std::string> audio_exts = {"mp3", "flac", "wav", "aac", "ogg", "wma", "m4a", "opus"};
        for (const auto& ext : extensions) {
            if (audio_exts.count(ext)) return "audio";
        }
        
        // Software/Games
        std::set<std::string> software_exts = {"exe", "msi", "dmg", "pkg", "deb", "rpm", "app", "iso", "bin"};
        for (const auto& ext : extensions) {
            if (software_exts.count(ext)) return "software";
        }
        
        // Documents
        std::set<std::string> doc_exts = {"pdf", "doc", "docx", "txt", "rtf", "odt", "ppt", "pptx", "xls", "xlsx"};
        for (const auto& ext : extensions) {
            if (doc_exts.count(ext)) return "document";
        }
        
        // Images
        std::set<std::string> image_exts = {"jpg", "jpeg", "png", "gif", "bmp", "tiff", "svg", "webp"};
        for (const auto& ext : extensions) {
            if (image_exts.count(ext)) return "image";
        }
        
        // Archives
        std::set<std::string> archive_exts = {"zip", "rar", "7z", "tar", "gz", "bz2", "xz"};
        for (const auto& ext : extensions) {
            if (archive_exts.count(ext)) return "archive";
        }
        
        return "other";
    }
    
    std::string formatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unit < 4) {
            size /= 1024.0;
            unit++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
        return oss.str();
    }
};

#endif // DISABLE_LIBTORRENT

// Global crawler instance for signal handling
#ifndef DISABLE_LIBTORRENT
DHTTorrentCrawler* g_crawler = nullptr;
#endif

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n\n*** Ctrl+C detected - Initiating graceful shutdown ***" << std::endl;
#ifndef DISABLE_LIBTORRENT
        if (g_crawler) {
            g_crawler->stop();
        }
#endif
    }
#ifndef _WIN32
    else if (signal == SIGTSTP) {
        std::cout << "\n\n*** Ctrl+Z detected - Pausing crawler ***" << std::endl;
        std::cout << "Use 'fg' to resume or Ctrl+C to shutdown gracefully" << std::endl;
#ifndef DISABLE_LIBTORRENT
        if (g_crawler) {
            g_crawler->stop();
        }
#endif
    }
#endif
    else if (signal == SIGTERM) {
        std::cout << "\n\n*** SIGTERM received - Shutting down ***" << std::endl;
#ifndef DISABLE_LIBTORRENT
        if (g_crawler) {
            g_crawler->stop();
        }
#endif
    }
}

struct LibraryStatus {
    bool libtorrent_available;
    bool mysql_available;
    std::string libtorrent_version;
    std::string mysql_version;
    std::string platform;
};

LibraryStatus checkLibraries() {
    LibraryStatus status;
    status.libtorrent_available = false;
    status.mysql_available = false;
    status.libtorrent_version = "unknown";
    status.mysql_version = "unknown";
    
#ifdef __linux__
    status.platform = "linux";
#elif __APPLE__
    status.platform = "macos";
#elif _WIN32
    status.platform = "windows";
#else
    status.platform = "unknown";
#endif

#ifndef DISABLE_LIBTORRENT
    status.libtorrent_available = true;
    // Try to get libtorrent version if possible
    try {
        // This is a simple check - in a real implementation you might want to 
        // call libtorrent functions to get the actual version
        status.libtorrent_version = "2.0+";
    } catch (...) {
        status.libtorrent_version = "available";
    }
#endif

#ifndef DISABLE_MYSQL
    status.mysql_available = true;
    status.mysql_version = "available";
#endif

    return status;
}

// Test function to simulate missing libraries for demonstration
LibraryStatus checkLibrariesTestMode(bool simulate_missing_libtorrent = false, bool simulate_missing_mysql = false) {
    LibraryStatus status = checkLibraries();
    
    if (simulate_missing_libtorrent) {
        status.libtorrent_available = false;
        status.libtorrent_version = "NOT FOUND";
    }
    
    if (simulate_missing_mysql) {
        status.mysql_available = false;
        status.mysql_version = "NOT FOUND";
    }
    
    return status;
}

void printLibraryInstallInstructions(const LibraryStatus& status) {
    if (status.platform == "linux") {
        std::cout << std::endl;
        std::cout << "=== LIBRARY INSTALLATION INSTRUCTIONS ===" << std::endl;
        std::cout << "Required libraries for Linux systems:" << std::endl;
        std::cout << std::endl;
        
        if (!status.libtorrent_available) {
            std::cout << "❌ libtorrent-rasterbar: NOT FOUND" << std::endl;
            std::cout << "   Install with: sudo apt install -y libtorrent-rasterbar2.0 libtorrent-rasterbar-dev" << std::endl;
            std::cout << "   Or on CentOS/RHEL: sudo yum install libtorrent-rasterbar-devel" << std::endl;
        } else {
            std::cout << "✅ libtorrent-rasterbar: " << status.libtorrent_version << std::endl;
        }
        
        if (!status.mysql_available) {
            std::cout << "❌ libmysqlclient: NOT FOUND" << std::endl;
            std::cout << "   Install with: sudo apt install -y libmysqlclient21 libmysqlclient-dev" << std::endl;
            std::cout << "   Or on CentOS/RHEL: sudo yum install mysql-devel" << std::endl;
        } else {
            std::cout << "✅ libmysqlclient: " << status.mysql_version << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "Complete installation command:" << std::endl;
        std::cout << "  sudo apt install -y libtorrent-rasterbar2.0 libtorrent-rasterbar-dev libmysqlclient21 libmysqlclient-dev" << std::endl;
        std::cout << std::endl;
    } else if (status.platform == "macos") {
        std::cout << std::endl;
        std::cout << "=== LIBRARY INSTALLATION INSTRUCTIONS ===" << std::endl;
        std::cout << "Required libraries for macOS:" << std::endl;
        std::cout << std::endl;
        
        if (!status.libtorrent_available) {
            std::cout << "❌ libtorrent-rasterbar: NOT FOUND" << std::endl;
            std::cout << "   Install with: brew install libtorrent-rasterbar" << std::endl;
        } else {
            std::cout << "✅ libtorrent-rasterbar: " << status.libtorrent_version << std::endl;
        }
        
        if (!status.mysql_available) {
            std::cout << "❌ libmysqlclient: NOT FOUND" << std::endl;
            std::cout << "   Install with: brew install mysql" << std::endl;
        } else {
            std::cout << "✅ libmysqlclient: " << status.mysql_version << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "Complete installation command:" << std::endl;
        std::cout << "  brew install libtorrent-rasterbar mysql" << std::endl;
        std::cout << std::endl;
    }
}

void printUsage(const char* program_name, bool test_missing_libs = false) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << "DHT Torrent Discovery Tool with MySQL Storage" << std::endl;
    std::cout << std::endl;
    std::cout << "DESCRIPTION:" << std::endl;
    std::cout << "  A comprehensive DHT (Distributed Hash Table) crawler that discovers torrents" << std::endl;
    std::cout << "  and peers from the BitTorrent DHT network, storing metadata in MySQL database." << std::endl;
    std::cout << "  Supports both continuous crawling and metadata-only modes." << std::endl;
    std::cout << std::endl;
    
    // Check library status and show installation instructions
    LibraryStatus lib_status;
    if (test_missing_libs) {
        lib_status = checkLibrariesTestMode(true, true); // Simulate both missing
    } else {
        lib_status = checkLibraries();
    }
    printLibraryInstallInstructions(lib_status);
    std::cout << "REQUIRED OPTIONS:" << std::endl;
    std::cout << "  --user USER       MySQL username for database connection" << std::endl;
    std::cout << "                    Example: --user myuser" << std::endl;
    std::cout << "  --password PASS   MySQL password for database connection" << std::endl;
    std::cout << "                    Example: --password mypassword" << std::endl;
    std::cout << "  --database DB     MySQL database name to store torrent data" << std::endl;
    std::cout << "                    Example: --database torrents" << std::endl;
    std::cout << std::endl;
    std::cout << "OPTIONAL OPTIONS:" << std::endl;
    std::cout << "  --server HOST     MySQL server hostname or IP address" << std::endl;
    std::cout << "                    Default: localhost" << std::endl;
    std::cout << "                    Example: --server 192.168.1.100" << std::endl;
    std::cout << "  --port PORT       MySQL server port number" << std::endl;
    std::cout << "                    Default: 3306" << std::endl;
    std::cout << "                    Example: --port 3307" << std::endl;
    std::cout << "  --queries NUM     Maximum number of DHT queries to perform" << std::endl;
    std::cout << "                    Default: infinite (run until Ctrl+C)" << std::endl;
    std::cout << "                    Example: --queries 10000" << std::endl;
    std::cout << "  --metadata HASHES Comma-delimited torrent hashes for metadata-only mode" << std::endl;
    std::cout << "                    Can also be a file path (.txt or .csv) containing hashes" << std::endl;
    std::cout << "                    Example: --metadata abc123def456,789xyz012" << std::endl;
    std::cout << "                    Example: --metadata /path/to/hashes.txt" << std::endl;
    std::cout << "  --debug           Enable detailed debug logging and verbose output" << std::endl;
    std::cout << "                    Example: --debug" << std::endl;
    std::cout << "  --verbose         Enable verbose output (default is counter mode)" << std::endl;
    std::cout << "                    Example: --verbose" << std::endl;
    std::cout << "  --metadata_log    Enable metadata-only logging (suppress all other logs)" << std::endl;
    std::cout << "                    Shows only metadata discovery, success, and timeout messages" << std::endl;
    std::cout << "                    Example: --metadata_log" << std::endl;
    std::cout << "  --sequential      Disable concurrent DHT worker pool (use sequential mode)" << std::endl;
    std::cout << "                    Example: --sequential" << std::endl;
    std::cout << "  --workers NUM     Number of concurrent DHT workers (default: 4)" << std::endl;
    std::cout << "                    Example: --workers 8" << std::endl;
    std::cout << "  --no-bep51        Disable BEP51 DHT infohash indexing (use random generation)" << std::endl;
    std::cout << "                    Example: --no-bep51" << std::endl;
    std::cout << "  --test-missing-libs Show help with simulated missing libraries (for testing)" << std::endl;
    std::cout << "                    Example: --test-missing-libs" << std::endl;
    std::cout << "  --help            Show this help message and exit" << std::endl;
    std::cout << "                    Example: --help" << std::endl;
    std::cout << std::endl;
    std::cout << "OPERATION MODES:" << std::endl;
    std::cout << "  Normal Mode:      Continuously crawls DHT network discovering new torrents" << std::endl;
    std::cout << "  Metadata Mode:    Fetches metadata for specific torrent hashes only" << std::endl;
    std::cout << "  Concurrent Mode:  Uses worker pool for parallel DHT query processing (default)" << std::endl;
    std::cout << "  Sequential Mode:  Uses single-threaded DHT query processing (legacy)" << std::endl;
    std::cout << std::endl;
    std::cout << "EXAMPLES:" << std::endl;
    std::cout << "  # Basic usage with local MySQL:" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Remote MySQL server with query limit:" << std::endl;
    std::cout << "  " << program_name << " --server 192.168.10.100 --user keynetworks --password K3yn3tw0rk5 --database torrents --queries 5000" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Metadata-only mode for specific hashes:" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents --metadata abc123def456,789xyz012" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Metadata-only mode from file:" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents --metadata /path/to/hashes.txt" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Debug mode for troubleshooting:" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents --debug --queries 100" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Metadata logging mode for focused metadata monitoring:" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents --metadata_log" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Verbose mode for detailed output:" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents --verbose" << std::endl;
    std::cout << std::endl;
    std::cout << "  # High-performance mode with 8 workers:" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents --workers 8" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Sequential mode (legacy):" << std::endl;
    std::cout << "  " << program_name << " --user admin --password secret --database torrents --sequential" << std::endl;
    std::cout << std::endl;
    std::cout << "DATABASE TABLES:" << std::endl;
    std::cout << "  The tool creates the following tables automatically:" << std::endl;
    std::cout << "  - discovered_torrents: Main torrent metadata and statistics" << std::endl;
    std::cout << "  - discovered_peers: Peer information for each torrent" << std::endl;
    std::cout << "  - torrent_files: Individual file information within torrents" << std::endl;
    std::cout << std::endl;
    std::cout << "CONTROL:" << std::endl;
    std::cout << "  Press Ctrl+C for graceful shutdown" << std::endl;
    std::cout << "  Press Ctrl+Z to pause (use 'fg' to resume)" << std::endl;
}

int main(int argc, char* argv[]) {
#ifdef DISABLE_LIBTORRENT
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    std::cout << "DHT Crawler - Windows Build (libtorrent disabled)" << std::endl;
    std::cout << "This is a placeholder Windows build." << std::endl;
    std::cout << "For full functionality, please use the Linux or macOS builds." << std::endl;
    return 0;
#else
    // Show help if no arguments provided
    if (argc == 1) {
        printUsage(argv[0]);
        return 0;
    }
    
    MySQLConfig config;
    int max_queries = -1; // Default to infinite
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--test-missing-libs") {
            printUsage(argv[0], true);
            return 0;
        } else if (arg == "--server" && i + 1 < argc) {
            config.server = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--user" && i + 1 < argc) {
            config.user = argv[++i];
        } else if (arg == "--password" && i + 1 < argc) {
            config.password = argv[++i];
        } else if (arg == "--database" && i + 1 < argc) {
            config.database = argv[++i];
        } else if (arg == "--queries" && i + 1 < argc) {
            max_queries = std::stoi(argv[++i]);
        } else if (arg == "--metadata" && i + 1 < argc) {
            config.metadata_hashes = argv[++i];
        } else if (arg == "--debug") {
            config.debug_mode = true;
        } else if (arg == "--verbose") {
            config.verbose_mode = true;
        } else if (arg == "--metadata_log") {
            config.metadata_log_mode = true;
        } else if (arg == "--sequential") {
            config.concurrent_mode = false;
        } else if (arg == "--workers" && i + 1 < argc) {
            config.num_workers = std::stoi(argv[++i]);
            if (config.num_workers < 1 || config.num_workers > 16) {
                std::cerr << "Error: Number of workers must be between 1 and 16" << std::endl;
                return 1;
            }
        } else if (arg == "--no-bep51") {
            config.bep51_mode = false;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Validate required parameters (skip for testing)
    if (config.user.empty() || config.password.empty() || config.database.empty()) {
        std::cout << "Running in test mode without MySQL..." << std::endl;
        config.user = "test";
        config.password = "test";
        config.database = "test";
    }
    
    // Set defaults
    if (config.server.empty()) {
        config.server = "localhost";
    }
    
    if (config.verbose_mode) {
        std::cout << "=== DHT Torrent Discovery Tool ===" << std::endl;
        std::cout << "MySQL Server: " << config.server << ":" << config.port << std::endl;
        std::cout << "Database: " << config.database << std::endl;
        std::cout << "User: " << config.user << std::endl;
        if (max_queries > 0) {
            std::cout << "Max Queries: " << max_queries << std::endl;
        } else {
            std::cout << "Running indefinitely (press Ctrl+C to stop)" << std::endl;
        }
        std::cout << std::endl;
    } else {
        std::cout << "DHT Crawler started - Press Ctrl+C to stop" << std::endl;
    }
    
    // Set up signal handling
    signal(SIGINT, signalHandler);   // Ctrl+C
#ifndef _WIN32
    signal(SIGTSTP, signalHandler);  // Ctrl+Z
#endif
    signal(SIGTERM, signalHandler);  // Termination signal
    
    try {
        DHTTorrentCrawler crawler(config);
        g_crawler = &crawler; // Set global reference for signal handler
        
        if (!crawler.initialize()) {
            std::cerr << "Failed to initialize crawler" << std::endl;
            return 1;
        }
        
        crawler.startCrawling(max_queries);
        
        // Perform graceful shutdown
        crawler.gracefulShutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        // Try to log the error if MySQL is available
        try {
            MySQLConfig temp_config;
            temp_config.server = "192.168.10.100";
            temp_config.user = "keynetworks";
            temp_config.password = "K3yn3tw0rk5";
            temp_config.database = "Torrents";
            MySQLConnection temp_mysql(temp_config);
            if (temp_mysql.connect()) {
                temp_mysql.logException("main", "", e);
            }
        } catch (...) {
            // Ignore logging errors in main
        }
        return 1;
    }
    
    return 0;
#endif
}
