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
#include <cstring>
#include <cstdint>

namespace dht_crawler {

/**
 * BitTorrent protocol implementation for protocol handling
 * Provides BitTorrent handshake implementation, protocol message parsing, and protocol state machine
 */
class BitTorrentProtocol {
public:
    enum class ProtocolState {
        DISCONNECTED,   // Not connected
        CONNECTING,     // Connection in progress
        HANDSHAKING,    // Handshake in progress
        CONNECTED,      // Successfully connected and handshaked
        FAILED,         // Protocol failed
        TIMEOUT,        // Protocol timed out
        CLOSED          // Protocol closed
    };

    enum class MessageType {
        HANDSHAKE,      // Handshake message
        KEEP_ALIVE,     // Keep-alive message
        CHOKE,          // Choke message
        UNCHOKE,        // Unchoke message
        INTERESTED,     // Interested message
        NOT_INTERESTED, // Not interested message
        HAVE,           // Have message
        BITFIELD,       // Bitfield message
        REQUEST,        // Request message
        PIECE,          // Piece message
        CANCEL,         // Cancel message
        PORT,           // Port message
        EXTENDED,       // Extended message (BEP 9)
        UNKNOWN         // Unknown message type
    };

    enum class ExtensionType {
        UT_METADATA,    // UT_Metadata extension
        UT_PEX,         // UT_PEX extension
        LT_EXTENDED,    // LT_Extended extension
        UNKNOWN         // Unknown extension type
    };

    struct ProtocolConfig {
        std::chrono::milliseconds handshake_timeout{10000};     // 10 seconds
        std::chrono::milliseconds message_timeout{5000};         // 5 seconds
        std::chrono::milliseconds keep_alive_interval{120000};  // 2 minutes
        std::chrono::milliseconds keep_alive_timeout{300000};    // 5 minutes
        bool enable_keep_alive = true;                         // Enable keep-alive
        bool enable_extension_protocol = true;                 // Enable BEP 9 extensions
        bool enable_ut_metadata = true;                        // Enable UT_Metadata
        bool enable_ut_pex = true;                            // Enable UT_PEX
        bool enable_lt_extended = true;                        // Enable LT_Extended
        int max_message_size = 16 * 1024 * 1024;              // 16MB max message size
        bool strict_protocol_validation = true;               // Strict protocol validation
        std::string peer_id_prefix = "-DC0001-";              // Peer ID prefix
    };

    struct HandshakeInfo {
        std::string protocol_string;   // "BitTorrent protocol"
        std::vector<uint8_t> reserved; // 8 reserved bytes
        std::string info_hash;         // 20-byte info hash
        std::string peer_id;           // 20-byte peer ID
        bool is_valid = false;
        std::string error_message;
    };

    struct MessageInfo {
        MessageType type;
        uint32_t length;
        std::vector<uint8_t> payload;
        std::chrono::steady_clock::time_point timestamp;
        bool is_valid = false;
        std::string error_message;
    };

    struct ExtensionInfo {
        ExtensionType type;
        std::string name;
        int message_id;
        bool is_supported = false;
        std::map<std::string, std::string> capabilities;
    };

    struct ProtocolSession {
        std::string session_id;
        std::string peer_ip;
        int peer_port;
        std::string peer_id;
        std::string info_hash;
        ProtocolState state;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_activity;
        
        // Handshake information
        HandshakeInfo handshake_info;
        bool handshake_sent;
        bool handshake_received;
        std::chrono::steady_clock::time_point handshake_sent_time;
        std::chrono::steady_clock::time_point handshake_received_time;
        
        // Extension information
        std::map<ExtensionType, ExtensionInfo> extensions;
        bool extension_protocol_supported;
        std::map<std::string, int> extension_message_ids;
        
        // Message tracking
        std::vector<MessageInfo> sent_messages;
        std::vector<MessageInfo> received_messages;
        std::map<MessageType, int> message_counts;
        
        // Protocol statistics
        int total_messages_sent;
        int total_messages_received;
        size_t total_bytes_sent;
        size_t total_bytes_received;
        std::chrono::milliseconds avg_message_time;
        
        // Error tracking
        std::vector<std::string> protocol_errors;
        std::chrono::steady_clock::time_point last_error_time;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct ProtocolStatistics {
        int total_sessions = 0;
        int successful_sessions = 0;
        int failed_sessions = 0;
        int handshake_failures = 0;
        int protocol_violations = 0;
        int message_parse_errors = 0;
        std::chrono::milliseconds avg_handshake_time;
        std::chrono::milliseconds avg_session_duration;
        std::map<ProtocolState, int> sessions_by_state;
        std::map<MessageType, int> messages_by_type;
        std::map<ExtensionType, int> extensions_by_type;
        std::map<std::string, int> sessions_by_peer;
        double protocol_success_rate = 0.0;
        double handshake_success_rate = 0.0;
        double extension_success_rate = 0.0;
    };

private:
    ProtocolConfig config_;
    std::map<std::string, std::shared_ptr<ProtocolSession>> active_sessions_;
    std::mutex sessions_mutex_;
    
