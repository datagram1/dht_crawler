#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace dht_crawler {

/**
 * Direct TCP peer connector for establishing direct connections to peers
 * Provides TCP socket management, connection pooling, and IPv4/IPv6 support
 */
class DirectPeerConnector {
public:
    enum class ConnectionState {
        DISCONNECTED,   // Not connected
        CONNECTING,     // Connection in progress
        CONNECTED,      // Successfully connected
        FAILED,         // Connection failed
        TIMEOUT,        // Connection timed out
        CLOSED          // Connection closed
    };

    enum class ConnectionType {
        TCP,            // TCP connection
        UTP,            // uTP connection (future implementation)
        HYBRID          // Hybrid connection type
    };

    struct ConnectionConfig {
        std::chrono::milliseconds connection_timeout{30000};    // 30 seconds
        std::chrono::milliseconds read_timeout{10000};          // 10 seconds
        std::chrono::milliseconds write_timeout{10000};         // 10 seconds
        int max_connections = 100;                              // Maximum concurrent connections
        int max_connections_per_peer = 5;                      // Max connections per peer
        bool enable_connection_pooling = true;                 // Enable connection pooling
        bool enable_keep_alive = true;                         // Enable TCP keep-alive
        int keep_alive_interval = 30;                          // Keep-alive interval in seconds
        bool enable_nagle_algorithm = false;                   // Enable Nagle's algorithm
        int socket_buffer_size = 65536;                        // Socket buffer size
        bool enable_ipv6 = true;                               // Enable IPv6 support
        bool enable_connection_reuse = true;                   // Enable connection reuse
        int max_retry_attempts = 3;                            // Maximum retry attempts
        std::chrono::milliseconds retry_delay{1000};           // Retry delay
        double retry_backoff_multiplier = 2.0;                 // Retry backoff multiplier
    };

    struct ConnectionInfo {
        std::string connection_id;
        std::string peer_ip;
        int peer_port;
        std::string peer_id;
        ConnectionState state;
        ConnectionType type;
        int socket_fd;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point connected_at;
        std::chrono::steady_clock::time_point last_activity;
        std::chrono::milliseconds connection_duration;
        
        // Connection statistics
        int connection_attempts;
        int successful_connections;
        int failed_connections;
        std::chrono::milliseconds avg_connection_time;
        std::chrono::milliseconds min_connection_time;
        std::chrono::milliseconds max_connection_time;
        
        // Data transfer statistics
        size_t bytes_sent;
        size_t bytes_received;
        int messages_sent;
        int messages_received;
        
        // Error tracking
        std::vector<std::string> connection_errors;
        std::chrono::steady_clock::time_point last_error_time;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct ConnectionStatistics {
        int total_connections = 0;
        int active_connections = 0;
        int successful_connections = 0;
        int failed_connections = 0;
        int timeout_connections = 0;
        int closed_connections = 0;
        std::chrono::milliseconds avg_connection_time;
        std::chrono::milliseconds max_connection_time;
        std::chrono::milliseconds min_connection_time;
        std::map<std::string, int> connections_by_peer;
        std::map<ConnectionState, int> connections_by_state;
        std::map<ConnectionType, int> connections_by_type;
        double connection_success_rate = 0.0;
        size_t total_bytes_sent = 0;
        size_t total_bytes_received = 0;
        int total_messages_sent = 0;
        int total_messages_received = 0;
    };

private:
    ConnectionConfig config_;
    std::map<std::string, std::shared_ptr<ConnectionInfo>> active_connections_;
    std::mutex connections_mutex_;
    
    std::map<std::string, std::vector<ConnectionInfo>> connection_history_;
    std::mutex history_mutex_;
    
    std::queue<std::shared_ptr<ConnectionInfo>> connection_pool_;
    std::mutex pool_mutex_;
    std::condition_variable pool_condition_;
    
    ConnectionStatistics stats_;
    std::mutex stats_mutex_;
    
    // Background processing
    std::thread connection_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Internal methods
    void connectionMonitorLoop();
    void monitorConnections();
    void cleanupClosedConnections();
    void updateStatistics(const ConnectionInfo& connection_info, bool success);
    std::string generateConnectionId();
    int createSocket(bool is_ipv6);
    bool configureSocket(int socket_fd);
    bool connectToPeer(int socket_fd, const std::string& peer_ip, int peer_port);
    bool setSocketNonBlocking(int socket_fd);
    bool setSocketBlocking(int socket_fd);
    void closeConnection(const std::string& connection_id);
    std::shared_ptr<ConnectionInfo> getConnectionFromPool();
    void returnConnectionToPool(std::shared_ptr<ConnectionInfo> connection);
    bool isConnectionPoolFull();
    void initializeConnectionPool();
    void shutdownConnectionPool();

public:
    DirectPeerConnector(const ConnectionConfig& config = ConnectionConfig{});
    ~DirectPeerConnector();
    
