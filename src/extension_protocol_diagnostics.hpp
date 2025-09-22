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
#include <fstream>

namespace dht_crawler {

/**
 * Extension Protocol Diagnostics for BEP 9 debugging
 * Provides comprehensive debugging tools for BEP 9 extension protocol negotiation issues
 */
class ExtensionProtocolDiagnostics {
public:
    enum class ExtensionState {
        NOT_STARTED,        // Extension protocol not started
        HANDSHAKE_SENT,     // Handshake message sent
        HANDSHAKE_RECEIVED, // Handshake message received
        NEGOTIATING,        // Capability negotiation in progress
        NEGOTIATED,         // Capabilities negotiated successfully
        FAILED,             // Extension protocol failed
        TIMEOUT,            // Extension protocol timed out
        DISCONNECTED        // Connection disconnected during negotiation
    };

    enum class ExtensionMessageType {
        HANDSHAKE,          // Extension handshake message
        EXTENDED,            // Extended message
        UT_METADATA,         // UT_Metadata extension message
        UT_PEX,             // UT_PEX extension message
        LT_EXTENDED,         // LT_Extended message
        UNKNOWN              // Unknown message type
    };

    enum class ErrorType {
        HANDSHAKE_FAILED,    // Handshake failed
        NEGOTIATION_FAILED,  // Capability negotiation failed
        MESSAGE_PARSE_ERROR, // Message parsing error
        TIMEOUT_ERROR,       // Timeout error
        PROTOCOL_ERROR,      // Protocol error
        CONNECTION_ERROR,    // Connection error
        UNKNOWN_ERROR        // Unknown error
    };

    struct ExtensionConfig {
        std::chrono::milliseconds handshake_timeout{10000};    // 10 seconds
        std::chrono::milliseconds negotiation_timeout{15000};   // 15 seconds
        std::chrono::milliseconds message_timeout{5000};        // 5 seconds
        bool enable_detailed_logging = true;
        bool enable_message_logging = true;
        bool enable_state_logging = true;
        bool enable_error_logging = true;
        bool enable_performance_logging = true;
        std::string log_file_path = "extension_protocol.log";
        size_t max_log_size = 10 * 1024 * 1024; // 10MB
        int max_log_files = 5;
    };

    struct ExtensionSession {
        std::string session_id;
        std::string peer_ip;
        int peer_port;
        std::string peer_id;
        ExtensionState current_state;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_activity;
        
        // Handshake information
        bool handshake_sent;
        bool handshake_received;
        std::chrono::steady_clock::time_point handshake_sent_time;
        std::chrono::steady_clock::time_point handshake_received_time;
        
        // Capability information
        std::map<std::string, int> supported_extensions;
        std::map<std::string, int> negotiated_extensions;
        bool ut_metadata_supported;
        bool ut_pex_supported;
        bool lt_extended_supported;
        
        // Message tracking
        std::vector<ExtensionMessageType> sent_messages;
        std::vector<ExtensionMessageType> received_messages;
        std::map<ExtensionMessageType, int> message_counts;
        
        // Error tracking
        std::vector<ErrorType> errors;
        std::vector<std::string> error_messages;
        std::chrono::steady_clock::time_point last_error_time;
        
        // Performance metrics
        std::chrono::milliseconds handshake_duration;
        std::chrono::milliseconds negotiation_duration;
        std::chrono::milliseconds total_duration;
        int retry_count;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct DiagnosticStatistics {
        int total_sessions = 0;
        int successful_sessions = 0;
        int failed_sessions = 0;
        int timeout_sessions = 0;
        int handshake_failures = 0;
        int negotiation_failures = 0;
        int message_parse_errors = 0;
        int connection_errors = 0;
        std::chrono::milliseconds avg_handshake_time;
        std::chrono::milliseconds avg_negotiation_time;
        std::chrono::milliseconds avg_total_time;
        std::map<ExtensionState, int> sessions_by_state;
        std::map<ErrorType, int> errors_by_type;
        std::map<std::string, int> errors_by_peer;
        std::map<ExtensionMessageType, int> messages_by_type;
        double success_rate = 0.0;
        double ut_metadata_success_rate = 0.0;
        double ut_pex_success_rate = 0.0;
    };

private:
    ExtensionConfig config_;
    std::map<std::string, std::shared_ptr<ExtensionSession>> active_sessions_;
    std::mutex sessions_mutex_;
    
    std::map<std::string, std::vector<ExtensionSession>> session_history_;
    std::mutex history_mutex_;
    
    DiagnosticStatistics stats_;
    std::mutex stats_mutex_;
    
    // Logging
    std::ofstream log_file_;
    std::mutex log_mutex_;
    bool logging_enabled_;
    
    // Background processing
    std::thread cleanup_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable cleanup_condition_;
    
