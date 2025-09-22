#include "metadata_piece_manager.hpp"
#include <algorithm>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

namespace dht_crawler {

MetadataPieceManager::MetadataPieceManager(const PieceConfig& config) : config_(config), should_stop_(false) {
    piece_monitor_thread_ = std::thread(&MetadataPieceManager::pieceMonitorLoop, this);
}

MetadataPieceManager::~MetadataPieceManager() {
    stop();
    if (piece_monitor_thread_.joinable()) {
        piece_monitor_thread_.join();
    }
}

void MetadataPieceManager::pieceMonitorLoop() {
    while (!should_stop_) {
        std::unique_lock<std::mutex> lock(monitor_mutex_);
        
        monitor_condition_.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return should_stop_;
        });
        
        if (should_stop_) break;
        
        lock.unlock();
        
        monitorPieces();
        cleanupExpiredPieces();
        cleanupExpiredAssemblies();
        updateStatistics();
    }
}

void MetadataPieceManager::monitorPieces() {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    for (auto& pair : pieces_) {
        for (auto& piece_pair : pair.second) {
            auto& piece = piece_pair.second;
            if (piece->status == PieceStatus::REQUESTED) {
                auto now = std::chrono::steady_clock::now();
                if (now - piece->last_request > std::chrono::milliseconds(config_.piece_timeout)) {
                    piece->status = PieceStatus::EXPIRED;
                }
            }
        }
    }
}