    /**
     * Establish a direct connection to a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID (optional)
     * @param connection_type Connection type
     * @return Connection ID
     */
    std::string connectToPeer(const std::string& peer_ip,
                             int peer_port,
                             const std::string& peer_id = "",
                             ConnectionType connection_type = ConnectionType::TCP);
    
    /**
     * Establish a connection with custom timeout
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param timeout Custom timeout duration
     * @param peer_id Peer ID (optional)
     * @return Connection ID
     */
    std::string connectToPeerWithTimeout(const std::string& peer_ip,
                                        int peer_port,
                                        std::chrono::milliseconds timeout,
                                        const std::string& peer_id = "");
    
    /**
     * Close a connection
     * @param connection_id Connection ID
     * @return true if connection was closed successfully
     */
    bool closeConnection(const std::string& connection_id);
    
    /**
     * Close all connections to a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Number of connections closed
     */
    int closeConnectionsToPeer(const std::string& peer_ip, int peer_port);
    
    /**
     * Check if connection is active
     * @param connection_id Connection ID
     * @return true if connection is active
     */
    bool isConnectionActive(const std::string& connection_id);
    
    /**
     * Get connection information
     * @param connection_id Connection ID
     * @return Connection information or nullptr if not found
     */
    std::shared_ptr<ConnectionInfo> getConnectionInfo(const std::string& connection_id);
    
    /**
     * Get connections by peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of connection information
     */
    std::vector<std::shared_ptr<ConnectionInfo>> getConnectionsByPeer(const std::string& peer_ip, int peer_port);
    
    /**
     * Get connections by state
     * @param state Connection state
     * @return Vector of connection information
     */
    std::vector<std::shared_ptr<ConnectionInfo>> getConnectionsByState(ConnectionState state);
    
    /**
     * Get active connections
     * @return Vector of active connection information
     */
    std::vector<std::shared_ptr<ConnectionInfo>> getActiveConnections();
    
    /**
     * Get connection history for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of historical connection information
     */
    std::vector<ConnectionInfo> getConnectionHistory(const std::string& peer_ip, int peer_port);
    
    /**
     * Send data through a connection
     * @param connection_id Connection ID
     * @param data Data to send
     * @param size Data size
     * @return Number of bytes sent, or -1 if failed
     */
    ssize_t sendData(const std::string& connection_id, const void* data, size_t size);
    
    /**
     * Receive data from a connection
     * @param connection_id Connection ID
     * @param buffer Buffer to receive data
     * @param size Buffer size
     * @return Number of bytes received, or -1 if failed
     */
    ssize_t receiveData(const std::string& connection_id, void* buffer, size_t size);
    
    /**
     * Send a string through a connection
     * @param connection_id Connection ID
     * @param message Message to send
     * @return Number of bytes sent, or -1 if failed
     */
    ssize_t sendMessage(const std::string& connection_id, const std::string& message);
    
    /**
     * Receive a string from a connection
     * @param connection_id Connection ID
     * @param max_size Maximum message size
     * @return Received message, or empty string if failed
     */
    std::string receiveMessage(const std::string& connection_id, size_t max_size = 4096);
    
    /**
     * Get connection statistics
     * @return Current connection statistics
     */
    ConnectionStatistics getStatistics();
    
    /**
     * Reset connection statistics
     */
    void resetStatistics();
    
    /**
     * Update connection configuration
     * @param config New connection configuration
     */
    void updateConfig(const ConnectionConfig& config);
    
    /**
     * Enable or disable connection pooling
     * @param enable true to enable connection pooling
     */
    void setConnectionPoolingEnabled(bool enable);
    
    /**
     * Check if connection pooling is enabled
     * @return true if connection pooling is enabled
     */
    bool isConnectionPoolingEnabled();
    
    /**
     * Get connection pool size
     * @return Current connection pool size
     */
    size_t getConnectionPoolSize();
    
    /**
     * Get maximum connection pool size
     * @return Maximum connection pool size
     */
    size_t getMaxConnectionPoolSize();
    
    /**
     * Set maximum connection pool size
     * @param max_size New maximum pool size
     */
    void setMaxConnectionPoolSize(size_t max_size);
    
    /**
     * Enable or disable IPv6 support
     * @param enable true to enable IPv6 support
     */
    void setIPv6Enabled(bool enable);
    
    /**
     * Check if IPv6 is enabled
     * @return true if IPv6 is enabled
     */
    bool isIPv6Enabled();
    
    /**
     * Get connection success rate
     * @return Connection success rate (0.0 to 1.0)
     */
    double getConnectionSuccessRate();
    
    /**
     * Get average connection time
     * @return Average connection time
     */
    std::chrono::milliseconds getAverageConnectionTime();
    
    /**
     * Get connection count
     * @return Total number of active connections
     */
    int getConnectionCount();
    