    // Internal methods
    void cleanupExpiredSessions();
    void updateStatistics(const ExtensionSession& session, bool success);
    void logMessage(const std::string& level, const std::string& message);
    void rotateLogFile();
    std::string stateToString(ExtensionState state);
    std::string messageTypeToString(ExtensionMessageType type);
    std::string errorTypeToString(ErrorType type);
    std::string generateSessionId();
    void cleanupLoop();

public:
    ExtensionProtocolDiagnostics(const ExtensionConfig& config = ExtensionConfig{});
    ~ExtensionProtocolDiagnostics();
    
    /**
     * Start a new extension protocol session
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @return Session ID
     */
    std::string startSession(const std::string& peer_ip, 
                           int peer_port, 
                           const std::string& peer_id);
    
    /**
     * Update session state
     * @param session_id Session ID
     * @param new_state New state
     * @return true if update was successful
     */
    bool updateSessionState(const std::string& session_id, ExtensionState new_state);
    
    /**
     * Log handshake sent
     * @param session_id Session ID
     * @return true if logged successfully
     */
    bool logHandshakeSent(const std::string& session_id);
    
    /**
     * Log handshake received
     * @param session_id Session ID
     * @param extensions Supported extensions
     * @return true if logged successfully
     */
    bool logHandshakeReceived(const std::string& session_id, 
                            const std::map<std::string, int>& extensions);
    
    /**
     * Log capability negotiation
     * @param session_id Session ID
     * @param negotiated_extensions Negotiated extensions
     * @return true if logged successfully
     */
    bool logCapabilityNegotiation(const std::string& session_id,
                                 const std::map<std::string, int>& negotiated_extensions);
    
    /**
     * Log extension message
     * @param session_id Session ID
     * @param message_type Message type
     * @param is_sent Whether message was sent or received
     * @param message_data Message data (optional)
     * @return true if logged successfully
     */
    bool logExtensionMessage(const std::string& session_id,
                           ExtensionMessageType message_type,
                           bool is_sent,
                           const std::string& message_data = "");
    
    /**
     * Log extension error
     * @param session_id Session ID
     * @param error_type Error type
     * @param error_message Error message
     * @return true if logged successfully
     */
    bool logExtensionError(const std::string& session_id,
                          ErrorType error_type,
                          const std::string& error_message);
    
    /**
     * Complete session successfully
     * @param session_id Session ID
     * @return true if session was completed successfully
     */
    bool completeSession(const std::string& session_id);
    
    /**
     * Fail session
     * @param session_id Session ID
     * @param error_type Error type
     * @param error_message Error message
     * @return true if session was failed successfully
     */
    bool failSession(const std::string& session_id,
                    ErrorType error_type,
                    const std::string& error_message);
    
    /**
     * Get session information
     * @param session_id Session ID
     * @return Session information or nullptr if not found
     */
    std::shared_ptr<ExtensionSession> getSession(const std::string& session_id);
    
    /**
     * Get active sessions
     * @return Vector of active session information
     */
    std::vector<std::shared_ptr<ExtensionSession>> getActiveSessions();
    
    /**
     * Get sessions by state
     * @param state Extension state
     * @return Vector of session information
     */
    std::vector<std::shared_ptr<ExtensionSession>> getSessionsByState(ExtensionState state);
    
    /**
     * Get sessions by peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of session information
     */
    std::vector<std::shared_ptr<ExtensionSession>> getSessionsByPeer(const std::string& peer_ip, int peer_port);
    
    /**
     * Get session history for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of historical session information
     */
    std::vector<ExtensionSession> getSessionHistory(const std::string& peer_ip, int peer_port);
    
    /**
     * Get diagnostic statistics
     * @return Current diagnostic statistics
     */
    DiagnosticStatistics getStatistics();
    
    /**
     * Reset diagnostic statistics
     */
    void resetStatistics();
    
    /**
     * Update diagnostic configuration
     * @param config New diagnostic configuration
     */
    void updateConfig(const ExtensionConfig& config);
    
    /**
     * Enable or disable logging
     * @param enable true to enable logging
     */
    void setLoggingEnabled(bool enable);
    
    /**
     * Check if logging is enabled
     * @return true if logging is enabled
     */
    bool isLoggingEnabled();
    
    /**
     * Get log file path
     * @return Log file path
     */
    std::string getLogFilePath();
    
    /**
     * Set log file path
     * @param file_path New log file path
     */
    void setLogFilePath(const std::string& file_path);
    
    /**
     * Get success rate
     * @return Success rate (0.0 to 1.0)
     */
    double getSuccessRate();
    
    /**
     * Get UT_Metadata success rate
     * @return UT_Metadata success rate (0.0 to 1.0)
     */
    double getUTMetadataSuccessRate();
    
    /**
     * Get UT_PEX success rate
     * @return UT_PEX success rate (0.0 to 1.0)
     */
    double getUTPexSuccessRate();
    
    /**
     * Get average handshake time
     * @return Average handshake time
     */
    std::chrono::milliseconds getAverageHandshakeTime();
    
    /**
     * Get average negotiation time
     * @return Average negotiation time
     */
    std::chrono::milliseconds getAverageNegotiationTime();
    
