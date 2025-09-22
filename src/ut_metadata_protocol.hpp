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
#include <cstdint>

namespace dht_crawler {

/**
 * UT_Metadata protocol implementation for metadata fetching
 * Provides metadata piece request generation, response parsing, and metadata assembly
 */
class UTMetadataProtocol {
public:
    enum class MetadataState {
        NOT_STARTED,        // Metadata fetching not started
        REQUESTING,         // Requesting metadata pieces
        RECEIVING,          // Receiving metadata pieces
        ASSEMBLING,         // Assembling metadata pieces
        COMPLETED,          // Metadata fetching completed
        FAILED,             // Metadata fetching failed
        TIMEOUT,            // Metadata fetching timed out
        CANCELLED            // Metadata fetching cancelled
    };

    enum class MessageType {
        REQUEST,            // Request metadata piece
        DATA,               // Metadata piece data
        REJECT,             // Reject metadata piece request
        UNKNOWN              // Unknown message type
    };

    struct MetadataConfig {
        std::chrono::milliseconds piece_timeout{10000};      // 10 seconds per piece
        std::chrono::milliseconds total_timeout{120000};    // 2 minutes total
        size_t piece_size = 16384;                          // 16KB pieces
        int max_concurrent_requests = 3;                    // Max concurrent piece requests
        int max_retry_attempts = 3;                         // Max retry attempts per piece
        std::chrono::milliseconds retry_delay{1000};        // Retry delay
        double retry_backoff_multiplier = 2.0;              // Retry backoff multiplier
        bool enable_parallel_requests = true;                // Enable parallel piece requests
        bool enable_piece_validation = true;                // Enable piece validation
        bool enable_metadata_validation = true;              // Enable metadata validation
        size_t max_metadata_size = 10 * 1024 * 1024;        // 10MB max metadata size
    };

    struct MetadataPiece {
        int piece_index;
        std::vector<uint8_t> data;
        bool is_received;
        bool is_valid;
        std::chrono::steady_clock::time_point request_time;
        std::chrono::steady_clock::time_point receive_time;
        int retry_count;
        std::string error_message;
    };

    struct MetadataRequest {
        std::string request_id;
        std::string peer_ip;
        int peer_port;
        std::string peer_id;
        std::string info_hash;
        MetadataState state;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        
        // Metadata information
        size_t metadata_size;
        int total_pieces;
        std::vector<MetadataPiece> pieces;
        std::map<int, bool> piece_status; // piece_index -> is_received
        
        // Request tracking
        int pieces_requested;
        int pieces_received;
        int pieces_failed;
        int concurrent_requests;
        
        // Statistics
        std::chrono::milliseconds total_duration;
        std::chrono::milliseconds avg_piece_time;
        size_t total_bytes_received;
        
        // Error tracking
        std::vector<std::string> errors;
        std::chrono::steady_clock::time_point last_error_time;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct MetadataStatistics {
        int total_requests = 0;
        int successful_requests = 0;
        int failed_requests = 0;
        int timeout_requests = 0;
        int cancelled_requests = 0;
        int total_pieces_requested = 0;
        int total_pieces_received = 0;
        int total_pieces_failed = 0;
        std::chrono::milliseconds avg_request_duration;
        std::chrono::milliseconds avg_piece_duration;
        std::chrono::milliseconds max_request_duration;
        std::chrono::milliseconds min_request_duration;
        std::map<MetadataState, int> requests_by_state;
        std::map<std::string, int> requests_by_peer;
        std::map<std::string, int> errors_by_type;
        double success_rate = 0.0;
        double piece_success_rate = 0.0;
        size_t total_metadata_bytes = 0;
        double avg_metadata_size = 0.0;
    };

private:
    MetadataConfig config_;
    std::map<std::string, std::shared_ptr<MetadataRequest>> active_requests_;
    std::mutex requests_mutex_;
    
    std::map<std::string, std::vector<MetadataRequest>> request_history_;
    std::mutex history_mutex_;
    