void MetadataPieceManager::cleanupExpiredPieces() {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : pieces_) {
        for (auto it = pair.second.begin(); it != pair.second.end();) {
            if (isPieceExpired(*it->second)) {
                it = pair.second.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void MetadataPieceManager::cleanupExpiredAssemblies() {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = assemblies_.begin(); it != assemblies_.end();) {
        if (isAssemblyExpired(*it->second)) {
            it = assemblies_.erase(it);
        } else {
            ++it;
        }
    }
}

void MetadataPieceManager::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_pieces = 0;
    stats_.missing_pieces = 0;
    stats_.requested_pieces = 0;
    stats_.received_pieces = 0;
    stats_.validated_pieces = 0;
    stats_.invalid_pieces = 0;
    stats_.duplicate_pieces = 0;
    stats_.corrupted_pieces = 0;
    stats_.expired_pieces = 0;
    
    double total_quality_score = 0.0;
    int pieces_with_quality = 0;
    
    std::lock_guard<std::mutex> pieces_lock(pieces_mutex_);
    for (const auto& pair : pieces_) {
        for (const auto& piece_pair : pair.second) {
            const auto& piece = piece_pair.second;
            stats_.total_pieces++;
            
            switch (piece->status) {
                case PieceStatus::MISSING:
                    stats_.missing_pieces++;
                    break;
                case PieceStatus::REQUESTED:
                    stats_.requested_pieces++;
                    break;
                case PieceStatus::RECEIVED:
                    stats_.received_pieces++;
                    break;
                case PieceStatus::VALIDATED:
                    stats_.validated_pieces++;
                    break;
                case PieceStatus::INVALID:
                    stats_.invalid_pieces++;
                    break;
                case PieceStatus::DUPLICATE:
                    stats_.duplicate_pieces++;
                    break;
                case PieceStatus::CORRUPTED:
                    stats_.corrupted_pieces++;
                    break;
                case PieceStatus::EXPIRED:
                    stats_.expired_pieces++;
                    break;
            }
            
            total_quality_score += piece->quality_score;
            pieces_with_quality++;
        }
    }
    
    if (pieces_with_quality > 0) {
        stats_.avg_quality_score = total_quality_score / pieces_with_quality;
    }
    
    stats_.last_update = std::chrono::steady_clock::now();
}

std::string MetadataPieceManager::calculatePieceChecksum(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(data.data(), data.size(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string MetadataPieceManager::calculateAssemblyChecksum(const std::vector<uint8_t>& data) {
    return calculatePieceChecksum(data);
}

bool MetadataPieceManager::validatePieceData(const std::vector<uint8_t>& data) {
    return !data.empty() && data.size() <= config_.max_piece_size;
}

bool MetadataPieceManager::validateAssemblyData(const std::vector<uint8_t>& data) {
    return !data.empty() && data.size() <= config_.max_total_size;
}

bool MetadataPieceManager::isPieceExpired(const MetadataPiece& piece) {
    auto now = std::chrono::steady_clock::now();
    return now > piece.expires_at;
}

bool MetadataPieceManager::isAssemblyExpired(const MetadataAssembly& assembly) {
    auto now = std::chrono::steady_clock::now();
    return now > assembly.expires_at;
}

void MetadataPieceManager::updatePieceQuality(std::shared_ptr<MetadataPiece> piece, bool success) {
    if (success) {
        piece->successful_validations++;
    } else {
        piece->failed_validations++;
    }
    
    piece->validation_attempts++;
    
    if (piece->validation_attempts > 0) {
        piece->quality_score = static_cast<double>(piece->successful_validations) / piece->validation_attempts;
    }
}

void MetadataPieceManager::updateAssemblyQuality(std::shared_ptr<MetadataAssembly> assembly) {
    if (assembly->total_pieces > 0) {
        assembly->completeness_ratio = static_cast<double>(assembly->received_pieces) / assembly->total_pieces;
        assembly->validity_ratio = static_cast<double>(assembly->validated_pieces) / assembly->total_pieces;
        assembly->quality_score = (assembly->completeness_ratio + assembly->validity_ratio) / 2.0;
    }
}

MetadataPieceManager::PieceStatus MetadataPieceManager::determinePieceStatus(const MetadataPiece& piece) {
    if (piece.status == PieceStatus::RECEIVED && piece.data.empty()) {
        return PieceStatus::MISSING;
    }
    
    if (piece.status == PieceStatus::RECEIVED && !piece.data.empty()) {
        if (validatePieceData(piece.data)) {
            return PieceStatus::VALIDATED;
        } else {
            return PieceStatus::INVALID;
        }
    }
    
    return piece.status;
}

MetadataPieceManager::AssemblyStatus MetadataPieceManager::determineAssemblyStatus(const MetadataAssembly& assembly) {
    if (assembly.received_pieces >= assembly.total_pieces) {
        if (assembly.validated_pieces >= assembly.total_pieces) {
            return AssemblyStatus::VALIDATED;
        } else {
            return AssemblyStatus::COMPLETE;
        }
    } else {
        return AssemblyStatus::INCOMPLETE;
    }
}

bool MetadataPieceManager::addPiece(const std::string& info_hash, int piece_index, const std::vector<uint8_t>& data) {
    if (!isValidInfoHash(info_hash) || !isValidPieceIndex(piece_index) || !isValidPieceData(data)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto piece = std::make_shared<MetadataPiece>();
    piece->info_hash = info_hash;
    piece->piece_index = piece_index;
    piece->status = PieceStatus::RECEIVED;
    piece->data = data;
    piece->checksum = calculatePieceChecksum(data);
    piece->created_at = std::chrono::steady_clock::now();
    piece->updated_at = piece->created_at;
    piece->expires_at = piece->created_at + std::chrono::milliseconds(config_.piece_timeout);
    piece->request_count = 0;
    piece->retry_count = 0;
    piece->quality_score = 1.0;
    piece->validation_attempts = 0;
    piece->successful_validations = 0;
    piece->failed_validations = 0;
    
    pieces_[info_hash][piece_index] = piece;
    
    return true;
}

bool MetadataPieceManager::requestPiece(const std::string& info_hash, int piece_index) {
    if (!isValidInfoHash(info_hash) || !isValidPieceIndex(piece_index)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto& info_pieces = pieces_[info_hash];
    auto it = info_pieces.find(piece_index);
    
    if (it != info_pieces.end()) {
        if (it->second->status == PieceStatus::REQUESTED) {
            return false; // Already requested
        }
        it->second->status = PieceStatus::REQUESTED;
        it->second->request_count++;
        it->second->last_request = std::chrono::steady_clock::now();
    } else {
        auto piece = std::make_shared<MetadataPiece>();
        piece->info_hash = info_hash;
        piece->piece_index = piece_index;
        piece->status = PieceStatus::REQUESTED;
        piece->created_at = std::chrono::steady_clock::now();
        piece->updated_at = piece->created_at;
        piece->expires_at = piece->created_at + std::chrono::milliseconds(config_.piece_timeout);
        piece->request_count = 1;
        piece->retry_count = 0;
        piece->last_request = piece->created_at;
        piece->quality_score = 0.0;
        piece->validation_attempts = 0;
        piece->successful_validations = 0;
        piece->failed_validations = 0;
        
        info_pieces[piece_index] = piece;
    }
    
    return true;
}

bool MetadataPieceManager::markPieceReceived(const std::string& info_hash, int piece_index, const std::vector<uint8_t>& data) {
    if (!isValidInfoHash(info_hash) || !isValidPieceIndex(piece_index) || !isValidPieceData(data)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto& info_pieces = pieces_[info_hash];
    auto it = info_pieces.find(piece_index);
    
    if (it != info_pieces.end()) {
        it->second->status = PieceStatus::RECEIVED;
        it->second->data = data;
        it->second->checksum = calculatePieceChecksum(data);
        it->second->updated_at = std::chrono::steady_clock::now();
        it->second->expires_at = it->second->updated_at + std::chrono::milliseconds(config_.piece_timeout);
        
        updatePieceQuality(it->second, true);
        
        return true;
    }
    
    return false;
}

bool MetadataPieceManager::markPieceInvalid(const std::string& info_hash, int piece_index) {
    if (!isValidInfoHash(info_hash) || !isValidPieceIndex(piece_index)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto& info_pieces = pieces_[info_hash];
    auto it = info_pieces.find(piece_index);
    
    if (it != info_pieces.end()) {
        it->second->status = PieceStatus::INVALID;
        it->second->updated_at = std::chrono::steady_clock::now();
        
        updatePieceQuality(it->second, false);
        
        return true;
    }
    
    return false;
}

bool MetadataPieceManager::markPieceDuplicate(const std::string& info_hash, int piece_index) {
    if (!isValidInfoHash(info_hash) || !isValidPieceIndex(piece_index)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto& info_pieces = pieces_[info_hash];
    auto it = info_pieces.find(piece_index);
    
    if (it != info_pieces.end()) {
        it->second->status = PieceStatus::DUPLICATE;
        it->second->updated_at = std::chrono::steady_clock::now();
        
        return true;
    }
    
    return false;
}

bool MetadataPieceManager::markPieceCorrupted(const std::string& info_hash, int piece_index) {
    if (!isValidInfoHash(info_hash) || !isValidPieceIndex(piece_index)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto& info_pieces = pieces_[info_hash];
    auto it = info_pieces.find(piece_index);
    
    if (it != info_pieces.end()) {
        it->second->status = PieceStatus::CORRUPTED;
        it->second->updated_at = std::chrono::steady_clock::now();
        
        updatePieceQuality(it->second, false);
        
        return true;
    }
    
    return false;
}

std::shared_ptr<MetadataPieceManager::MetadataPiece> MetadataPieceManager::getPiece(const std::string& info_hash, int piece_index) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto info_it = pieces_.find(info_hash);
    if (info_it != pieces_.end()) {
        auto piece_it = info_it->second.find(piece_index);
        if (piece_it != info_it->second.end()) {
            return piece_it->second;
        }
    }
    
    return nullptr;
}

std::map<int, std::shared_ptr<MetadataPieceManager::MetadataPiece>> MetadataPieceManager::getPieces(const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto it = pieces_.find(info_hash);
    if (it != pieces_.end()) {
        return it->second;
    }
    
    return {};
}

std::vector<int> MetadataPieceManager::getPiecesByStatus(const std::string& info_hash, PieceStatus status) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    std::vector<int> indices;
    auto it = pieces_.find(info_hash);
    if (it != pieces_.end()) {
        for (const auto& piece_pair : it->second) {
            if (piece_pair.second->status == status) {
                indices.push_back(piece_pair.first);
            }
        }
    }
    
    return indices;
}

std::vector<int> MetadataPieceManager::getMissingPieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::MISSING);
}

std::vector<int> MetadataPieceManager::getRequestedPieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::REQUESTED);
}

std::vector<int> MetadataPieceManager::getReceivedPieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::RECEIVED);
}

std::vector<int> MetadataPieceManager::getValidatedPieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::VALIDATED);
}

