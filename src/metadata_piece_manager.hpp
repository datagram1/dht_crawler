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
#include <set>
#include <algorithm>
#include <unordered_map>
#include <bitset>

namespace dht_crawler {

/**
 * Metadata piece manager for sophisticated piece handling
 * Provides piece tracking, assembly, validation, and recovery
 */
class MetadataPieceManager {
public:
    enum class PieceStatus {
        MISSING,        // Piece is missing
        REQUESTED,      // Piece has been requested
        RECEIVED,       // Piece has been received
        VALIDATED,      // Piece has been validated
        INVALID,        // Piece is invalid
        DUPLICATE,      // Piece is duplicate
        CORRUPTED,      // Piece is corrupted
        EXPIRED         // Piece has expired
    };

    enum class AssemblyStatus {
        INCOMPLETE,     // Assembly is incomplete
        COMPLETE,       // Assembly is complete
        VALIDATED,      // Assembly has been validated
        INVALID,        // Assembly is invalid
        CORRUPTED,      // Assembly is corrupted
        EXPIRED         // Assembly has expired
    };

    struct PieceConfig {
        size_t max_piece_size = 16384;                  // 16KB pieces like magnetico
        size_t max_total_size = 1024 * 1024;           // 1MB max total size
        std::chrono::milliseconds piece_timeout{30000}; // 30 seconds
        std::chrono::milliseconds assembly_timeout{300000}; // 5 minutes
        int max_retry_attempts = 3;                    // Max retry attempts
        std::chrono::milliseconds retry_delay{1000};    // Retry delay
        bool enable_piece_validation = true;            // Enable piece validation
        bool enable_assembly_validation = true;          // Enable assembly validation
        bool enable_duplicate_detection = true;         // Enable duplicate detection
        bool enable_corruption_detection = true;        // Enable corruption detection
        bool enable_piece_caching = true;               // Enable piece caching
        bool enable_assembly_caching = true;             // Enable assembly caching
        double corruption_threshold = 0.1;             // Corruption threshold
        int max_concurrent_requests = 10;               // Max concurrent requests
        std::chrono::milliseconds cleanup_interval{60000}; // Cleanup interval
    };

    struct MetadataPiece {
        std::string info_hash;
        int piece_index;
        PieceStatus status;
        std::vector<uint8_t> data;
        std::string checksum;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point updated_at;
        std::chrono::steady_clock::time_point expires_at;
        
        // Request tracking
        int request_count;
        int retry_count;
        std::chrono::steady_clock::time_point last_request;
        std::chrono::steady_clock::time_point last_response;
        
