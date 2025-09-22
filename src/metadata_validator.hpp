#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace dht_crawler {

/**
 * Comprehensive metadata validation for torrent metadata
 * Based on magnetico's validation approach with enhanced error handling
 */
class MetadataValidator {
public:
    struct ValidationResult {
        bool is_valid = false;
        std::string error_message;
        std::string validation_details;
        double quality_score = 0.0;
        std::map<std::string, std::string> validation_flags;
    };

    struct ValidationConfig {
        size_t max_metadata_size; // 10MB max like magnetico
        size_t min_piece_length; // 16KB minimum
        size_t max_piece_length; // 16MB maximum
        size_t max_files; // Maximum number of files
        size_t max_file_name_length; // Maximum file name length
        bool strict_validation; // Enable strict validation mode
        bool validate_checksums; // Enable checksum validation
        
        ValidationConfig() : 
            max_metadata_size(10 * 1024 * 1024),
            min_piece_length(16 * 1024),
            max_piece_length(16 * 1024 * 1024),
            max_files(10000),
            max_file_name_length(255),
            strict_validation(true),
            validate_checksums(true) {}
    };

private:
    ValidationConfig config_;
    
    // SHA1 hash calculation
    std::string calculateSHA1(const std::string& data);
    
    // Metadata size validation
    bool validateMetadataSize(const std::string& metadata, ValidationResult& result);
    
    // Piece validation
    bool validatePieces(const std::string& metadata, ValidationResult& result);
    
    // Info dictionary validation
    bool validateInfoDictionary(const std::string& metadata, ValidationResult& result);
    
    // File structure validation
    bool validateFileStructure(const std::string& metadata, ValidationResult& result);
    
    // Bencode parsing helpers
    bool isValidBencode(const std::string& data);
    std::map<std::string, std::string> parseBencodeDictionary(const std::string& data);
    std::vector<std::string> parseBencodeList(const std::string& data);
    
    // Quality scoring
    double calculateQualityScore(const std::string& metadata, const ValidationResult& result);

public:
    MetadataValidator(const ValidationConfig& config = ValidationConfig());
    
    /**
     * Validate torrent metadata comprehensively
     * @param metadata Raw metadata bytes
     * @param expected_info_hash Expected SHA1 hash of info dictionary
     * @return ValidationResult with validation status and details
     */
    ValidationResult validateMetadata(const std::string& metadata, 
                                     const std::string& expected_info_hash = "");
    
    /**
     * Validate metadata against specific criteria
     * @param metadata Raw metadata bytes
     * @param criteria Specific validation criteria
     * @return ValidationResult with validation status
     */
    ValidationResult validateWithCriteria(const std::string& metadata,
                                        const std::map<std::string, bool>& criteria);
    
    /**
     * Quick validation for basic metadata integrity
     * @param metadata Raw metadata bytes
     * @return true if basic validation passes
     */
    bool quickValidate(const std::string& metadata);
    
    /**
     * Get validation statistics
     * @return Map of validation statistics
     */
    std::map<std::string, int> getValidationStats();
    
    /**
     * Update validation configuration
     * @param config New validation configuration
     */
    void updateConfig(const ValidationConfig& config);
    
    /**
     * Reset validation statistics
     */
    void resetStats();

private:
    // Validation statistics
    std::map<std::string, int> validation_stats_;
    
    // Helper methods for specific validations
    bool validateTorrentName(const std::string& name);
    bool validatePieceLength(size_t piece_length);
    bool validateFileCount(size_t file_count);
    bool validateTotalSize(size_t total_size);
    bool validateAnnounceList(const std::vector<std::string>& announce_list);
    bool validateFileNames(const std::vector<std::string>& file_names);
    bool validateFileSizes(const std::vector<size_t>& file_sizes);
    
    // Error message formatting
    std::string formatValidationError(const std::string& error_type, 
                                    const std::string& details);
};

/**
 * Enhanced torrent metadata structure with validation flags
 */
struct EnhancedTorrentMetadata {
    std::string info_hash;
    std::string name;
    size_t size;
    size_t piece_length;
    int pieces_count;
    int files_count;
    std::chrono::steady_clock::time_point discovered_at;
    std::chrono::steady_clock::time_point last_seen_at;
    std::string tracker_info;
    std::string comment;
    std::string created_by;
    std::string encoding;
    bool private_flag;
    std::string source;
    std::string url_list;
    std::string announce_list;
    std::string file_list;
    size_t metadata_size;
    
    // Validation flags
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
    
    // BEP 9 Extension Protocol tracking
    bool bep9_extension_used = false;
    std::string extension_protocol_version;
    std::string extension_protocol_error;
    
    // Validation details
    std::map<std::string, std::string> validation_details;
    std::vector<std::string> validation_warnings;
    
    // File information
    struct FileInfo {
        std::string name;
        size_t size;
        std::string path;
        std::string content_type;
    };
    
    std::vector<FileInfo> files;
    
    // Peer information
    struct PeerInfo {
        std::string ip;
        int port;
        std::string peer_id;
        std::chrono::steady_clock::time_point last_seen;
        double quality_score;
    };
    
    std::vector<PeerInfo> peers;
    
    // Tracker information
    struct TrackerInfo {
        std::string url;
        std::string domain;
        std::string protocol;
        std::chrono::steady_clock::time_point last_announce;
        int seeders;
        int leechers;
        double success_rate;
    };
    
    std::vector<TrackerInfo> trackers;
};

/**
 * Metadata validation factory for different validation strategies
 */
class MetadataValidationFactory {
public:
    enum class ValidationStrategy {
        STRICT,     // Strict validation with all checks
        MODERATE,   // Moderate validation with essential checks
        LENIENT,    // Lenient validation with basic checks
        CUSTOM      // Custom validation configuration
    };
    
    static std::unique_ptr<MetadataValidator> createValidator(
        ValidationStrategy strategy,
        const MetadataValidator::ValidationConfig& config = MetadataValidator::ValidationConfig{}
    );
    
    static MetadataValidator::ValidationConfig getDefaultConfig(ValidationStrategy strategy);
};

} // namespace dht_crawler