    MetadataStatistics stats_;
    std::mutex stats_mutex_;
    
    // Background processing
    std::thread metadata_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Internal methods
    void metadataMonitorLoop();
    void monitorRequests();
    void cleanupExpiredRequests();
    void updateStatistics(const MetadataRequest& request, bool success);
    std::string generateRequestId();
    std::string metadataStateToString(MetadataState state);
    std::string messageTypeToString(MessageType type);
    int calculateTotalPieces(size_t metadata_size);
    std::vector<uint8_t> serializeRequestMessage(int piece_index);
    std::vector<uint8_t> serializeDataMessage(int piece_index, const std::vector<uint8_t>& data);
    std::vector<uint8_t> serializeRejectMessage(int piece_index);
    MessageType parseMessageType(const std::vector<uint8_t>& data);
    int parsePieceIndex(const std::vector<uint8_t>& data);
    std::vector<uint8_t> parsePieceData(const std::vector<uint8_t>& data);
    bool validateMetadataPiece(const MetadataPiece& piece);
    bool validateCompleteMetadata(const std::vector<uint8_t>& metadata);
    std::vector<uint8_t> assembleMetadata(const std::vector<MetadataPiece>& pieces);
    void requestMetadataPiece(std::shared_ptr<MetadataRequest> request, int piece_index);
    void processReceivedPiece(std::shared_ptr<MetadataRequest> request, int piece_index, const std::vector<uint8_t>& data);
    void processRejectedPiece(std::shared_ptr<MetadataRequest> request, int piece_index);
    bool isRequestComplete(const MetadataRequest& request);
    void completeRequest(std::shared_ptr<MetadataRequest> request, bool success, const std::string& error = "");

public:
    UTMetadataProtocol(const MetadataConfig& config = MetadataConfig{});
    ~UTMetadataProtocol();
    
    /**
     * Start metadata fetching from a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param info_hash Info hash
     * @param estimated_size Estimated metadata size (optional)
     * @return Request ID
     */
    std::string startMetadataRequest(const std::string& peer_ip,
                                    int peer_port,
                                    const std::string& peer_id,
                                    const std::string& info_hash,
                                    size_t estimated_size = 0);
    
    /**
     * Process received metadata message
     * @param request_id Request ID
     * @param message_data Message data
     * @return true if message was processed successfully
     */
    bool processMetadataMessage(const std::string& request_id,
                               const std::vector<uint8_t>& message_data);
    
    /**
     * Process metadata piece data
     * @param request_id Request ID
     * @param piece_index Piece index
     * @param piece_data Piece data
     * @return true if piece was processed successfully
     */
    bool processMetadataPiece(const std::string& request_id,
                             int piece_index,
                             const std::vector<uint8_t>& piece_data);
    
    /**
     * Process metadata piece rejection
     * @param request_id Request ID
     * @param piece_index Piece index
     * @return true if rejection was processed successfully
     */
    bool processMetadataRejection(const std::string& request_id, int piece_index);
    
    /**
     * Get metadata request information
     * @param request_id Request ID
     * @return Request information or nullptr if not found
     */
    std::shared_ptr<MetadataRequest> getRequest(const std::string& request_id);
    
    /**
     * Get requests by state
     * @param state Metadata state
     * @return Vector of request information
     */
    std::vector<std::shared_ptr<MetadataRequest>> getRequestsByState(MetadataState state);
    
    /**
     * Get active requests
     * @return Vector of active request information
     */
    std::vector<std::shared_ptr<MetadataRequest>> getActiveRequests();
    
    /**
     * Get request history for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of historical request information
     */
    std::vector<MetadataRequest> getRequestHistory(const std::string& peer_ip, int peer_port);
    
    /**
     * Cancel a metadata request
     * @param request_id Request ID
     * @return true if request was cancelled successfully
     */
    bool cancelRequest(const std::string& request_id);
    
    /**
     * Cancel all requests to a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Number of requests cancelled
     */
    int cancelRequestsToPeer(const std::string& peer_ip, int peer_port);
    