        // Quality metrics
        double quality_score;
        int validation_attempts;
        int successful_validations;
        int failed_validations;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct MetadataAssembly {
        std::string info_hash;
        AssemblyStatus status;
        std::vector<uint8_t> data;
        std::string checksum;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point updated_at;
        std::chrono::steady_clock::time_point expires_at;
        
        // Piece tracking
        std::bitset<1024> piece_bitmap; // Support up to 1024 pieces
        int total_pieces;
        int received_pieces;
        int validated_pieces;
        int missing_pieces;
        int invalid_pieces;
        int duplicate_pieces;
        int corrupted_pieces;
        
        // Assembly statistics
        std::chrono::milliseconds assembly_time;
        std::chrono::milliseconds validation_time;
        int assembly_attempts;
        int validation_attempts;
        
        // Quality metrics
        double quality_score;
        double completeness_ratio;
        double validity_ratio;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct PieceManagerStatistics {
        int total_pieces = 0;
        int missing_pieces = 0;
        int requested_pieces = 0;
        int received_pieces = 0;
        int validated_pieces = 0;
        int invalid_pieces = 0;
        int duplicate_pieces = 0;
        int corrupted_pieces = 0;
        int expired_pieces = 0;
        int total_assemblies = 0;
        int incomplete_assemblies = 0;
        int complete_assemblies = 0;
        int validated_assemblies = 0;
        int invalid_assemblies = 0;
        int corrupted_assemblies = 0;
        int expired_assemblies = 0;
        double avg_quality_score = 0.0;
        double avg_completeness_ratio = 0.0;
        double avg_validity_ratio = 0.0;
        std::chrono::milliseconds avg_assembly_time;
        std::chrono::milliseconds avg_validation_time;
        std::map<PieceStatus, int> pieces_by_status;
        std::map<AssemblyStatus, int> assemblies_by_status;
        std::map<std::string, int> pieces_by_info_hash;
        std::map<std::string, int> assemblies_by_info_hash;
        std::chrono::steady_clock::time_point last_update;
    };

private:
    PieceConfig config_;
    std::map<std::string, std::map<int, std::shared_ptr<MetadataPiece>>> pieces_;
    std::mutex pieces_mutex_;
    
    std::map<std::string, std::shared_ptr<MetadataAssembly>> assemblies_;
    std::mutex assemblies_mutex_;
    
    PieceManagerStatistics stats_;
    std::mutex stats_mutex_;
    
    // Background processing
    std::thread piece_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable monitor_condition_;
    
    // Internal methods
    void pieceMonitorLoop();
    void monitorPieces();
    void cleanupExpiredPieces();
    void cleanupExpiredAssemblies();
    void updateStatistics();
    std::string calculatePieceChecksum(const std::vector<uint8_t>& data);
    std::string calculateAssemblyChecksum(const std::vector<uint8_t>& data);
    bool validatePieceData(const std::vector<uint8_t>& data);
    bool validateAssemblyData(const std::vector<uint8_t>& data);
    bool isPieceExpired(const MetadataPiece& piece);
    bool isAssemblyExpired(const MetadataAssembly& assembly);
    void updatePieceQuality(std::shared_ptr<MetadataPiece> piece, bool success);
    void updateAssemblyQuality(std::shared_ptr<MetadataAssembly> assembly);
    PieceStatus determinePieceStatus(const MetadataPiece& piece);
    AssemblyStatus determineAssemblyStatus(const MetadataAssembly& assembly);
    void redistributePieces(std::shared_ptr<MetadataAssembly> assembly);
    std::vector<int> getMissingPieceIndices(const MetadataAssembly& assembly);
    std::vector<int> getInvalidPieceIndices(const MetadataAssembly& assembly);
    std::vector<int> getCorruptedPieceIndices(const MetadataAssembly& assembly);
    void initializeAssembly(std::shared_ptr<MetadataAssembly> assembly);
    void finalizeAssembly(std::shared_ptr<MetadataAssembly> assembly);
    bool isValidInfoHash(const std::string& info_hash);
    bool isValidPieceIndex(int piece_index);
    bool isValidPieceData(const std::vector<uint8_t>& data);
    bool isValidAssemblyData(const std::vector<uint8_t>& data);

public:
    MetadataPieceManager(const PieceConfig& config = PieceConfig{});
    ~MetadataPieceManager();
    
    /**
     * Add a piece to the manager
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @param data Piece data
     * @return true if piece was added successfully
     */
    bool addPiece(const std::string& info_hash, int piece_index, const std::vector<uint8_t>& data);
    
    /**
     * Request a piece
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece was requested successfully
     */
    bool requestPiece(const std::string& info_hash, int piece_index);
    
    /**
     * Mark piece as received
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @param data Piece data
     * @return true if piece was marked as received successfully
     */
    bool markPieceReceived(const std::string& info_hash, int piece_index, const std::vector<uint8_t>& data);
    
    /**
     * Mark piece as invalid
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece was marked as invalid successfully
     */
    bool markPieceInvalid(const std::string& info_hash, int piece_index);
    
    /**
     * Mark piece as duplicate
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece was marked as duplicate successfully
     */
    bool markPieceDuplicate(const std::string& info_hash, int piece_index);
    
    /**
     * Mark piece as corrupted
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece was marked as corrupted successfully
     */
    bool markPieceCorrupted(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece information
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Piece information or nullptr if not found
     */
    std::shared_ptr<MetadataPiece> getPiece(const std::string& info_hash, int piece_index);
    
    /**
     * Get all pieces for an info hash
     * @param info_hash Info hash
     * @return Map of piece index to piece information
     */
    std::map<int, std::shared_ptr<MetadataPiece>> getPieces(const std::string& info_hash);
    
    /**
     * Get pieces by status
     * @param info_hash Info hash
     * @param status Piece status
     * @return Vector of piece indices
     */
    std::vector<int> getPiecesByStatus(const std::string& info_hash, PieceStatus status);
    
    /**
     * Get missing pieces
     * @param info_hash Info hash
     * @return Vector of missing piece indices
     */
    std::vector<int> getMissingPieces(const std::string& info_hash);
    
    /**
     * Get requested pieces
     * @param info_hash Info hash
     * @return Vector of requested piece indices
     */
    std::vector<int> getRequestedPieces(const std::string& info_hash);
    
    /**
     * Get received pieces
     * @param info_hash Info hash
     * @return Vector of received piece indices
     */
    std::vector<int> getReceivedPieces(const std::string& info_hash);
    
    /**
     * Get validated pieces
     * @param info_hash Info hash
     * @return Vector of validated piece indices
     */
    std::vector<int> getValidatedPieces(const std::string& info_hash);
    
    /**
     * Get invalid pieces
     * @param info_hash Info hash
     * @return Vector of invalid piece indices
     */
    std::vector<int> getInvalidPieces(const std::string& info_hash);
    
    /**
     * Get duplicate pieces
     * @param info_hash Info hash
     * @return Vector of duplicate piece indices
     */
    std::vector<int> getDuplicatePieces(const std::string& info_hash);
    
    /**
     * Get corrupted pieces
     * @param info_hash Info hash
     * @return Vector of corrupted piece indices
     */
    std::vector<int> getCorruptedPieces(const std::string& info_hash);
    
    /**
     * Get expired pieces
     * @param info_hash Info hash
     * @return Vector of expired piece indices
     */
    std::vector<int> getExpiredPieces(const std::string& info_hash);
    
    /**
     * Check if piece exists
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece exists
     */
    bool hasPiece(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is complete
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is complete
     */
    bool isPieceComplete(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is valid
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is valid
     */
    bool isPieceValid(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is missing
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is missing
     */
    bool isPieceMissing(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is requested
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is requested
     */
    bool isPieceRequested(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is received
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is received
     */
    bool isPieceReceived(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is invalid
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is invalid
     */
    bool isPieceInvalid(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is duplicate
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is duplicate
     */
    bool isPieceDuplicate(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is corrupted
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is corrupted
     */
    bool isPieceCorrupted(const std::string& info_hash, int piece_index);
    
    /**
     * Check if piece is expired
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece is expired
     */
    bool isPieceExpired(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece status
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Piece status or MISSING if not found
     */
    PieceStatus getPieceStatus(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece data
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Piece data or empty vector if not found
     */
    std::vector<uint8_t> getPieceData(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece checksum
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Piece checksum or empty string if not found
     */
    std::string getPieceChecksum(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece quality score
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Quality score or 0.0 if not found
     */
    double getPieceQualityScore(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece request count
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Request count or 0 if not found
     */
    int getPieceRequestCount(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece retry count
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Retry count or 0 if not found
     */
    int getPieceRetryCount(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece creation time
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Creation time or default time if not found
     */
    std::chrono::steady_clock::time_point getPieceCreationTime(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece update time
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Update time or default time if not found
     */
    std::chrono::steady_clock::time_point getPieceUpdateTime(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece expiration time
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Expiration time or default time if not found
     */
    std::chrono::steady_clock::time_point getPieceExpirationTime(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece metadata
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return Piece metadata or empty map if not found
     */
    std::map<std::string, std::string> getPieceMetadata(const std::string& info_hash, int piece_index);
    
    /**
     * Set piece metadata
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @param key Metadata key
     * @param value Metadata value
     * @return true if metadata was set successfully
     */
    bool setPieceMetadata(const std::string& info_hash, int piece_index, const std::string& key, const std::string& value);
    
    /**
     * Remove piece metadata
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @param key Metadata key
     * @return true if metadata was removed successfully
     */
    bool removePieceMetadata(const std::string& info_hash, int piece_index, const std::string& key);
    
    /**
     * Clear piece metadata
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if metadata was cleared successfully
     */
    bool clearPieceMetadata(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece count for info hash
     * @param info_hash Info hash
     * @return Piece count
     */
    int getPieceCount(const std::string& info_hash);
    
    /**
     * Get piece count by status
     * @param info_hash Info hash
     * @param status Piece status
     * @return Piece count
     */
    int getPieceCountByStatus(const std::string& info_hash, PieceStatus status);
    
    /**
     * Get total piece count
     * @return Total piece count
     */
    int getTotalPieceCount();
    
    /**
     * Get total piece count by status
     * @param status Piece status
     * @return Total piece count
     */
    int getTotalPieceCountByStatus(PieceStatus status);
    
    /**
     * Get piece completion ratio
     * @param info_hash Info hash
     * @return Completion ratio (0.0 to 1.0)
     */
    double getPieceCompletionRatio(const std::string& info_hash);
    
    /**
     * Get piece validity ratio
     * @param info_hash Info hash
     * @return Validity ratio (0.0 to 1.0)
     */
    double getPieceValidityRatio(const std::string& info_hash);
    
    /**
     * Get piece quality ratio
     * @param info_hash Info hash
     * @return Quality ratio (0.0 to 1.0)
     */
    double getPieceQualityRatio(const std::string& info_hash);
    
    /**
     * Get piece statistics
     * @param info_hash Info hash
     * @return Piece statistics
     */
    std::map<std::string, int> getPieceStatistics(const std::string& info_hash);
    
    /**
     * Get piece manager statistics
     * @return Piece manager statistics
     */
    PieceManagerStatistics getStatistics();
    
    /**
     * Reset piece manager statistics
     */
    void resetStatistics();
    
    /**
     * Update piece configuration
     * @param config New piece configuration
     */
    void updateConfig(const PieceConfig& config);
    
    /**
     * Get piece configuration
     * @return Current piece configuration
     */
    PieceConfig getConfig();
    
    /**
     * Start piece manager
     */
    void start();
    
    /**
     * Stop piece manager
     */
    void stop();
    
    /**
     * Check if piece manager is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export pieces to file
     * @param info_hash Info hash
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportPieces(const std::string& info_hash, const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get piece manager health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear pieces for info hash
     * @param info_hash Info hash
     * @return Number of pieces cleared
     */
    int clearPieces(const std::string& info_hash);
    
    /**
     * Clear all pieces
     * @return Number of pieces cleared
     */
    int clearAllPieces();
    
    /**
     * Clear expired pieces
     * @return Number of pieces cleared
     */
    int clearExpiredPieces();
    
    /**
     * Force cleanup of pieces
     */
    void forceCleanup();
    
    /**
     * Get piece statistics by info hash
     * @return Map of info hashes to piece counts
     */
    std::map<std::string, int> getPieceStatisticsByInfoHash();
    
    /**
     * Get piece statistics by status
     * @return Map of piece statuses to counts
     */
    std::map<PieceStatus, int> getPieceStatisticsByStatus();
    
    /**
     * Get piece manager recommendations
     * @return Vector of piece manager recommendations
     */
    std::vector<std::string> getPieceManagerRecommendations();
    
    /**
     * Optimize piece manager
     */
    void optimizePieceManager();
    
    /**
     * Get piece manager performance metrics
     * @return Performance metrics
     */
    std::map<std::string, double> getPieceManagerPerformanceMetrics();
    
    /**
     * Force piece validation
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece was validated successfully
     */
    bool forcePieceValidation(const std::string& info_hash, int piece_index);
    
    /**
     * Force piece cleanup
     * @param info_hash Info hash
     * @param piece_index Piece index
     * @return true if piece was cleaned up successfully
     */
    bool forcePieceCleanup(const std::string& info_hash, int piece_index);
    
    /**
     * Get piece manager capacity
     * @return Piece manager capacity information
     */
    std::map<std::string, int> getPieceManagerCapacity();
    
    /**
     * Validate piece manager integrity
     * @return true if piece manager is valid
     */
    bool validatePieceManagerIntegrity();
    
    /**
     * Get piece manager issues
     * @return Vector of piece manager issues
     */
    std::vector<std::string> getPieceManagerIssues();
    
    /**
     * Repair piece manager
     * @return Number of issues repaired
     */
    int repairPieceManager();
};

/**
 * Metadata assembly manager for sophisticated assembly handling
 */
class MetadataAssemblyManager {
private:
    std::shared_ptr<MetadataPieceManager> piece_manager_;
    std::map<std::string, std::shared_ptr<MetadataAssembly>> assemblies_;
    std::mutex assemblies_mutex_;
    
public:
    MetadataAssemblyManager(std::shared_ptr<MetadataPieceManager> piece_manager);
    
    /**
     * Create assembly for info hash
     * @param info_hash Info hash
     * @param total_pieces Total number of pieces
     * @return true if assembly was created successfully
     */
    bool createAssembly(const std::string& info_hash, int total_pieces);
    
    /**
     * Get assembly information
     * @param info_hash Info hash
     * @return Assembly information or nullptr if not found
     */
    std::shared_ptr<MetadataAssembly> getAssembly(const std::string& info_hash);
    
    /**
     * Check if assembly exists
     * @param info_hash Info hash
     * @return true if assembly exists
     */
    bool hasAssembly(const std::string& info_hash);
    
    /**
     * Check if assembly is complete
     * @param info_hash Info hash
     * @return true if assembly is complete
     */
    bool isAssemblyComplete(const std::string& info_hash);
    
    /**
     * Check if assembly is valid
     * @param info_hash Info hash
     * @return true if assembly is valid
     */
    bool isAssemblyValid(const std::string& info_hash);
    
    /**
     * Get assembly data
     * @param info_hash Info hash
     * @return Assembly data or empty vector if not found
     */
    std::vector<uint8_t> getAssemblyData(const std::string& info_hash);
    
    /**
     * Get assembly checksum
     * @param info_hash Info hash
     * @return Assembly checksum or empty string if not found
     */
    std::string getAssemblyChecksum(const std::string& info_hash);
    
    /**
     * Get assembly status
     * @param info_hash Info hash
     * @return Assembly status or INCOMPLETE if not found
     */
    AssemblyStatus getAssemblyStatus(const std::string& info_hash);
    
    /**
     * Get assembly completion ratio
     * @param info_hash Info hash
     * @return Completion ratio (0.0 to 1.0)
     */
    double getAssemblyCompletionRatio(const std::string& info_hash);
    
    /**
     * Get assembly validity ratio
     * @param info_hash Info hash
     * @return Validity ratio (0.0 to 1.0)
     */
    double getAssemblyValidityRatio(const std::string& info_hash);
    
    /**
     * Get assembly quality score
     * @param info_hash Info hash
     * @return Quality score or 0.0 if not found
     */
    double getAssemblyQualityScore(const std::string& info_hash);
    
    /**
     * Get assembly statistics
     * @param info_hash Info hash
     * @return Assembly statistics
     */
    std::map<std::string, int> getAssemblyStatistics(const std::string& info_hash);
    
    /**
     * Get assembly count
     * @return Assembly count
     */
    int getAssemblyCount();
    
    /**
     * Get assembly count by status
     * @param status Assembly status
     * @return Assembly count
     */
    int getAssemblyCountByStatus(AssemblyStatus status);
    
    /**
     * Get total assembly count
     * @return Total assembly count
     */
    int getTotalAssemblyCount();
    
    /**
     * Get total assembly count by status
     * @param status Assembly status
     * @return Total assembly count
     */
    int getTotalAssemblyCountByStatus(AssemblyStatus status);
    
    /**
     * Get assembly completion ratio
     * @return Completion ratio (0.0 to 1.0)
     */
    double getAssemblyCompletionRatio();
    
    /**
     * Get assembly validity ratio
     * @return Validity ratio (0.0 to 1.0)
     */
    double getAssemblyValidityRatio();
    
    /**
     * Get assembly quality ratio
     * @return Quality ratio (0.0 to 1.0)
     */
    double getAssemblyQualityRatio();
    
    /**
     * Get assembly statistics
     * @return Assembly statistics
     */
    std::map<std::string, int> getAssemblyStatistics();
    
    /**
     * Get assembly manager statistics
     * @return Assembly manager statistics
     */
    std::map<std::string, int> getAssemblyManagerStatistics();
    
    /**
     * Reset assembly manager statistics
     */
    void resetAssemblyManagerStatistics();
    
    /**
     * Clear assembly for info hash
     * @param info_hash Info hash
     * @return true if assembly was cleared successfully
     */
    bool clearAssembly(const std::string& info_hash);
    
    /**
     * Clear all assemblies
     * @return Number of assemblies cleared
     */
    int clearAllAssemblies();
    
    /**
     * Clear expired assemblies
     * @return Number of assemblies cleared
     */
    int clearExpiredAssemblies();
    
    /**
     * Force cleanup of assemblies
     */
    void forceCleanup();
    
    /**
     * Get assembly statistics by info hash
     * @return Map of info hashes to assembly counts
     */
    std::map<std::string, int> getAssemblyStatisticsByInfoHash();
    
    /**
     * Get assembly statistics by status
     * @return Map of assembly statuses to counts
     */
    std::map<AssemblyStatus, int> getAssemblyStatisticsByStatus();
    
    /**
     * Get assembly manager recommendations
     * @return Vector of assembly manager recommendations
     */
    std::vector<std::string> getAssemblyManagerRecommendations();
    
    /**
     * Optimize assembly manager
     */
    void optimizeAssemblyManager();
    
    /**
     * Get assembly manager performance metrics
     * @return Performance metrics
     */
    std::map<std::string, double> getAssemblyManagerPerformanceMetrics();
    
    /**
     * Force assembly validation
     * @param info_hash Info hash
     * @return true if assembly was validated successfully
     */
    bool forceAssemblyValidation(const std::string& info_hash);
    
    /**
     * Force assembly cleanup
     * @param info_hash Info hash
     * @return true if assembly was cleaned up successfully
     */
    bool forceAssemblyCleanup(const std::string& info_hash);
    
    /**
     * Get assembly manager capacity
     * @return Assembly manager capacity information
     */
    std::map<std::string, int> getAssemblyManagerCapacity();
    
    /**
     * Validate assembly manager integrity
     * @return true if assembly manager is valid
     */
    bool validateAssemblyManagerIntegrity();
    
    /**
     * Get assembly manager issues
     * @return Vector of assembly manager issues
     */
    std::vector<std::string> getAssemblyManagerIssues();
    
    /**
     * Repair assembly manager
     * @return Number of issues repaired
     */
    int repairAssemblyManager();
};

/**
 * Metadata piece manager factory for different piece strategies
 */
class MetadataPieceManagerFactory {
public:
    enum class PieceStrategy {
        PERFORMANCE,    // Optimized for performance
        RELIABILITY,    // Optimized for reliability
        BALANCED,       // Balanced performance and reliability
        CUSTOM          // Custom piece configuration
    };
    
    static std::unique_ptr<MetadataPieceManager> createManager(
        PieceStrategy strategy,
        const MetadataPieceManager::PieceConfig& config = MetadataPieceManager::PieceConfig{}
    );
    
    static MetadataPieceManager::PieceConfig getDefaultConfig(PieceStrategy strategy);
};

/**
 * RAII metadata piece wrapper for automatic piece management
 */
class MetadataPieceGuard {
private:
    std::shared_ptr<MetadataPieceManager> manager_;
    std::string info_hash_;
    int piece_index_;

public:
    MetadataPieceGuard(std::shared_ptr<MetadataPieceManager> manager, const std::string& info_hash, int piece_index);
    
    /**
     * Add piece data
     * @param data Piece data
     * @return true if piece was added successfully
     */
    bool addPiece(const std::vector<uint8_t>& data);
    
    /**
     * Request piece
     * @return true if piece was requested successfully
     */
    bool requestPiece();
    
    /**
     * Mark piece as received
     * @param data Piece data
     * @return true if piece was marked as received successfully
     */
    bool markPieceReceived(const std::vector<uint8_t>& data);
    
    /**
     * Mark piece as invalid
     * @return true if piece was marked as invalid successfully
     */
    bool markPieceInvalid();
    
    /**
     * Mark piece as duplicate
     * @return true if piece was marked as duplicate successfully
     */
    bool markPieceDuplicate();
    
    /**
     * Mark piece as corrupted
     * @return true if piece was marked as corrupted successfully
     */
    bool markPieceCorrupted();
    
    /**
     * Get piece information
     * @return Piece information or nullptr if not found
     */
    std::shared_ptr<MetadataPieceManager::MetadataPiece> getPiece();
    
    /**
     * Check if piece exists
     * @return true if piece exists
     */
    bool hasPiece();
    
    /**
     * Check if piece is complete
     * @return true if piece is complete
     */
    bool isPieceComplete();
    
    /**
     * Check if piece is valid
     * @return true if piece is valid
     */
    bool isPieceValid();
    
    /**
     * Check if piece is missing
     * @return true if piece is missing
     */
    bool isPieceMissing();
    
    /**
     * Check if piece is requested
     * @return true if piece is requested
     */
    bool isPieceRequested();
    
    /**
     * Check if piece is received
     * @return true if piece is received
     */
    bool isPieceReceived();
    
    /**
     * Check if piece is invalid
     * @return true if piece is invalid
     */
    bool isPieceInvalid();
    
    /**
     * Check if piece is duplicate
     * @return true if piece is duplicate
     */
    bool isPieceDuplicate();
    
    /**
     * Check if piece is corrupted
     * @return true if piece is corrupted
     */
    bool isPieceCorrupted();
    
    /**
     * Check if piece is expired
     * @return true if piece is expired
     */
    bool isPieceExpired();
    
    /**
     * Get piece status
     * @return Piece status or MISSING if not found
     */
    MetadataPieceManager::PieceStatus getPieceStatus();
    
    /**
     * Get piece data
     * @return Piece data or empty vector if not found
     */
    std::vector<uint8_t> getPieceData();
    
    /**
     * Get piece checksum
     * @return Piece checksum or empty string if not found
     */
    std::string getPieceChecksum();
    
    /**
     * Get piece quality score
     * @return Quality score or 0.0 if not found
     */
    double getPieceQualityScore();
    
    /**
     * Get piece request count
     * @return Request count or 0 if not found
     */
    int getPieceRequestCount();
    
    /**
     * Get piece retry count
     * @return Retry count or 0 if not found
     */
    int getPieceRetryCount();
    
    /**
     * Get piece creation time
     * @return Creation time or default time if not found
     */
    std::chrono::steady_clock::time_point getPieceCreationTime();
    
    /**
     * Get piece update time
     * @return Update time or default time if not found
     */
    std::chrono::steady_clock::time_point getPieceUpdateTime();
    
    /**
     * Get piece expiration time
     * @return Expiration time or default time if not found
     */
    std::chrono::steady_clock::time_point getPieceExpirationTime();
    
    /**
     * Get piece metadata
     * @return Piece metadata or empty map if not found
     */
    std::map<std::string, std::string> getPieceMetadata();
    
    /**
     * Set piece metadata
     * @param key Metadata key
     * @param value Metadata value
     * @return true if metadata was set successfully
     */
    bool setPieceMetadata(const std::string& key, const std::string& value);
    
    /**
     * Remove piece metadata
     * @param key Metadata key
     * @return true if metadata was removed successfully
     */
    bool removePieceMetadata(const std::string& key);
    
    /**
     * Clear piece metadata
     * @return true if metadata was cleared successfully
     */
    bool clearPieceMetadata();
};

/**
 * Metadata piece analyzer for pattern detection and optimization
 */
class MetadataPieceAnalyzer {
private:
    std::shared_ptr<MetadataPieceManager> manager_;
    
public:
    MetadataPieceAnalyzer(std::shared_ptr<MetadataPieceManager> manager);
    
    /**
     * Analyze piece patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzePiecePatterns();
    
    /**
     * Detect piece issues
     * @return Vector of detected issues
     */
    std::vector<std::string> detectPieceIssues();
    
    /**
     * Analyze piece distribution
     * @return Piece distribution analysis
     */
    std::map<std::string, int> analyzePieceDistribution();
    
    /**
     * Get piece optimization recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getPieceOptimizationRecommendations();
    
    /**
     * Analyze piece performance trends
     * @return Performance trend analysis
     */
    std::map<std::string, double> analyzePiecePerformanceTrends();
    
    /**
     * Get piece health score
     * @return Health score (0.0 to 1.0)
     */
    double getPieceHealthScore();
    
    /**
     * Get piece efficiency analysis
     * @return Piece efficiency analysis
     */
    std::map<std::string, double> getPieceEfficiencyAnalysis();
    
    /**
     * Get piece quality distribution
     * @return Piece quality distribution
     */
    std::map<std::string, int> getPieceQualityDistribution();
    
    /**
     * Get piece capacity analysis
     * @return Capacity analysis
     */
    std::map<std::string, double> getPieceCapacityAnalysis();
};

} // namespace dht_crawler
