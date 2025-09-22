#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/connection.h>
#include <cppconn/metadata.h>
#include <cppconn/sqlstring.h>

namespace dht_crawler {

/**
 * Comprehensive database manager for MySQL operations
 * Implements Tribler-inspired database architecture with advanced tracking and monitoring
 */
class DatabaseManager {
public:
    struct DatabaseConfig {
        std::string host = "192.168.10.100";
        std::string database = "Torrents";
        std::string username = "keynetworks";
        std::string password = "K3yn3tw0rk5";
        int port = 3306;
        int max_connections = 10;
        int connection_timeout = 30;
        bool auto_reconnect = true;
        bool ssl_enabled = false;
        std::string charset = "utf8mb4";
    };

    // Enhanced torrent metadata structure
    struct TorrentMetadata {
        int id = 0;
        std::string infohash;
        std::string name;
        size_t size = 0;
        size_t piece_length = 0;
        int pieces_count = 0;
        int files_count = 0;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point discovered_at;
        std::chrono::steady_clock::time_point last_seen_at;
        std::string tracker_info;
        std::string comment;
        std::string created_by;
        std::string encoding;
        bool private_flag = false;
        std::string source;
        std::string url_list;
        std::string announce_list;
        std::string file_list;
        size_t metadata_size = 0;
        
        enum class ValidationStatus {
            PENDING,
            VALID,
            INVALID,
            ERROR
        };
        ValidationStatus validation_status = ValidationStatus::PENDING;
        std::string validation_error;
        
        enum class MetadataSource {
            DHT,
            TRACKER,
            PEER,
            CACHE
        };
        MetadataSource metadata_source = MetadataSource::DHT;
        double quality_score = 0.0;
    };

    // Tracker state structure
    struct TrackerState {
        int id = 0;
        std::string tracker_url;
        std::string tracker_domain;
        enum class TrackerType {
            HTTP,
            HTTPS,
            UDP
        };
        TrackerType tracker_type = TrackerType::HTTP;
        std::chrono::steady_clock::time_point last_announce;
        std::chrono::steady_clock::time_point last_scrape;
        int announce_interval = 1800;
        int min_interval = 0;
        int scrape_interval = 0;
        
        enum class Status {
            ACTIVE,
            INACTIVE,
            ERROR,
            UNKNOWN
        };
        Status status = Status::UNKNOWN;
        std::string error_message;
        int response_time_ms = 0;
        double success_rate = 0.0;
        int total_announces = 0;
        int successful_announces = 0;
        int total_scrapes = 0;
        int successful_scrapes = 0;
        int peers_returned = 0;
        int seeders = 0;
        int leechers = 0;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point updated_at;
    };

    // Peer state structure
    struct PeerState {
        int id = 0;
        std::string peer_id;
        std::string peer_ip;
        int peer_port = 0;
        std::string peer_country;
        
        enum class ConnectionType {
            TCP,
            UTP,
            UNKNOWN
        };
        ConnectionType connection_type = ConnectionType::UNKNOWN;
        std::chrono::steady_clock::time_point last_seen;
        std::chrono::steady_clock::time_point first_seen;
        int connection_attempts = 0;
        int successful_connections = 0;
        int failed_connections = 0;
        int avg_response_time_ms = 0;
        int metadata_requests = 0;
        int metadata_responses = 0;
        double quality_score = 0.0;
        
        enum class Status {
            GOOD,
            UNKNOWN,
            BAD,
            BLACKLISTED
        };
        Status status = Status::UNKNOWN;
        std::string blacklist_reason;
        
        // BEP 9 Extension Protocol Tracking
        bool bep9_support = false;
        int bep9_negotiation_attempts = 0;
        int bep9_negotiation_successes = 0;
        std::chrono::steady_clock::time_point bep9_last_negotiation;
        std::string bep9_negotiation_error;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point updated_at;
    };

    // DHT node state structure
    struct DHTNodeState {
        int id = 0;
        std::string node_id;
        std::string node_ip;
        int node_port = 0;
        std::chrono::steady_clock::time_point last_seen;
        std::chrono::steady_clock::time_point first_seen;
        int ping_attempts = 0;
        int successful_pings = 0;
        int failed_pings = 0;
        int avg_response_time_ms = 0;
        int find_node_requests = 0;
        int find_node_responses = 0;
        int get_peers_requests = 0;
        int get_peers_responses = 0;
        int announce_peer_requests = 0;
        int announce_peer_responses = 0;
        double quality_score = 0.0;
        
        enum class Status {
            GOOD,
            UNKNOWN,
            BAD,
            BLACKLISTED
        };
        Status status = Status::UNKNOWN;
        std::string blacklist_reason;
        int bucket_distance = 0;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point updated_at;
    };