std::vector<int> MetadataPieceManager::getInvalidPieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::INVALID);
}

std::vector<int> MetadataPieceManager::getDuplicatePieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::DUPLICATE);
}

std::vector<int> MetadataPieceManager::getCorruptedPieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::CORRUPTED);
}

std::vector<int> MetadataPieceManager::getExpiredPieces(const std::string& info_hash) {
    return getPiecesByStatus(info_hash, PieceStatus::EXPIRED);
}

bool MetadataPieceManager::hasPiece(const std::string& info_hash, int piece_index) {
    return getPiece(info_hash, piece_index) != nullptr;
}

bool MetadataPieceManager::isPieceComplete(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && !piece->data.empty();
}

bool MetadataPieceManager::isPieceValid(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::VALIDATED;
}

bool MetadataPieceManager::isPieceMissing(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::MISSING;
}

bool MetadataPieceManager::isPieceRequested(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::REQUESTED;
}

bool MetadataPieceManager::isPieceReceived(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::RECEIVED;
}

bool MetadataPieceManager::isPieceInvalid(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::INVALID;
}

bool MetadataPieceManager::isPieceDuplicate(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::DUPLICATE;
}

bool MetadataPieceManager::isPieceCorrupted(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::CORRUPTED;
}

bool MetadataPieceManager::isPieceExpired(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece && piece->status == PieceStatus::EXPIRED;
}

