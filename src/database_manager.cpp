#include "database_manager.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace dht_crawler {

DatabaseManager::DatabaseManager(const DatabaseConfig& config) : config_(config) {
    initializeConnectionPool();
}

DatabaseManager::~DatabaseManager() {
    closeAllConnections();
}

void DatabaseManager::initializeConnectionPool() {
    for (int i = 0; i < config_.max_connections; i++) {
        auto connection = createConnection();
        if (connection) {
            connection_pool_.push(connection);
        }
    }
}

std::shared_ptr<sql::Connection> DatabaseManager::createConnection() {
    try {
        sql::Driver* driver = sql::mysql::get_driver_instance();
        std::string connection_string = "tcp://" + config_.host + ":" + std::to_string(config_.port);
        
        auto connection = std::shared_ptr<sql::Connection>(
            driver->connect(connection_string, config_.username, config_.password)
        );
        
        connection->setSchema(config_.database);
        connection->setAutoCommit(false);
        
        return connection;
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to create database connection: " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<sql::Connection> DatabaseManager::getConnection() {
    std::lock_guard<std::mutex> lock(connection_pool_mutex_);
    
    if (!connection_pool_.empty()) {
        auto connection = connection_pool_.front();
        connection_pool_.pop();
        return connection;
    }
    
    // Create new connection if pool is empty
    return createConnection();
}

void DatabaseManager::returnConnection(std::shared_ptr<sql::Connection> connection) {
    if (connection) {
        std::lock_guard<std::mutex> lock(connection_pool_mutex_);
        connection_pool_.push(connection);
    }
}

void DatabaseManager::closeAllConnections() {
    std::lock_guard<std::mutex> lock(connection_pool_mutex_);
    
    while (!connection_pool_.empty()) {
        auto connection = connection_pool_.front();
        connection_pool_.pop();
        if (connection) {
            connection->close();
        }
    }
}

bool DatabaseManager::insertTorrentMetadata(const TorrentMetadata& metadata) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "INSERT INTO TorrentMetadata (infohash, name, size, piece_length, pieces_count, files_count, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, metadata.infohash);
        statement->setString(2, metadata.name);
        statement->setUInt64(3, metadata.size);
        statement->setUInt64(4, metadata.piece_length);
        statement->setInt(5, metadata.pieces_count);
        statement->setInt(6, metadata.files_count);
        statement->setDateTime(7, formatDateTime(metadata.created_at));
        statement->setDateTime(8, formatDateTime(metadata.updated_at));
        
        statement->execute();
        connection->commit();
        
        returnConnection(connection);
        return true;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to insert torrent metadata: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

bool DatabaseManager::updateTorrentMetadata(const TorrentMetadata& metadata) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "UPDATE TorrentMetadata SET name = ?, size = ?, piece_length = ?, pieces_count = ?, files_count = ?, updated_at = ? WHERE infohash = ?";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, metadata.name);
        statement->setUInt64(2, metadata.size);
        statement->setUInt64(3, metadata.piece_length);
        statement->setInt(4, metadata.pieces_count);
        statement->setInt(5, metadata.files_count);
        statement->setDateTime(6, formatDateTime(metadata.updated_at));
        statement->setString(7, metadata.infohash);
        
        int rows_affected = statement->executeUpdate();
        connection->commit();
        
        returnConnection(connection);
        return rows_affected > 0;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to update torrent metadata: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

bool DatabaseManager::deleteTorrentMetadata(const std::string& infohash) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "DELETE FROM TorrentMetadata WHERE infohash = ?";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, infohash);
        
        int rows_affected = statement->executeUpdate();
        connection->commit();
        
        returnConnection(connection);
        return rows_affected > 0;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to delete torrent metadata: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