    // Crawl session structure
    struct CrawlSession {
        int id = 0;
        std::string session_id;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        int duration_seconds = 0;
        
        enum class Status {
            RUNNING,
            COMPLETED,
            FAILED,
            STOPPED
        };
        Status status = Status::RUNNING;
        int total_requests = 0;
        int successful_requests = 0;
        int failed_requests = 0;
        int total_peers_discovered = 0;
        int total_torrents_discovered = 0;
        int total_metadata_extracted = 0;
        int avg_response_time_ms = 0;
        int peak_memory_usage_mb = 0;
        double peak_cpu_usage_percent = 0.0;
        int error_count = 0;
        int warning_count = 0;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point updated_at;
    };

    // Metadata request structure
    struct MetadataRequest {
        int id = 0;
        std::string request_id;
        std::string infohash;
        std::string peer_ip;
        int peer_port = 0;
        
        enum class RequestType {
            UT_METADATA,
            DIRECT,
            DHT
        };
        RequestType request_type = RequestType::UT_METADATA;
        
        enum class RequestMethod {
            LIBTORRENT,
            DIRECT_TCP,
            HYBRID
        };
        RequestMethod request_method = RequestMethod::LIBTORRENT;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        int duration_ms = 0;
        
        enum class Status {
            PENDING,
            SUCCESS,
            FAILED,
            TIMEOUT
        };
        Status status = Status::PENDING;
        std::string error_message;
        size_t metadata_size = 0;
        int pieces_requested = 0;
        int pieces_received = 0;
        int retry_count = 0;
        double quality_score = 0.0;
        
        // BEP 9 Extension Protocol Tracking
        bool extension_protocol_used = false;
        std::string extension_protocol_version;
        std::string extension_protocol_error;
        std::chrono::steady_clock::time_point created_at;
    };

private:
    DatabaseConfig config_;
    std::unique_ptr<sql::Connection> connection_;
    std::mutex connection_mutex_;
    std::vector<std::unique_ptr<sql::Connection>> connection_pool_;
    std::mutex pool_mutex_;
    std::atomic<bool> is_connected_{false};
    
    // Prepared statements cache
    std::map<std::string, std::unique_ptr<sql::PreparedStatement>> prepared_statements_;
    std::mutex statements_mutex_;
    
    // Connection management
    bool initializeConnection();
    void cleanupConnection();
    sql::Connection* getConnection();
    void returnConnection(sql::Connection* conn);
    bool testConnection();
    void reconnect();

    // Prepared statement management
    sql::PreparedStatement* getPreparedStatement(const std::string& query);
    void prepareStatements();

    // Utility methods
    std::string infohashToHex(const std::string& infohash);
    std::string hexToInfohash(const std::string& hex);
    std::string statusToString(TorrentMetadata::ValidationStatus status);
    std::string sourceToString(TorrentMetadata::MetadataSource source);
    std::string trackerTypeToString(TrackerState::TrackerType type);
    std::string trackerStatusToString(TrackerState::Status status);
    std::string connectionTypeToString(PeerState::ConnectionType type);
    std::string peerStatusToString(PeerState::Status status);
    std::string dhtStatusToString(DHTNodeState::Status status);
    std::string sessionStatusToString(CrawlSession::Status status);
    std::string requestTypeToString(MetadataRequest::RequestType type);
    std::string requestMethodToString(MetadataRequest::RequestMethod type);
    std::string requestStatusToString(MetadataRequest::Status status);

public:
    DatabaseManager(const DatabaseConfig& config = DatabaseConfig{});
    ~DatabaseManager();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected();
    
    // Torrent metadata operations
    bool insertTorrentMetadata(const TorrentMetadata& metadata);
    bool updateTorrentMetadata(const std::string& infohash, const TorrentMetadata& metadata);
    std::vector<TorrentMetadata> getTorrentMetadata(const std::string& infohash);
    std::vector<TorrentMetadata> getTorrentMetadataByStatus(TorrentMetadata::ValidationStatus status);
    std::vector<TorrentMetadata> getTorrentMetadataBySource(TorrentMetadata::MetadataSource source);
    bool deleteTorrentMetadata(const std::string& infohash);
    int getTorrentMetadataCount();
    
    // Tracker state operations
    bool insertOrUpdateTrackerState(const TrackerState& state);
    std::vector<TrackerState> getTrackerStates(const std::string& domain = "");
    std::vector<TrackerState> getTrackerStatesByStatus(TrackerState::Status status);
    bool updateTrackerStats(const std::string& url, bool success, int responseTime);
    bool deleteTrackerState(const std::string& url);
    int getTrackerStateCount();
    