    /**
     * Check if request is active
     * @param request_id Request ID
     * @return true if request is active
     */
    bool isRequestActive(const std::string& request_id);
    
    /**
     * Check if request is completed
     * @param request_id Request ID
     * @return true if request is completed
     */
    bool isRequestCompleted(const std::string& request_id);
    
    /**
     * Get completed metadata
     * @param request_id Request ID
     * @return Completed metadata or empty vector if not available
     */
    std::vector<uint8_t> getCompletedMetadata(const std::string& request_id);
    
    /**
     * Get request progress
     * @param request_id Request ID
     * @return Progress percentage (0.0 to 1.0)
     */
    double getRequestProgress(const std::string& request_id);
    
    /**
     * Get remaining pieces
     * @param request_id Request ID
     * @return Vector of remaining piece indices
     */
    std::vector<int> getRemainingPieces(const std::string& request_id);
    
    /**
     * Get received pieces
     * @param request_id Request ID
     * @return Vector of received piece indices
     */
    std::vector<int> getReceivedPieces(const std::string& request_id);
    
    /**
     * Get failed pieces
     * @param request_id Request ID
     * @return Vector of failed piece indices
     */
    std::vector<int> getFailedPieces(const std::string& request_id);
    
    /**
     * Retry failed pieces
     * @param request_id Request ID
     * @return Number of pieces retried
     */
    int retryFailedPieces(const std::string& request_id);
    
    /**
     * Get metadata statistics
     * @return Current metadata statistics
     */
    MetadataStatistics getStatistics();
    
    /**
     * Reset metadata statistics
     */
    void resetStatistics();
    
    /**
     * Update metadata configuration
     * @param config New metadata configuration
     */
    void updateConfig(const MetadataConfig& config);
    
    /**
     * Enable or disable parallel requests
     * @param enable true to enable parallel requests
     */
    void setParallelRequestsEnabled(bool enable);
    
    /**
     * Check if parallel requests are enabled
     * @return true if parallel requests are enabled
     */
    bool isParallelRequestsEnabled();
    
    /**
     * Set maximum concurrent requests
     * @param max_requests Maximum concurrent requests
     */
    void setMaxConcurrentRequests(int max_requests);
    
    /**
     * Get maximum concurrent requests
     * @return Maximum concurrent requests
     */
    int getMaxConcurrentRequests();
    
    /**
     * Set piece size
     * @param piece_size Piece size in bytes
     */
    void setPieceSize(size_t piece_size);
    
    /**
     * Get piece size
     * @return Piece size in bytes
     */
    size_t getPieceSize();
    
    /**
     * Get metadata success rate
     * @return Metadata success rate (0.0 to 1.0)
     */
    double getMetadataSuccessRate();
    
    /**
     * Get piece success rate
     * @return Piece success rate (0.0 to 1.0)
     */
    double getPieceSuccessRate();
    
    /**
     * Get average request duration
     * @return Average request duration
     */
    std::chrono::milliseconds getAverageRequestDuration();
    
    /**
     * Get average piece duration
     * @return Average piece duration
     */
    std::chrono::milliseconds getAveragePieceDuration();
    
    /**
     * Get request count
     * @return Total number of active requests
     */
    int getRequestCount();
    
    /**
     * Get request count by state
     * @param state Metadata state
     * @return Number of requests with the state
     */
    int getRequestCount(MetadataState state);
    
    /**
     * Start metadata monitor
     */
    void start();
    
    /**
     * Stop metadata monitor
     */
    void stop();
    
    /**
     * Check if metadata protocol is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export request data to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportRequestData(const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get metadata protocol health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear request history
     */
    void clearRequestHistory();
    
    /**
     * Clear active requests
     */
    void clearActiveRequests();
    
    /**
     * Force cleanup of expired requests
     */
    void forceCleanup();
    
    /**
     * Get peer metadata statistics
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Peer metadata statistics
     */
    std::map<std::string, int> getPeerMetadataStatistics(const std::string& peer_ip, int peer_port);
    