MetadataPieceManager::PieceStatus MetadataPieceManager::getPieceStatus(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->status : PieceStatus::MISSING;
}

std::vector<uint8_t> MetadataPieceManager::getPieceData(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->data : std::vector<uint8_t>{};
}

std::string MetadataPieceManager::getPieceChecksum(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->checksum : "";
}

double MetadataPieceManager::getPieceQualityScore(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->quality_score : 0.0;
}

int MetadataPieceManager::getPieceRequestCount(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->request_count : 0;
}

int MetadataPieceManager::getPieceRetryCount(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->retry_count : 0;
}

std::chrono::steady_clock::time_point MetadataPieceManager::getPieceCreationTime(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->created_at : std::chrono::steady_clock::time_point{};
}

std::chrono::steady_clock::time_point MetadataPieceManager::getPieceUpdateTime(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->updated_at : std::chrono::steady_clock::time_point{};
}

std::chrono::steady_clock::time_point MetadataPieceManager::getPieceExpirationTime(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->expires_at : std::chrono::steady_clock::time_point{};
}

std::map<std::string, std::string> MetadataPieceManager::getPieceMetadata(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    return piece ? piece->metadata : std::map<std::string, std::string>{};
}

bool MetadataPieceManager::setPieceMetadata(const std::string& info_hash, int piece_index, const std::string& key, const std::string& value) {
    auto piece = getPiece(info_hash, piece_index);
    if (piece) {
        piece->metadata[key] = value;
        return true;
    }
    return false;
}

bool MetadataPieceManager::removePieceMetadata(const std::string& info_hash, int piece_index, const std::string& key) {
    auto piece = getPiece(info_hash, piece_index);
    if (piece) {
        piece->metadata.erase(key);
        return true;
    }
    return false;
}

bool MetadataPieceManager::clearPieceMetadata(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    if (piece) {
        piece->metadata.clear();
        return true;
    }
    return false;
}

int MetadataPieceManager::getPieceCount(const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto it = pieces_.find(info_hash);
    if (it != pieces_.end()) {
        return static_cast<int>(it->second.size());
    }
    
    return 0;
}

int MetadataPieceManager::getPieceCountByStatus(const std::string& info_hash, PieceStatus status) {
    return static_cast<int>(getPiecesByStatus(info_hash, status).size());
}

int MetadataPieceManager::getTotalPieceCount() {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    int count = 0;
    for (const auto& pair : pieces_) {
        count += static_cast<int>(pair.second.size());
    }
    
    return count;
}

int MetadataPieceManager::getTotalPieceCountByStatus(PieceStatus status) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    int count = 0;
    for (const auto& pair : pieces_) {
        for (const auto& piece_pair : pair.second) {
            if (piece_pair.second->status == status) {
                count++;
            }
        }
    }
    
    return count;
}