    std::map<std::string, std::vector<ProtocolSession>> session_history_;
    std::mutex history_mutex_;
    
    ProtocolStatistics stats_;
    std::mutex stats_mutex_;
    
    // Background processing
    std::thread protocol_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Internal methods
    void protocolMonitorLoop();
    void monitorSessions();
    void cleanupExpiredSessions();
    void updateStatistics(const ProtocolSession& session, bool success);
    std::string generateSessionId();
    std::string generatePeerId();
    std::string messageTypeToString(MessageType type);
    std::string extensionTypeToString(ExtensionType type);
    std::string protocolStateToString(ProtocolState state);
    bool validateHandshake(const HandshakeInfo& handshake);
    bool validateMessage(const MessageInfo& message);
    std::vector<uint8_t> serializeHandshake(const HandshakeInfo& handshake);
    HandshakeInfo parseHandshake(const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeMessage(const MessageInfo& message);
    MessageInfo parseMessage(const std::vector<uint8_t>& data);
    bool isKeepAliveMessage(const std::vector<uint8_t>& data);
    bool isHandshakeMessage(const std::vector<uint8_t>& data);
    uint32_t calculateMessageLength(const std::vector<uint8_t>& data);
    MessageType getMessageType(const std::vector<uint8_t>& data);
    std::vector<uint8_t> createKeepAliveMessage();
    std::vector<uint8_t> createChokeMessage();
    std::vector<uint8_t> createUnchokeMessage();
    std::vector<uint8_t> createInterestedMessage();
    std::vector<uint8_t> createNotInterestedMessage();
    std::vector<uint8_t> createHaveMessage(uint32_t piece_index);
    std::vector<uint8_t> createBitfieldMessage(const std::vector<bool>& bitfield);
    std::vector<uint8_t> createRequestMessage(uint32_t piece_index, uint32_t offset, uint32_t length);
    std::vector<uint8_t> createPieceMessage(uint32_t piece_index, uint32_t offset, const std::vector<uint8_t>& data);
    std::vector<uint8_t> createCancelMessage(uint32_t piece_index, uint32_t offset, uint32_t length);
    std::vector<uint8_t> createPortMessage(uint16_t port);
    std::vector<uint8_t> createExtendedMessage(int message_id, const std::vector<uint8_t>& payload);

public:
    BitTorrentProtocol(const ProtocolConfig& config = ProtocolConfig{});
    ~BitTorrentProtocol();
    
    /**
     * Start a new protocol session
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param info_hash Info hash
     * @param peer_id Peer ID (optional)
     * @return Session ID
     */
    std::string startSession(const std::string& peer_ip,
                            int peer_port,
                            const std::string& info_hash,
                            const std::string& peer_id = "");
    
    /**
     * Perform handshake
     * @param session_id Session ID
     * @param info_hash Info hash
     * @param peer_id Peer ID
     * @return true if handshake was successful
     */
    bool performHandshake(const std::string& session_id,
                         const std::string& info_hash,
                         const std::string& peer_id);
    
    /**
     * Process received handshake
     * @param session_id Session ID
     * @param handshake_data Handshake data
     * @return true if handshake was processed successfully
     */
    bool processHandshake(const std::string& session_id,
                         const std::vector<uint8_t>& handshake_data);
    
    /**
     * Send a message
     * @param session_id Session ID
     * @param message_type Message type
     * @param payload Message payload
     * @return true if message was sent successfully
     */
    bool sendMessage(const std::string& session_id,
                    MessageType message_type,
                    const std::vector<uint8_t>& payload = {});
    
    /**
     * Process received message
     * @param session_id Session ID
     * @param message_data Message data
     * @return Message information
     */
    MessageInfo processMessage(const std::string& session_id,
                              const std::vector<uint8_t>& message_data);
    
    /**
     * Send keep-alive message
     * @param session_id Session ID
     * @return true if message was sent successfully
     */
    bool sendKeepAlive(const std::string& session_id);
    
    /**
     * Send choke message
     * @param session_id Session ID
     * @return true if message was sent successfully
     */
    bool sendChoke(const std::string& session_id);
    
    /**
     * Send unchoke message
     * @param session_id Session ID
     * @return true if message was sent successfully
     */
    bool sendUnchoke(const std::string& session_id);
    
    /**
     * Send interested message
     * @param session_id Session ID
     * @return true if message was sent successfully
     */
    bool sendInterested(const std::string& session_id);
    
    /**
     * Send not interested message
     * @param session_id Session ID
     * @return true if message was sent successfully
     */
    bool sendNotInterested(const std::string& session_id);
    
    /**
     * Send have message
     * @param session_id Session ID
     * @param piece_index Piece index
     * @return true if message was sent successfully
     */
    bool sendHave(const std::string& session_id, uint32_t piece_index);
    
    /**
     * Send bitfield message
     * @param session_id Session ID
     * @param bitfield Bitfield data
     * @return true if message was sent successfully
     */
    bool sendBitfield(const std::string& session_id, const std::vector<bool>& bitfield);
    
    /**
     * Send request message
     * @param session_id Session ID
     * @param piece_index Piece index
     * @param offset Piece offset
     * @param length Request length
     * @return true if message was sent successfully
     */
    bool sendRequest(const std::string& session_id,
                    uint32_t piece_index,
                    uint32_t offset,
                    uint32_t length);
    
    /**
     * Send piece message
     * @param session_id Session ID
     * @param piece_index Piece index
     * @param offset Piece offset
     * @param data Piece data
     * @return true if message was sent successfully
     */
    bool sendPiece(const std::string& session_id,
                   uint32_t piece_index,
                   uint32_t offset,
                   const std::vector<uint8_t>& data);
    
    /**
     * Send cancel message
     * @param session_id Session ID
     * @param piece_index Piece index
     * @param offset Piece offset
     * @param length Cancel length
     * @return true if message was sent successfully
     */
    bool sendCancel(const std::string& session_id,
                   uint32_t piece_index,
                   uint32_t offset,
                   uint32_t length);
    
    /**
     * Send port message
     * @param session_id Session ID
     * @param port Port number
     * @return true if message was sent successfully
     */
    bool sendPort(const std::string& session_id, uint16_t port);
    
    /**
     * Send extended message
     * @param session_id Session ID
     * @param message_id Extended message ID
     * @param payload Extended message payload
     * @return true if message was sent successfully
     */
    bool sendExtended(const std::string& session_id,
                     int message_id,
                     const std::vector<uint8_t>& payload);
    
    /**
     * Close a session
     * @param session_id Session ID
     * @return true if session was closed successfully
     */
    bool closeSession(const std::string& session_id);
    
    /**
     * Get session information
     * @param session_id Session ID
     * @return Session information or nullptr if not found
     */
    std::shared_ptr<ProtocolSession> getSession(const std::string& session_id);
    
    /**
     * Get sessions by state
     * @param state Protocol state
     * @return Vector of session information
     */
    std::vector<std::shared_ptr<ProtocolSession>> getSessionsByState(ProtocolState state);
    
    /**
     * Get active sessions
     * @return Vector of active session information
     */
    std::vector<std::shared_ptr<ProtocolSession>> getActiveSessions();
    
    /**
     * Get session history for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of historical session information
     */
    std::vector<ProtocolSession> getSessionHistory(const std::string& peer_ip, int peer_port);
    
    /**
     * Get protocol statistics
     * @return Current protocol statistics
     */
    ProtocolStatistics getStatistics();
    
    /**
     * Reset protocol statistics
     */
    void resetStatistics();
    
    /**
     * Update protocol configuration
     * @param config New protocol configuration
     */
    void updateConfig(const ProtocolConfig& config);
    
    /**
     * Enable or disable extension protocol
     * @param enable true to enable extension protocol
     */
    void setExtensionProtocolEnabled(bool enable);
    
    /**
     * Check if extension protocol is enabled
     * @return true if extension protocol is enabled
     */
    bool isExtensionProtocolEnabled();
    
    /**
     * Enable or disable specific extension
     * @param extension_type Extension type
     * @param enable true to enable extension
     */
    void setExtensionEnabled(ExtensionType extension_type, bool enable);
    
    /**
     * Check if specific extension is enabled
     * @param extension_type Extension type
     * @return true if extension is enabled
     */
    bool isExtensionEnabled(ExtensionType extension_type);
    
    /**
     * Get protocol success rate
     * @return Protocol success rate (0.0 to 1.0)
     */
    double getProtocolSuccessRate();
    
    /**
     * Get handshake success rate
     * @return Handshake success rate (0.0 to 1.0)
     */
    double getHandshakeSuccessRate();
    
    /**
     * Get extension success rate
     * @return Extension success rate (0.0 to 1.0)
     */
    double getExtensionSuccessRate();
    
    /**
     * Get average handshake time
     * @return Average handshake time
     */
    std::chrono::milliseconds getAverageHandshakeTime();
    
    /**
     * Get session count
     * @return Total number of active sessions
     */
    int getSessionCount();
    
    /**
     * Get session count by state
     * @param state Protocol state
     * @return Number of sessions with the state
     */
    int getSessionCount(ProtocolState state);
    
    /**
     * Start protocol monitor
     */
    void start();
    
    /**
     * Stop protocol monitor
     */
    void stop();
    
    /**
     * Check if protocol is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export session data to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportSessionData(const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get protocol health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear session history
     */
    void clearSessionHistory();
    
    /**
     * Clear active sessions
     */
    void clearActiveSessions();
    
    /**
     * Force cleanup of expired sessions
     */
    void forceCleanup();
    
    /**
     * Get peer protocol statistics
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Peer protocol statistics
     */
    std::map<std::string, int> getPeerProtocolStatistics(const std::string& peer_ip, int peer_port);
    
    /**
     * Get message statistics by type
     * @return Message statistics by type
     */
    std::map<MessageType, int> getMessageStatisticsByType();
    
    /**
     * Get extension statistics by type
     * @return Extension statistics by type
     */
    std::map<ExtensionType, int> getExtensionStatisticsByType();
    
    /**
     * Test protocol with a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param info_hash Info hash
     * @param timeout Test timeout
     * @return true if protocol test was successful
     */
    bool testProtocol(const std::string& peer_ip,
                     int peer_port,
                     const std::string& info_hash,
                     std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
    
    /**
     * Get protocol recommendations
     * @return Vector of protocol recommendations
     */
    std::vector<std::string> getProtocolRecommendations();
};

/**
 * BitTorrent protocol factory for different protocol strategies
 */
class BitTorrentProtocolFactory {
public:
    enum class ProtocolStrategy {
        STRICT,      // Strict protocol compliance
        MODERATE,    // Moderate protocol compliance
        LENIENT,     // Lenient protocol compliance
        CUSTOM       // Custom protocol configuration
    };
    
    static std::unique_ptr<BitTorrentProtocol> createProtocol(
        ProtocolStrategy strategy,
        const BitTorrentProtocol::ProtocolConfig& config = BitTorrentProtocol::ProtocolConfig{}
    );
    
    static BitTorrentProtocol::ProtocolConfig getDefaultConfig(ProtocolStrategy strategy);
};

/**
 * RAII protocol session wrapper for automatic session management
 */
class ProtocolSessionGuard {
private:
    std::shared_ptr<BitTorrentProtocol> protocol_;
    std::string session_id_;
    bool closed_;

public:
    ProtocolSessionGuard(std::shared_ptr<BitTorrentProtocol> protocol,
                        const std::string& peer_ip,
                        int peer_port,
                        const std::string& info_hash,
                        const std::string& peer_id = "");
    
    ~ProtocolSessionGuard();
    
    /**
     * Perform handshake
     * @param info_hash Info hash
     * @param peer_id Peer ID
     * @return true if handshake was successful
     */
    bool performHandshake(const std::string& info_hash, const std::string& peer_id);
    
    /**
     * Send a message
     * @param message_type Message type
     * @param payload Message payload
     * @return true if message was sent successfully
     */
    bool sendMessage(BitTorrentProtocol::MessageType message_type,
                    const std::vector<uint8_t>& payload = {});
    
    /**
     * Process received message
     * @param message_data Message data
     * @return Message information
     */
    BitTorrentProtocol::MessageInfo processMessage(const std::vector<uint8_t>& message_data);
    
    /**
     * Close the session
     */
    void close();
    
    /**
     * Get session ID
     * @return Session ID
     */
    const std::string& getSessionId() const;
    
    /**
     * Get session information
     * @return Session information
     */
    std::shared_ptr<BitTorrentProtocol::ProtocolSession> getSessionInfo();
    
    /**
     * Check if session is active
     * @return true if active
     */
    bool isActive();
};

/**
 * Protocol analyzer for pattern detection and optimization
 */
class ProtocolAnalyzer {
private:
    std::shared_ptr<BitTorrentProtocol> protocol_;
    
public:
    ProtocolAnalyzer(std::shared_ptr<BitTorrentProtocol> protocol);
    
    /**
     * Analyze protocol patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzeProtocolPatterns();
    
    /**
     * Detect protocol issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectProtocolIssues();
    
    /**
     * Analyze peer compatibility
     * @return Map of peer IPs to compatibility scores
     */
    std::map<std::string, double> analyzePeerCompatibility();
    
    /**
     * Get protocol optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getOptimizationRecommendations();
    
    /**
     * Analyze protocol performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzePerformanceTrends();
    
    /**
     * Get protocol health score
     * @return Health score (0.0 to 1.0)
     */
    double getProtocolHealthScore();
};

} // namespace dht_crawler
