#include "metadata_validator.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

namespace dht_crawler {

MetadataValidator::MetadataValidator(const ValidationConfig& config) : config_(config) {
}

MetadataValidator::ValidationResult MetadataValidator::validateMetadata(const std::string& metadata) {
    ValidationResult result;
    
    if (metadata.empty()) {
        result.error_message = "Metadata is empty";
        return result;
    }
    
    // Validate metadata size
    if (!validateMetadataSize(metadata, result)) {
        return result;
    }
    
    // Validate info hash
    if (!validateInfoHash(metadata, result)) {
        return result;
    }
    
    // Validate pieces
    if (!validatePieces(metadata, result)) {
        return result;
    }
    
    // Validate info dictionary
    if (!validateInfoDictionary(metadata, result)) {
        return result;
    }
    
    // Validate file structure
    if (!validateFileStructure(metadata, result)) {
        return result;
    }
    
    result.is_valid = true;
    result.quality_score = calculateQualityScore(result);
    return result;
}

std::string MetadataValidator::calculateSHA1(const std::string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool MetadataValidator::validateMetadataSize(const std::string& metadata, ValidationResult& result) {
    if (metadata.size() > config_.max_metadata_size) {
        result.error_message = "Metadata size exceeds maximum allowed size";
        result.validation_flags["size_exceeded"] = "true";
        result.validation_flags["actual_size"] = std::to_string(metadata.size());
        result.validation_flags["max_size"] = std::to_string(config_.max_metadata_size);
        return false;
    }
    
    if (metadata.size() < 100) { // Minimum reasonable size
        result.error_message = "Metadata size is too small";
        result.validation_flags["size_too_small"] = "true";
        result.validation_flags["actual_size"] = std::to_string(metadata.size());
        return false;
    }
    
    result.validation_flags["size_valid"] = "true";
    result.validation_flags["actual_size"] = std::to_string(metadata.size());
    return true;
}

bool MetadataValidator::validateInfoHash(const std::string& metadata, ValidationResult& result) {
    // Calculate SHA1 hash of the metadata
    std::string calculated_hash = calculateSHA1(metadata);
    
    if (calculated_hash.empty()) {
        result.error_message = "Failed to calculate metadata hash";
        result.validation_flags["hash_calculation_failed"] = "true";
        return false;
    }
    
    result.validation_flags["info_hash"] = calculated_hash;
    result.validation_flags["hash_valid"] = "true";
    return true;
}

bool MetadataValidator::validatePieces(const std::string& metadata, ValidationResult& result) {
    // This is a simplified validation - in a real implementation,
    // you would parse the bencoded metadata and validate the pieces array
    if (metadata.find("pieces") == std::string::npos) {
        result.error_message = "Metadata does not contain pieces information";
        result.validation_flags["pieces_missing"] = "true";
        return false;
    }
    
    result.validation_flags["pieces_present"] = "true";
    return true;
}

bool MetadataValidator::validateInfoDictionary(const std::string& metadata, ValidationResult& result) {
    // Check for required fields in info dictionary
    std::vector<std::string> required_fields = {"name", "length", "piece length", "pieces"};
    
    for (const auto& field : required_fields) {
        if (metadata.find(field) == std::string::npos) {
            result.error_message = "Missing required field: " + field;
            result.validation_flags["missing_field"] = field;
            return false;
        }
    }
    
    result.validation_flags["info_dictionary_valid"] = "true";
    return true;
}

bool MetadataValidator::validateFileStructure(const std::string& metadata, ValidationResult& result) {
    // Check for file structure validity
    if (metadata.find("files") != std::string::npos) {
        // Multi-file torrent
        result.validation_flags["torrent_type"] = "multi_file";
    } else if (metadata.find("length") != std::string::npos) {
        // Single file torrent
        result.validation_flags["torrent_type"] = "single_file";
    } else {
        result.error_message = "Invalid file structure - neither single nor multi-file";
        result.validation_flags["file_structure_invalid"] = "true";
        return false;
    }
    
    result.validation_flags["file_structure_valid"] = "true";
    return true;
}

double MetadataValidator::calculateQualityScore(const ValidationResult& result) {
    double score = 0.0;
    
    // Base score for valid metadata
    if (result.is_valid) {
        score += 0.5;
    }
    
    // Additional points for various validations
    if (result.validation_flags.find("size_valid") != result.validation_flags.end()) {
        score += 0.1;
    }
    
    if (result.validation_flags.find("hash_valid") != result.validation_flags.end()) {
        score += 0.1;
    }
    
    if (result.validation_flags.find("pieces_present") != result.validation_flags.end()) {
        score += 0.1;
    }
    
    if (result.validation_flags.find("info_dictionary_valid") != result.validation_flags.end()) {
        score += 0.1;
    }
    
    if (result.validation_flags.find("file_structure_valid") != result.validation_flags.end()) {
        score += 0.1;
    }
    
    return std::min(score, 1.0);
}

void MetadataValidator::updateConfig(const ValidationConfig& config) {
    config_ = config;
}

MetadataValidator::ValidationConfig MetadataValidator::getConfig() const {
    return config_;
}

std::map<std::string, std::string> MetadataValidator::getValidationFlags(const ValidationResult& result) {
    return result.validation_flags;
}

std::string MetadataValidator::getErrorMessage(const ValidationResult& result) {
    return result.error_message;
}

double MetadataValidator::getQualityScore(const ValidationResult& result) {
    return result.quality_score;
}

bool MetadataValidator::isValid(const ValidationResult& result) {
    return result.is_valid;
}

} // namespace dht_crawler