    // Peer state operations
    bool insertOrUpdatePeerState(const PeerState& state);
    std::vector<PeerState> getPeerStates(const std::string& status = "");
    std::vector<PeerState> getPeerStatesByStatus(PeerState::Status status);
    std::vector<PeerState> getBEP9CompatiblePeers();
    bool updatePeerStats(const std::string& ip, int port, bool success, int responseTime);
    bool updatePeerBEP9Support(const std::string& peerIp, int peerPort, bool supportsBEP9);
    bool logExtensionProtocolNegotiation(const std::string& peerIp, int peerPort, 
                                        const std::string& error, bool success);
    bool deletePeerState(const std::string& ip, int port);
    int getPeerStateCount();
    
    // DHT node state operations
    bool insertOrUpdateDHTNodeState(const DHTNodeState& state);
    std::vector<DHTNodeState> getDHTNodeStates(const std::string& status = "");
    std::vector<DHTNodeState> getDHTNodeStatesByStatus(DHTNodeState::Status status);
    bool updateDHTNodeStats(const std::string& ip, int port, bool success, int responseTime);
    bool deleteDHTNodeState(const std::string& ip, int port);
    int getDHTNodeStateCount();
    
    // Session management
    std::string createCrawlSession();
    bool updateCrawlSession(const std::string& sessionId, const CrawlSession& session);
    bool endCrawlSession(const std::string& sessionId);
    std::vector<CrawlSession> getCrawlSessions();
    CrawlSession getCrawlSession(const std::string& sessionId);
    int getCrawlSessionCount();
    
    // Request tracking
    std::string createMetadataRequest(const std::string& infohash, const std::string& peerIp, int peerPort);
    bool updateMetadataRequest(const std::string& requestId, const MetadataRequest& request);
    bool completeMetadataRequest(const std::string& requestId, bool success, const std::string& error = "");
    bool updateExtensionProtocolRequest(const std::string& requestId, bool used, 
                                       const std::string& version, const std::string& error);
    std::vector<MetadataRequest> getMetadataRequests(const std::string& infohash = "");
    int getMetadataRequestCount();
    
    // Statistics and reporting
    std::map<std::string, int> getSessionStatistics(const std::string& sessionId);
    std::map<std::string, int> getOverallStatistics();
    std::vector<std::pair<std::string, int>> getTopTrackers(int limit = 10);
    std::vector<std::pair<std::string, int>> getTopPeers(int limit = 10);
    std::map<std::string, int> getExtensionProtocolStatistics();
    
    // Database maintenance
    bool createTables();
    bool dropTables();
    bool backupDatabase(const std::string& backupPath);
    bool restoreDatabase(const std::string& backupPath);
    bool optimizeDatabase();
    bool vacuumDatabase();
    
    // Migration support
    bool migrateFromOldSchema();
    bool validateSchema();
    std::vector<std::string> getSchemaIssues();
    
    // Performance monitoring
    std::map<std::string, double> getPerformanceMetrics();
    bool enableQueryLogging(bool enable);
    std::vector<std::string> getSlowQueries(int limit = 10);
    
    // Transaction support
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool isInTransaction();
    
    // Connection pool management
    bool initializeConnectionPool();
    void shutdownConnectionPool();
    int getActiveConnectionCount();
    int getPoolSize();
    
    // Error handling
    std::string getLastError();
    int getLastErrorCode();
    bool hasError();
    void clearError();
    
    // Configuration
    void updateConfig(const DatabaseConfig& config);
    DatabaseConfig getConfig() const;
    
    // Health check
    bool healthCheck();
    std::map<std::string, std::string> getHealthStatus();
};

/**
 * Database manager factory for different database strategies
 */
class DatabaseManagerFactory {
public:
    enum class DatabaseStrategy {
        PERFORMANCE,    // Optimized for performance
        RELIABILITY,   // Optimized for reliability
        BALANCED,      // Balanced performance and reliability
        CUSTOM         // Custom configuration
    };
    
    static std::unique_ptr<DatabaseManager> createDatabaseManager(
        DatabaseStrategy strategy,
        const DatabaseManager::DatabaseConfig& config = DatabaseManager::DatabaseConfig{}
    );
    
    static DatabaseManager::DatabaseConfig getDefaultConfig(DatabaseStrategy strategy);
};

/**
 * Database transaction RAII wrapper
 */
class DatabaseTransaction {
private:
    std::shared_ptr<DatabaseManager> db_manager_;
    bool committed_;

public:
    DatabaseTransaction(std::shared_ptr<DatabaseManager> db_manager);
    ~DatabaseTransaction();
    
    /**
     * Commit the transaction
     * @return true if commit was successful
     */
    bool commit();
    
    /**
     * Rollback the transaction
     * @return true if rollback was successful
     */
    bool rollback();
    
    /**
     * Check if transaction is active
     * @return true if transaction is active
     */
    bool isActive();
};

} // namespace dht_crawler