double MetadataPieceManager::getPieceCompletionRatio(const std::string& info_hash) {
    int total_pieces = getPieceCount(info_hash);
    if (total_pieces == 0) {
        return 0.0;
    }
    
    int received_pieces = getPieceCountByStatus(info_hash, PieceStatus::RECEIVED);
    int validated_pieces = getPieceCountByStatus(info_hash, PieceStatus::VALIDATED);
    
    return static_cast<double>(received_pieces + validated_pieces) / total_pieces;
}

double MetadataPieceManager::getPieceValidityRatio(const std::string& info_hash) {
    int total_pieces = getPieceCount(info_hash);
    if (total_pieces == 0) {
        return 0.0;
    }
    
    int validated_pieces = getPieceCountByStatus(info_hash, PieceStatus::VALIDATED);
    
    return static_cast<double>(validated_pieces) / total_pieces;
}

double MetadataPieceManager::getPieceQualityRatio(const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto it = pieces_.find(info_hash);
    if (it == pieces_.end() || it->second.empty()) {
        return 0.0;
    }
    
    double total_quality = 0.0;
    for (const auto& piece_pair : it->second) {
        total_quality += piece_pair.second->quality_score;
    }
    
    return total_quality / it->second.size();
}

std::map<std::string, int> MetadataPieceManager::getPieceStatistics(const std::string& info_hash) {
    std::map<std::string, int> stats;
    
    stats["total_pieces"] = getPieceCount(info_hash);
    stats["missing_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::MISSING);
    stats["requested_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::REQUESTED);
    stats["received_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::RECEIVED);
    stats["validated_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::VALIDATED);
    stats["invalid_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::INVALID);
    stats["duplicate_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::DUPLICATE);
    stats["corrupted_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::CORRUPTED);
    stats["expired_pieces"] = getPieceCountByStatus(info_hash, PieceStatus::EXPIRED);
    
    return stats;
}

MetadataPieceManager::PieceManagerStatistics MetadataPieceManager::getStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void MetadataPieceManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = PieceManagerStatistics{};
}

void MetadataPieceManager::updateConfig(const PieceConfig& config) {
    config_ = config;
}

MetadataPieceManager::PieceConfig MetadataPieceManager::getConfig() const {
    return config_;
}

void MetadataPieceManager::start() {
    should_stop_ = false;
    if (!piece_monitor_thread_.joinable()) {
        piece_monitor_thread_ = std::thread(&MetadataPieceManager::pieceMonitorLoop, this);
    }
}

void MetadataPieceManager::stop() {
    should_stop_ = true;
    monitor_condition_.notify_all();
}

bool MetadataPieceManager::isRunning() const {
    return !should_stop_ && piece_monitor_thread_.joinable();
}

bool MetadataPieceManager::exportPieces(const std::string& info_hash, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto it = pieces_.find(info_hash);
    if (it != pieces_.end()) {
        file << "PieceIndex,Status,DataSize,Checksum,QualityScore,RequestCount,RetryCount" << std::endl;
        
        for (const auto& piece_pair : it->second) {
            const auto& piece = piece_pair.second;
            file << piece->piece_index << "," << static_cast<int>(piece->status) << ","
                 << piece->data.size() << "," << piece->checksum << "," << piece->quality_score << ","
                 << piece->request_count << "," << piece->retry_count << std::endl;
        }
    }
    
    file.close();
    return true;
}

bool MetadataPieceManager::exportStatistics(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    file << "Metric,Value" << std::endl;
    file << "TotalPieces," << stats_.total_pieces << std::endl;
    file << "MissingPieces," << stats_.missing_pieces << std::endl;
    file << "RequestedPieces," << stats_.requested_pieces << std::endl;
    file << "ReceivedPieces," << stats_.received_pieces << std::endl;
    file << "ValidatedPieces," << stats_.validated_pieces << std::endl;
    file << "InvalidPieces," << stats_.invalid_pieces << std::endl;
    file << "DuplicatePieces," << stats_.duplicate_pieces << std::endl;
    file << "CorruptedPieces," << stats_.corrupted_pieces << std::endl;
    file << "ExpiredPieces," << stats_.expired_pieces << std::endl;
    file << "AverageQualityScore," << stats_.avg_quality_score << std::endl;
    
    file.close();
    return true;
}

std::map<std::string, std::string> MetadataPieceManager::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["total_pieces"] = std::to_string(getTotalPieceCount());
    status["missing_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::MISSING));
    status["requested_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::REQUESTED));
    status["received_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::RECEIVED));
    status["validated_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::VALIDATED));
    status["invalid_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::INVALID));
    status["duplicate_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::DUPLICATE));
    status["corrupted_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::CORRUPTED));
    status["expired_pieces"] = std::to_string(getTotalPieceCountByStatus(PieceStatus::EXPIRED));
    status["average_quality_score"] = std::to_string(stats_.avg_quality_score);
    status["piece_timeout"] = std::to_string(config_.piece_timeout);
    status["assembly_timeout"] = std::to_string(config_.assembly_timeout);
    status["max_piece_size"] = std::to_string(config_.max_piece_size);
    status["max_total_size"] = std::to_string(config_.max_total_size);
    status["is_running"] = isRunning() ? "true" : "false";
    
    return status;
}

int MetadataPieceManager::clearPieces(const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto it = pieces_.find(info_hash);
    if (it != pieces_.end()) {
        int count = static_cast<int>(it->second.size());
        pieces_.erase(it);
        return count;
    }
    
    return 0;
}

int MetadataPieceManager::clearAllPieces() {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    int total_count = 0;
    for (const auto& pair : pieces_) {
        total_count += static_cast<int>(pair.second.size());
    }
    
    pieces_.clear();
    return total_count;
}

int MetadataPieceManager::clearExpiredPieces() {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    int cleared_count = 0;
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : pieces_) {
        for (auto it = pair.second.begin(); it != pair.second.end();) {
            if (isPieceExpired(*it->second)) {
                it = pair.second.erase(it);
                cleared_count++;
            } else {
                ++it;
            }
        }
    }
    
    return cleared_count;
}

void MetadataPieceManager::forceCleanup() {
    clearExpiredPieces();
    clearExpiredAssemblies();
}

std::map<std::string, int> MetadataPieceManager::getPieceStatisticsByInfoHash() {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    std::map<std::string, int> stats;
    for (const auto& pair : pieces_) {
        stats[pair.first] = static_cast<int>(pair.second.size());
    }
    
    return stats;
}

std::map<MetadataPieceManager::PieceStatus, int> MetadataPieceManager::getPieceStatisticsByStatus() {
    std::map<PieceStatus, int> stats;
    
    stats[PieceStatus::MISSING] = getTotalPieceCountByStatus(PieceStatus::MISSING);
    stats[PieceStatus::REQUESTED] = getTotalPieceCountByStatus(PieceStatus::REQUESTED);
    stats[PieceStatus::RECEIVED] = getTotalPieceCountByStatus(PieceStatus::RECEIVED);
    stats[PieceStatus::VALIDATED] = getTotalPieceCountByStatus(PieceStatus::VALIDATED);
    stats[PieceStatus::INVALID] = getTotalPieceCountByStatus(PieceStatus::INVALID);
    stats[PieceStatus::DUPLICATE] = getTotalPieceCountByStatus(PieceStatus::DUPLICATE);
    stats[PieceStatus::CORRUPTED] = getTotalPieceCountByStatus(PieceStatus::CORRUPTED);
    stats[PieceStatus::EXPIRED] = getTotalPieceCountByStatus(PieceStatus::EXPIRED);
    
    return stats;
}

std::vector<std::string> MetadataPieceManager::getPieceManagerRecommendations() {
    std::vector<std::string> recommendations;
    
    if (getTotalPieceCount() > config_.max_total_size / config_.max_piece_size * 10) {
        recommendations.push_back("Consider reducing piece collection frequency");
    }
    
    if (getTotalPieceCountByStatus(PieceStatus::INVALID) > getTotalPieceCount() * 0.3) {
        recommendations.push_back("High invalid piece rate, check validation logic");
    }
    
    if (getTotalPieceCountByStatus(PieceStatus::CORRUPTED) > getTotalPieceCount() * 0.1) {
        recommendations.push_back("High corruption rate, check data integrity");
    }
    
    return recommendations;
}

void MetadataPieceManager::optimizePieceManager() {
    clearExpiredPieces();
    clearExpiredAssemblies();
}

std::map<std::string, double> MetadataPieceManager::getPieceManagerPerformanceMetrics() {
    std::map<std::string, double> metrics;
    
    metrics["total_pieces"] = getTotalPieceCount();
    metrics["average_quality_score"] = stats_.avg_quality_score;
    metrics["completion_ratio"] = getPieceCompletionRatio("");
    metrics["validity_ratio"] = getPieceValidityRatio("");
    
    return metrics;
}

bool MetadataPieceManager::forcePieceValidation(const std::string& info_hash, int piece_index) {
    auto piece = getPiece(info_hash, piece_index);
    if (piece && !piece->data.empty()) {
        if (validatePieceData(piece->data)) {
            piece->status = PieceStatus::VALIDATED;
            updatePieceQuality(piece, true);
            return true;
        } else {
            piece->status = PieceStatus::INVALID;
            updatePieceQuality(piece, false);
            return false;
        }
    }
    return false;
}

bool MetadataPieceManager::forcePieceCleanup(const std::string& info_hash, int piece_index) {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    auto it = pieces_.find(info_hash);
    if (it != pieces_.end()) {
        auto piece_it = it->second.find(piece_index);
        if (piece_it != it->second.end()) {
            it->second.erase(piece_it);
            return true;
        }
    }
    
    return false;
}

std::map<std::string, int> MetadataPieceManager::getPieceManagerCapacity() {
    std::map<std::string, int> capacity;
    
    capacity["current_pieces"] = getTotalPieceCount();
    capacity["max_piece_size"] = static_cast<int>(config_.max_piece_size);
    capacity["max_total_size"] = static_cast<int>(config_.max_total_size);
    capacity["available_capacity"] = static_cast<int>(config_.max_total_size - getTotalPieceCount());
    capacity["utilization_percent"] = static_cast<int>((getTotalPieceCount() / static_cast<double>(config_.max_total_size)) * 100);
    
    return capacity;
}

bool MetadataPieceManager::validatePieceManagerIntegrity() {
    std::lock_guard<std::mutex> lock(pieces_mutex_);
    
    for (const auto& pair : pieces_) {
        for (const auto& piece_pair : pair.second) {
            const auto& piece = piece_pair.second;
            if (piece->data.size() > config_.max_piece_size) {
                return false;
            }
            if (piece->quality_score < 0.0 || piece->quality_score > 1.0) {
                return false;
            }
        }
    }
    
    return true;
}

std::vector<std::string> MetadataPieceManager::getPieceManagerIssues() {
    std::vector<std::string> issues;
    
    if (!validatePieceManagerIntegrity()) {
        issues.push_back("Piece manager integrity check failed");
    }
    
    if (getTotalPieceCount() > config_.max_total_size / config_.max_piece_size * 10) {
        issues.push_back("Too many pieces collected");
    }
    
    if (getTotalPieceCountByStatus(PieceStatus::INVALID) > getTotalPieceCount() * 0.5) {
        issues.push_back("High invalid piece rate");
    }
    
    return issues;
}

int MetadataPieceManager::repairPieceManager() {
    int repaired_count = 0;
    
    if (!validatePieceManagerIntegrity()) {
        repaired_count += clearAllPieces();
    }
    
    if (getTotalPieceCount() > config_.max_total_size / config_.max_piece_size * 10) {
        repaired_count += clearExpiredPieces();
    }
    
    return repaired_count;
}

bool MetadataPieceManager::isValidInfoHash(const std::string& info_hash) {
    return info_hash.length() == 40; // SHA1 hash is 40 characters
}

bool MetadataPieceManager::isValidPieceIndex(int piece_index) {
    return piece_index >= 0;
}

bool MetadataPieceManager::isValidPieceData(const std::vector<uint8_t>& data) {
    return !data.empty() && data.size() <= config_.max_piece_size;
}

bool MetadataPieceManager::isValidAssemblyData(const std::vector<uint8_t>& data) {
    return !data.empty() && data.size() <= config_.max_total_size;
}

} // namespace dht_crawler