DatabaseManager::TorrentMetadata DatabaseManager::getTorrentMetadata(const std::string& infohash) {
    auto connection = getConnection();
    if (!connection) {
        return TorrentMetadata{};
    }
    
    try {
        std::string query = "SELECT * FROM TorrentMetadata WHERE infohash = ?";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, infohash);
        
        auto result = std::shared_ptr<sql::ResultSet>(statement->executeQuery());
        
        TorrentMetadata metadata;
        if (result->next()) {
            metadata.id = result->getInt("id");
            metadata.infohash = result->getString("infohash");
            metadata.name = result->getString("name");
            metadata.size = result->getUInt64("size");
            metadata.piece_length = result->getUInt64("piece_length");
            metadata.pieces_count = result->getInt("pieces_count");
            metadata.files_count = result->getInt("files_count");
            metadata.created_at = parseDateTime(result->getString("created_at"));
            metadata.updated_at = parseDateTime(result->getString("updated_at"));
        }
        
        returnConnection(connection);
        return metadata;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to get torrent metadata: " << e.what() << std::endl;
        returnConnection(connection);
        return TorrentMetadata{};
    }
}

std::vector<DatabaseManager::TorrentMetadata> DatabaseManager::getAllTorrentMetadata() {
    auto connection = getConnection();
    if (!connection) {
        return {};
    }
    
    try {
        std::string query = "SELECT * FROM TorrentMetadata ORDER BY created_at DESC";
        
        auto statement = std::shared_ptr<sql::Statement>(connection->createStatement());
        auto result = std::shared_ptr<sql::ResultSet>(statement->executeQuery(query));
        
        std::vector<TorrentMetadata> metadata_list;
        while (result->next()) {
            TorrentMetadata metadata;
            metadata.id = result->getInt("id");
            metadata.infohash = result->getString("infohash");
            metadata.name = result->getString("name");
            metadata.size = result->getUInt64("size");
            metadata.piece_length = result->getUInt64("piece_length");
            metadata.pieces_count = result->getInt("pieces_count");
            metadata.files_count = result->getInt("files_count");
            metadata.created_at = parseDateTime(result->getString("created_at"));
            metadata.updated_at = parseDateTime(result->getString("updated_at"));
            
            metadata_list.push_back(metadata);
        }
        
        returnConnection(connection);
        return metadata_list;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to get all torrent metadata: " << e.what() << std::endl;
        returnConnection(connection);
        return {};
    }
}

bool DatabaseManager::insertTrackerState(const TrackerState& tracker) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "INSERT INTO TrackerState (infohash, tracker_url, status, last_announce, next_announce, seeders, leechers, downloaded) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, tracker.infohash);
        statement->setString(2, tracker.tracker_url);
        statement->setString(3, tracker.status);
        statement->setDateTime(4, formatDateTime(tracker.last_announce));
        statement->setDateTime(5, formatDateTime(tracker.next_announce));
        statement->setInt(6, tracker.seeders);
        statement->setInt(7, tracker.leechers);
        statement->setInt(8, tracker.downloaded);
        
        statement->execute();
        connection->commit();
        
        returnConnection(connection);
        return true;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to insert tracker state: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

bool DatabaseManager::insertPeerState(const PeerState& peer) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "INSERT INTO PeerState (infohash, peer_id, ip_address, port, status, last_seen, uploaded, downloaded, left_bytes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, peer.infohash);
        statement->setString(2, peer.peer_id);
        statement->setString(3, peer.ip_address);
        statement->setInt(4, peer.port);
        statement->setString(5, peer.status);
        statement->setDateTime(6, formatDateTime(peer.last_seen));
        statement->setUInt64(7, peer.uploaded);
        statement->setUInt64(8, peer.downloaded);
        statement->setUInt64(9, peer.left_bytes);
        
        statement->execute();
        connection->commit();
        
        returnConnection(connection);
        return true;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to insert peer state: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

bool DatabaseManager::insertDHTNodeState(const DHTNodeState& node) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "INSERT INTO DHTNodeState (node_id, ip_address, port, status, last_seen, quality_score, response_time) VALUES (?, ?, ?, ?, ?, ?, ?)";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, node.node_id);
        statement->setString(2, node.ip_address);
        statement->setInt(3, node.port);
        statement->setString(4, node.status);
        statement->setDateTime(5, formatDateTime(node.last_seen));
        statement->setDouble(6, node.quality_score);
        statement->setInt(7, node.response_time);
        
        statement->execute();
        connection->commit();
        
        returnConnection(connection);
        return true;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to insert DHT node state: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