    /**
     * Get metadata quality score
     * @param request_id Request ID
     * @return Quality score (0.0 to 1.0)
     */
    double getMetadataQualityScore(const std::string& request_id);
    
    /**
     * Get peer reliability score for metadata
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Reliability score (0.0 to 1.0)
     */
    double getPeerMetadataReliabilityScore(const std::string& peer_ip, int peer_port);
    
    /**
     * Test metadata protocol with a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param info_hash Info hash
     * @param timeout Test timeout
     * @return true if metadata test was successful
     */
    bool testMetadataProtocol(const std::string& peer_ip,
                             int peer_port,
                             const std::string& info_hash,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    
    /**
     * Get metadata protocol recommendations
     * @return Vector of metadata protocol recommendations
     */
    std::vector<std::string> getMetadataProtocolRecommendations();
};

/**
 * UT_Metadata protocol factory for different metadata strategies
 */
class UTMetadataProtocolFactory {
public:
    enum class MetadataStrategy {
        FAST,         // Fast metadata fetching with aggressive timeouts
        RELIABLE,     // Reliable metadata fetching with conservative timeouts
        BALANCED,     // Balanced speed and reliability
        CUSTOM        // Custom metadata configuration
    };
    
    static std::unique_ptr<UTMetadataProtocol> createProtocol(
        MetadataStrategy strategy,
        const UTMetadataProtocol::MetadataConfig& config = UTMetadataProtocol::MetadataConfig{}
    );
    
    static UTMetadataProtocol::MetadataConfig getDefaultConfig(MetadataStrategy strategy);
};

/**
 * RAII metadata request wrapper for automatic request management
 */
class MetadataRequestGuard {
private:
    std::shared_ptr<UTMetadataProtocol> protocol_;
    std::string request_id_;
    bool completed_;

public:
    MetadataRequestGuard(std::shared_ptr<UTMetadataProtocol> protocol,
                        const std::string& peer_ip,
                        int peer_port,
                        const std::string& peer_id,
                        const std::string& info_hash,
                        size_t estimated_size = 0);
    
    ~MetadataRequestGuard();
    
    /**
     * Process received metadata message
     * @param message_data Message data
     * @return true if message was processed successfully
     */
    bool processMessage(const std::vector<uint8_t>& message_data);
    
    /**
     * Get completed metadata
     * @return Completed metadata
     */
    std::vector<uint8_t> getCompletedMetadata();
    
    /**
     * Get request progress
     * @return Progress percentage (0.0 to 1.0)
     */
    double getProgress();
    
    /**
     * Cancel the request
     */
    void cancel();
    
    /**
     * Get request ID
     * @return Request ID
     */
    const std::string& getRequestId() const;
    
    /**
     * Get request information
     * @return Request information
     */
    std::shared_ptr<UTMetadataProtocol::MetadataRequest> getRequestInfo();
    
    /**
     * Check if request is completed
     * @return true if completed
     */
    bool isCompleted();
    
    /**
     * Check if request is active
     * @return true if active
     */
    bool isActive();
};

/**
 * Metadata analyzer for pattern detection and optimization
 */
class MetadataAnalyzer {
private:
    std::shared_ptr<UTMetadataProtocol> protocol_;
    
public:
    MetadataAnalyzer(std::shared_ptr<UTMetadataProtocol> protocol);
    
    /**
     * Analyze metadata patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzeMetadataPatterns();
    
    /**
     * Detect metadata issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectMetadataIssues();
    
    /**
     * Analyze peer metadata compatibility
     * @return Map of peer IPs to compatibility scores
     */
    std::map<std::string, double> analyzePeerMetadataCompatibility();
    
    /**
     * Get metadata optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getMetadataOptimizationRecommendations();
    
    /**
     * Analyze metadata performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzeMetadataPerformanceTrends();
    
    /**
     * Get metadata protocol health score
     * @return Health score (0.0 to 1.0)
     */
    double getMetadataProtocolHealthScore();
};

} // namespace dht_crawler
