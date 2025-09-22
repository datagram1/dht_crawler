#include "dht_token_manager.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/hmac.h>

namespace dht_crawler {

DHTTokenManager::DHTTokenManager(const TokenConfig& config) : config_(config) {
    // Initialize random number generator
    std::random_device rd;
    rng_.seed(rd());
}

std::string DHTTokenManager::generateToken(const std::string& node_id, const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    // Create token data
    std::string token_data = node_id + transaction_id + std::to_string(std::time(nullptr));
    
    // Generate random salt
    std::string salt = generateRandomSalt();
    
    // Create HMAC-SHA1 token
    std::string token = createHMACToken(token_data, salt);
    
    // Store token info
    TokenInfo token_info;
    token_info.token = token;
    token_info.node_id = node_id;
    token_info.transaction_id = transaction_id;
    token_info.created_at = std::chrono::steady_clock::now();
    token_info.expires_at = token_info.created_at + std::chrono::milliseconds(config_.token_lifetime);
    token_info.salt = salt;
    token_info.usage_count = 0;
    
    active_tokens_[token] = token_info;
    
    return token;
}

bool DHTTokenManager::validateToken(const std::string& token, const std::string& node_id) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    auto it = active_tokens_.find(token);
    if (it == active_tokens_.end()) {
        return false;
    }
    
    TokenInfo& token_info = it->second;
    
    // Check if token is expired
    auto now = std::chrono::steady_clock::now();
    if (now > token_info.expires_at) {
        // Move to expired tokens
        expired_tokens_[token] = token_info;
        active_tokens_.erase(it);
        return false;
    }
    
    // Check if node ID matches
    if (token_info.node_id != node_id) {
        return false;
    }
    
    // Increment usage count
    token_info.usage_count++;
    
    return true;
}

bool DHTTokenManager::refreshToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    auto it = active_tokens_.find(token);
    if (it == active_tokens_.end()) {
        return false;
    }
    
    TokenInfo& token_info = it->second;
    
    // Extend expiration time
    auto now = std::chrono::steady_clock::now();
    token_info.expires_at = now + std::chrono::milliseconds(config_.token_lifetime);
    
    return true;
}

bool DHTTokenManager::isTokenExpired(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    auto it = active_tokens_.find(token);
    if (it == active_tokens_.end()) {
        // Check expired tokens
        auto expired_it = expired_tokens_.find(token);
        return expired_it != expired_tokens_.end();
    }
    
    auto now = std::chrono::steady_clock::now();
    return now > it->second.expires_at;
}

std::string DHTTokenManager::generateRandomSalt() {
    std::string salt;
    salt.resize(16); // 16 bytes
    
    std::uniform_int_distribution<int> dist(0, 255);
    for (char& c : salt) {
        c = static_cast<char>(dist(rng_));
    }
    
    return salt;
}

std::string DHTTokenManager::createHMACToken(const std::string& data, const std::string& salt) {
    // Create HMAC-SHA1 token
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int hash_len;
    
    HMAC(EVP_sha1(), 
         config_.secret_key.c_str(), config_.secret_key.length(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         hash, &hash_len);
    
    // Convert to hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

DHTTokenManager::TokenInfo DHTTokenManager::getTokenInfo(const std::string& token) {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    auto it = active_tokens_.find(token);
    if (it != active_tokens_.end()) {
        return it->second;
    }
    
    // Check expired tokens
    auto expired_it = expired_tokens_.find(token);
    if (expired_it != expired_tokens_.end()) {
        return expired_it->second;
    }
    
    return TokenInfo{}; // Return empty info if not found
}

std::vector<std::string> DHTTokenManager::getActiveTokens() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    std::vector<std::string> tokens;
    for (const auto& pair : active_tokens_) {
        tokens.push_back(pair.first);
    }
    
    return tokens;
}

std::vector<std::string> DHTTokenManager::getExpiredTokens() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    std::vector<std::string> tokens;
    for (const auto& pair : expired_tokens_) {
        tokens.push_back(pair.first);
    }
    
    return tokens;
}

int DHTTokenManager::getActiveTokenCount() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    return static_cast<int>(active_tokens_.size());
}

int DHTTokenManager::getExpiredTokenCount() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    return static_cast<int>(expired_tokens_.size());
}

int DHTTokenManager::getTotalTokenCount() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    return static_cast<int>(active_tokens_.size() + expired_tokens_.size());
}

void DHTTokenManager::cleanupExpiredTokens() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Move expired tokens from active to expired
    for (auto it = active_tokens_.begin(); it != active_tokens_.end();) {
        if (now > it->second.expires_at) {
            expired_tokens_[it->first] = it->second;
            it = active_tokens_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clean up old expired tokens
    for (auto it = expired_tokens_.begin(); it != expired_tokens_.end();) {
        if (now - it->second.expires_at > std::chrono::milliseconds(config_.cleanup_interval)) {
            it = expired_tokens_.erase(it);
        } else {
            ++it;
        }
    }
}

void DHTTokenManager::clearAllTokens() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    active_tokens_.clear();
    expired_tokens_.clear();
}

void DHTTokenManager::updateConfig(const TokenConfig& config) {
    config_ = config;
}

DHTTokenManager::TokenConfig DHTTokenManager::getConfig() const {
    return config_;
}

std::map<std::string, std::string> DHTTokenManager::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["active_tokens"] = std::to_string(getActiveTokenCount());
    status["expired_tokens"] = std::to_string(getExpiredTokenCount());
    status["total_tokens"] = std::to_string(getTotalTokenCount());
    status["token_lifetime"] = std::to_string(config_.token_lifetime);
    
    return status;
}

} // namespace dht_crawler