    /**
     * Get error rate by type
     * @param error_type Error type
     * @return Error rate (0.0 to 1.0)
     */
    double getErrorRate(ErrorType error_type);
    
    /**
     * Get error count by type
     * @param error_type Error type
     * @return Error count
     */
    int getErrorCount(ErrorType error_type);
    
    /**
     * Start diagnostics
     */
    void start();
    
    /**
     * Stop diagnostics
     */
    void stop();
    
    /**
     * Check if diagnostics is running
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
     * Get diagnostics health status
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
     * Get session count
     * @return Total number of active sessions
     */
    int getSessionCount();
    
    /**
     * Get session count by state
     * @param state Extension state
     * @return Number of sessions with the state
     */
    int getSessionCount(ExtensionState state);
    
    /**
     * Get error patterns
     * @return Map of error patterns and their frequencies
     */
    std::map<std::string, int> getErrorPatterns();
    
    /**
     * Get peer compatibility information
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Peer compatibility information
     */
    std::map<std::string, std::string> getPeerCompatibility(const std::string& peer_ip, int peer_port);
    
    /**
     * Detect extension protocol issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectIssues();
    
    /**
     * Get extension protocol recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getRecommendations();
};

/**
 * Extension protocol diagnostics factory for different diagnostic strategies
 */
class ExtensionProtocolDiagnosticsFactory {
public:
    enum class DiagnosticStrategy {
        DETAILED,    // Detailed diagnostics with extensive logging
        MODERATE,    // Moderate diagnostics with balanced logging
        MINIMAL,     // Minimal diagnostics with basic logging
        CUSTOM       // Custom diagnostic configuration
    };
    
    static std::unique_ptr<ExtensionProtocolDiagnostics> createDiagnostics(
        DiagnosticStrategy strategy,
        const ExtensionProtocolDiagnostics::ExtensionConfig& config = ExtensionProtocolDiagnostics::ExtensionConfig{}
    );
    
    static ExtensionProtocolDiagnostics::ExtensionConfig getDefaultConfig(DiagnosticStrategy strategy);
};

/**
 * RAII extension session wrapper for automatic session management
 */
class ExtensionSessionGuard {
private:
    std::shared_ptr<ExtensionProtocolDiagnostics> diagnostics_;
    std::string session_id_;
    bool completed_;

public:
    ExtensionSessionGuard(std::shared_ptr<ExtensionProtocolDiagnostics> diagnostics,
                          const std::string& peer_ip,
                          int peer_port,
                          const std::string& peer_id);
    
    ~ExtensionSessionGuard();
    
    /**
     * Update session state
     * @param new_state New state
     */
    void updateState(ExtensionProtocolDiagnostics::ExtensionState new_state);
    
    /**
     * Log handshake sent
     */
    void logHandshakeSent();
    
    /**
     * Log handshake received
     * @param extensions Supported extensions
     */
    void logHandshakeReceived(const std::map<std::string, int>& extensions);
    
    /**
     * Log capability negotiation
     * @param negotiated_extensions Negotiated extensions
     */
    void logCapabilityNegotiation(const std::map<std::string, int>& negotiated_extensions);
    
    /**
     * Log extension message
     * @param message_type Message type
     * @param is_sent Whether message was sent or received
     * @param message_data Message data
     */
    void logMessage(ExtensionProtocolDiagnostics::ExtensionMessageType message_type,
                   bool is_sent,
                   const std::string& message_data = "");
    
    /**
     * Log extension error
     * @param error_type Error type
     * @param error_message Error message
     */
    void logError(ExtensionProtocolDiagnostics::ErrorType error_type,
                  const std::string& error_message);
    
    /**
     * Complete session successfully
     */
    void complete();
    
    /**
     * Fail session
     * @param error_type Error type
     * @param error_message Error message
     */
    void fail(ExtensionProtocolDiagnostics::ErrorType error_type,
              const std::string& error_message);
    
    /**
     * Get session ID
     * @return Session ID
     */
    const std::string& getSessionId() const;
    
    /**
     * Get session information
     * @return Session information
     */
    std::shared_ptr<ExtensionProtocolDiagnostics::ExtensionSession> getSessionInfo();
};

/**
 * Extension protocol analyzer for pattern detection
 */
class ExtensionProtocolAnalyzer {
private:
    std::shared_ptr<ExtensionProtocolDiagnostics> diagnostics_;
    
public:
    ExtensionProtocolAnalyzer(std::shared_ptr<ExtensionProtocolDiagnostics> diagnostics);
    
    /**
     * Analyze extension protocol patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzePatterns();
    
    /**
     * Detect extension protocol issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectIssues();
    
    /**
     * Analyze peer compatibility
     * @return Map of peer IPs to compatibility scores
     */
    std::map<std::string, double> analyzePeerCompatibility();
    
    /**
     * Get extension protocol recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getRecommendations();
    
    /**
     * Analyze performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzePerformanceTrends();
    
    /**
     * Get extension protocol health score
     * @return Health score (0.0 to 1.0)
     */
    double getHealthScore();
};

} // namespace dht_crawler