bool DatabaseManager::insertCrawlSession(const CrawlSession& session) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "INSERT INTO CrawlSession (session_id, start_time, end_time, status, torrents_discovered, torrents_processed, errors_count) VALUES (?, ?, ?, ?, ?, ?, ?)";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, session.session_id);
        statement->setDateTime(2, formatDateTime(session.start_time));
        statement->setDateTime(3, formatDateTime(session.end_time));
        statement->setString(4, session.status);
        statement->setInt(5, session.torrents_discovered);
        statement->setInt(6, session.torrents_processed);
        statement->setInt(7, session.errors_count);
        
        statement->execute();
        connection->commit();
        
        returnConnection(connection);
        return true;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to insert crawl session: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

bool DatabaseManager::insertMetadataRequest(const MetadataRequest& request) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "INSERT INTO MetadataRequest (request_id, infohash, peer_ip, peer_port, status, created_at, completed_at, error_message) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, request.request_id);
        statement->setString(2, request.infohash);
        statement->setString(3, request.peer_ip);
        statement->setInt(4, request.peer_port);
        statement->setString(5, request.status);
        statement->setDateTime(6, formatDateTime(request.created_at));
        statement->setDateTime(7, formatDateTime(request.completed_at));
        statement->setString(8, request.error_message);
        
        statement->execute();
        connection->commit();
        
        returnConnection(connection);
        return true;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to insert metadata request: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

bool DatabaseManager::insertBEP9ExtensionProtocol(const BEP9ExtensionProtocol& protocol) {
    auto connection = getConnection();
    if (!connection) {
        return false;
    }
    
    try {
        std::string query = "INSERT INTO BEP9ExtensionProtocol (infohash, peer_ip, peer_port, extension_name, status, created_at, completed_at, error_message) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        
        auto statement = std::shared_ptr<sql::PreparedStatement>(
            connection->prepareStatement(query)
        );
        
        statement->setString(1, protocol.infohash);
        statement->setString(2, protocol.peer_ip);
        statement->setInt(3, protocol.peer_port);
        statement->setString(4, protocol.extension_name);
        statement->setString(5, protocol.status);
        statement->setDateTime(6, formatDateTime(protocol.created_at));
        statement->setDateTime(7, formatDateTime(protocol.completed_at));
        statement->setString(8, protocol.error_message);
        
        statement->execute();
        connection->commit();
        
        returnConnection(connection);
        return true;
        
    } catch (const sql::SQLException& e) {
        std::cerr << "Failed to insert BEP9 extension protocol: " << e.what() << std::endl;
        connection->rollback();
        returnConnection(connection);
        return false;
    }
}

std::string DatabaseManager::formatDateTime(const std::chrono::steady_clock::time_point& time_point) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    return ss.str();
}

std::chrono::steady_clock::time_point DatabaseManager::parseDateTime(const std::string& datetime) {
    // Simple datetime parsing - in a real implementation, you'd use proper parsing
    return std::chrono::steady_clock::now();
}

void DatabaseManager::updateConfig(const DatabaseConfig& config) {
    config_ = config;
}

DatabaseManager::DatabaseConfig DatabaseManager::getConfig() const {
    return config_;
}

std::map<std::string, std::string> DatabaseManager::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["host"] = config_.host;
    status["database"] = config_.database;
    status["port"] = std::to_string(config_.port);
    status["max_connections"] = std::to_string(config_.max_connections);
    status["connection_timeout"] = std::to_string(config_.connection_timeout);
    status["auto_reconnect"] = config_.auto_reconnect ? "true" : "false";
    status["ssl_enabled"] = config_.ssl_enabled ? "true" : "false";
    status["charset"] = config_.charset;
    
    return status;
}

} // namespace dht_crawler