    /**
     * Get connection count by state
     * @param state Connection state
     * @return Number of connections with the state
     */
    int getConnectionCount(ConnectionState state);
    
    /**
     * Start connection monitor
     */
    void start();
    
    /**
     * Stop connection monitor
     */
    void stop();
    
    /**
     * Check if connector is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export connection data to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportConnectionData(const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get connector health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear connection history
     */
    void clearConnectionHistory();
    
    /**
     * Clear active connections
     */
    void clearActiveConnections();
    
    /**
     * Force cleanup of closed connections
     */
    void forceCleanup();
    
    /**
     * Get peer connection statistics
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Peer connection statistics
     */
    std::map<std::string, int> getPeerConnectionStatistics(const std::string& peer_ip, int peer_port);
    
    /**
     * Get connection quality score
     * @param connection_id Connection ID
     * @return Quality score (0.0 to 1.0)
     */
    double getConnectionQualityScore(const std::string& connection_id);
    
    /**
     * Get peer reliability score
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Reliability score (0.0 to 1.0)
     */
    double getPeerReliabilityScore(const std::string& peer_ip, int peer_port);
    
    /**
     * Test connection to a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param timeout Test timeout
     * @return true if connection test was successful
     */
    bool testConnection(const std::string& peer_ip, 
                       int peer_port, 
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    /**
     * Get connection recommendations
     * @return Vector of connection recommendations
     */
    std::vector<std::string> getConnectionRecommendations();
};

/**
 * Direct peer connector factory for different connection strategies
 */
class DirectPeerConnectorFactory {
public:
    enum class ConnectionStrategy {
        PERFORMANCE,    // Optimized for performance
        RELIABILITY,    // Optimized for reliability
        BALANCED,       // Balanced performance and reliability
        CUSTOM          // Custom configuration
    };
    
    static std::unique_ptr<DirectPeerConnector> createConnector(
        ConnectionStrategy strategy,
        const DirectPeerConnector::ConnectionConfig& config = DirectPeerConnector::ConnectionConfig{}
    );
    
    static DirectPeerConnector::ConnectionConfig getDefaultConfig(ConnectionStrategy strategy);
};

/**
 * RAII connection wrapper for automatic connection management
 */
class ConnectionGuard {
private:
    std::shared_ptr<DirectPeerConnector> connector_;
    std::string connection_id_;
    bool closed_;

public:
    ConnectionGuard(std::shared_ptr<DirectPeerConnector> connector,
                   const std::string& peer_ip,
                   int peer_port,
                   const std::string& peer_id = "",
                   DirectPeerConnector::ConnectionType type = DirectPeerConnector::ConnectionType::TCP);
    
    ~ConnectionGuard();
    
    /**
     * Send data through the connection
     * @param data Data to send
     * @param size Data size
     * @return Number of bytes sent
     */
    ssize_t sendData(const void* data, size_t size);
    
    /**
     * Receive data from the connection
     * @param buffer Buffer to receive data
     * @param size Buffer size
     * @return Number of bytes received
     */
    ssize_t receiveData(void* buffer, size_t size);
    
    /**
     * Send a message through the connection
     * @param message Message to send
     * @return Number of bytes sent
     */
    ssize_t sendMessage(const std::string& message);
    
    /**
     * Receive a message from the connection
     * @param max_size Maximum message size
     * @return Received message
     */
    std::string receiveMessage(size_t max_size = 4096);
    
    /**
     * Close the connection
     */
    void close();
    
    /**
     * Get connection ID
     * @return Connection ID
     */
    const std::string& getConnectionId() const;
    
    /**
     * Check if connection is active
     * @return true if active
     */
    bool isActive();
    
    /**
     * Get connection information
     * @return Connection information
     */
    std::shared_ptr<DirectPeerConnector::ConnectionInfo> getConnectionInfo();
};

/**
 * Connection analyzer for pattern detection and optimization
 */
class ConnectionAnalyzer {
private:
    std::shared_ptr<DirectPeerConnector> connector_;
    
public:
    ConnectionAnalyzer(std::shared_ptr<DirectPeerConnector> connector);
    
    /**
     * Analyze connection patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzeConnectionPatterns();
    
    /**
     * Detect connection clusters
     * @param time_window Time window for clustering
     * @return Vector of connection clusters
     */
    std::vector<std::vector<std::string>> detectConnectionClusters(
        std::chrono::minutes time_window = std::chrono::minutes(5)
    );
    
    /**
     * Predict connection success probability
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Success probability (0.0 to 1.0)
     */
    double predictConnectionSuccess(const std::string& peer_ip, int peer_port);
    
    /**
     * Get connection optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getOptimizationRecommendations();
    
    /**
     * Analyze connection performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzePerformanceTrends();
    
    /**
     * Get connection health score
     * @return Health score (0.0 to 1.0)
     */
    double getConnectionHealthScore();
};

} // namespace dht_crawler
