#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>
#include <random>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace dht_crawler {

/**
 * Cryptographic token manager for DHT operations
 * Implements Tribler-inspired token-based authentication with SHA1 hash validation
 * Reference: py-ipv8/dht/community.py - Token Management
 */
class DHTTokenManager {
public:
    struct TokenConfig {
        std::chrono::seconds token_expiration_time{600};  // 600s like Tribler
        std::chrono::seconds token_refresh_interval{300}; // 300s refresh interval
        std::string secret_key = "dht_crawler_secret_key";
        bool enable_secret_rotation = true;
        std::chrono::hours secret_rotation_interval{24}; // 24 hours
        int max_tokens_per_node = 100;
        bool enable_token_caching = true;
        size_t token_cache_size = 1000;
    };

    struct TokenInfo {
        std::string token;
        std::string node_id;
        std::string node_ip;
        int node_port;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point expires_at;
        std::string secret_version;
        bool is_valid = true;
        int usage_count = 0;
        std::chrono::steady_clock::time_point last_used;
    };

    struct TokenValidationResult {
        bool is_valid = false;
        std::string error_message;
        std::string token_info;
        std::chrono::steady_clock::time_point validated_at;
        TokenInfo token_details;
    };

    struct TokenStatistics {
        int total_tokens_generated = 0;
        int total_tokens_validated = 0;
        int valid_tokens = 0;
        int expired_tokens = 0;
        int invalid_tokens = 0;
        int tokens_refreshed = 0;
        std::chrono::milliseconds avg_generation_time;
        std::chrono::milliseconds avg_validation_time;
        std::map<std::string, int> tokens_by_node;
        std::map<std::string, int> validation_errors;
    };

private:
    TokenConfig config_;
    std::map<std::string, TokenInfo> token_cache_;
    std::mutex token_cache_mutex_;
    
    std::map<std::string, std::string> node_secrets_;
    std::mutex secrets_mutex_;
    
    std::string current_secret_;
    std::string previous_secret_;
    std::chrono::steady_clock::time_point last_secret_rotation_;
    std::mutex secret_mutex_;
    
    TokenStatistics stats_;
    std::mutex stats_mutex_;
    
    std::thread token_cleanup_thread_;
    std::atomic<bool> should_stop_{false};
    
    // Internal methods
    std::string generateToken(const std::string& node_id, const std::string& node_ip, int node_port);
    std::string calculateTokenHash(const std::string& node_info, const std::string& secret);
    std::string getNodeInfo(const std::string& node_id, const std::string& node_ip, int node_port);
    bool validateTokenHash(const std::string& token, const std::string& node_info, const std::string& secret);
    void rotateSecret();
    void cleanupExpiredTokens();
    void updateStatistics(const std::string& operation, bool success, std::chrono::milliseconds duration);
    std::string getCurrentSecret();
    std::string getPreviousSecret();
    bool isTokenExpired(const TokenInfo& token_info);

public:
    DHTTokenManager(const TokenConfig& config = TokenConfig{});
    ~DHTTokenManager();
    
    /**
     * Generate a token for a DHT node
     * @param node_id Node ID
     * @param node_ip Node IP address
     * @param node_port Node port
     * @return Generated token
     */
    std::string generateTokenForNode(const std::string& node_id, 
                                   const std::string& node_ip, 
                                   int node_port);
    
    /**
     * Validate a token from a DHT node
     * @param token Token to validate
     * @param node_id Node ID
     * @param node_ip Node IP address
     * @param node_port Node port
     * @return Token validation result
     */
    TokenValidationResult validateToken(const std::string& token,
                                      const std::string& node_id,
                                      const std::string& node_ip,
                                      int node_port);
    
    /**
     * Refresh a token for a node
     * @param node_id Node ID
     * @param node_ip Node IP address
     * @param node_port Node port
     * @return New token
     */
    std::string refreshToken(const std::string& node_id,
                           const std::string& node_ip,
                           int node_port);
    
    /**
     * Revoke a token
     * @param token Token to revoke
     * @return true if token was revoked successfully
     */
    bool revokeToken(const std::string& token);
    
    /**
     * Revoke all tokens for a node
     * @param node_id Node ID
     * @return Number of tokens revoked
     */
    int revokeTokensForNode(const std::string& node_id);
    
    /**
     * Check if a token is valid
     * @param token Token to check
     * @return true if token is valid
     */
    bool isTokenValid(const std::string& token);
    
    /**
     * Get token information
     * @param token Token to get info for
     * @return Token information or nullptr if not found
     */
    std::shared_ptr<TokenInfo> getTokenInfo(const std::string& token);
    
    /**
     * Get all tokens for a node
     * @param node_id Node ID
     * @return Vector of token information
     */
    std::vector<TokenInfo> getTokensForNode(const std::string& node_id);
    
    /**
     * Get token statistics
     * @return Current token statistics
     */
    TokenStatistics getStatistics();
    
    /**
     * Reset token statistics
     */
    void resetStatistics();
    
    /**
     * Update token configuration
     * @param config New token configuration
     */
    void updateConfig(const TokenConfig& config);
    
    /**
     * Force secret rotation
     */
    void forceSecretRotation();
    
    /**
     * Get current secret version
     * @return Current secret version
     */
    std::string getCurrentSecretVersion();
    
    /**
     * Get token cache size
     * @return Number of tokens in cache
     */
    size_t getTokenCacheSize();
    
    /**
     * Clear token cache
     */
    void clearTokenCache();
    
    /**
     * Start token manager
     */
    void start();
    
    /**
     * Stop token manager
     */
    void stop();
    
    /**
     * Check if token manager is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Get token expiration time
     * @return Token expiration time in seconds
     */
    std::chrono::seconds getTokenExpirationTime();
    
    /**
     * Set token expiration time
     * @param expiration_time New expiration time
     */
    void setTokenExpirationTime(std::chrono::seconds expiration_time);
    
    /**
     * Get token refresh interval
     * @return Token refresh interval in seconds
     */
    std::chrono::seconds getTokenRefreshInterval();
    
    /**
     * Set token refresh interval
     * @param refresh_interval New refresh interval
     */
    void setTokenRefreshInterval(std::chrono::seconds refresh_interval);
    
    /**
     * Enable or disable secret rotation
     * @param enable true to enable secret rotation
     */
    void setSecretRotationEnabled(bool enable);
    
    /**
     * Check if secret rotation is enabled
     * @return true if secret rotation is enabled
     */
    bool isSecretRotationEnabled();
    
    /**
     * Get secret rotation interval
     * @return Secret rotation interval in hours
     */
    std::chrono::hours getSecretRotationInterval();
    
    /**
     * Set secret rotation interval
     * @param rotation_interval New rotation interval
     */
    void setSecretRotationInterval(std::chrono::hours rotation_interval);
    
    /**
     * Get token generation success rate
     * @return Success rate (0.0 to 1.0)
     */
    double getTokenGenerationSuccessRate();
    
    /**
     * Get token validation success rate
     * @return Success rate (0.0 to 1.0)
     */
    double getTokenValidationSuccessRate();
    
    /**
     * Get token usage statistics for a node
     * @param node_id Node ID
     * @return Token usage statistics
     */
    std::map<std::string, int> getNodeTokenUsage(const std::string& node_id);
    
    /**
     * Get token cache hit rate
     * @return Cache hit rate (0.0 to 1.0)
     */
    double getTokenCacheHitRate();
    
    /**
     * Export token cache to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportTokenCache(const std::string& filename);
    
    /**
     * Import token cache from file
     * @param filename Input filename
     * @return true if import was successful
     */
    bool importTokenCache(const std::string& filename);
    
    /**
     * Clean up expired tokens
     */
    void cleanupExpiredTokens();
    
    /**
     * Get token manager health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
};

/**
 * Token manager factory for different token strategies
 */
class DHTTokenManagerFactory {
public:
    enum class TokenStrategy {
        STRICT,      // Strict token validation with short expiration
        MODERATE,    // Moderate token validation with balanced expiration
        LENIENT,     // Lenient token validation with long expiration
        CUSTOM       // Custom token configuration
    };
    
    static std::unique_ptr<DHTTokenManager> createTokenManager(
        TokenStrategy strategy,
        const DHTTokenManager::TokenConfig& config = DHTTokenManager::TokenConfig{}
    );
    
    static DHTTokenManager::TokenConfig getDefaultConfig(TokenStrategy strategy);
};

/**
 * RAII token wrapper for automatic token management
 */
class TokenGuard {
private:
    std::shared_ptr<DHTTokenManager> token_manager_;
    std::string token_;
    std::string node_id_;
    std::string node_ip_;
    int node_port_;
    bool generated_;

public:
    TokenGuard(std::shared_ptr<DHTTokenManager> token_manager,
               const std::string& node_id,
               const std::string& node_ip,
               int node_port);
    
    ~TokenGuard();
    
    /**
     * Get the token
     * @return Token string
     */
    const std::string& getToken() const;
    
    /**
     * Refresh the token
     * @return New token
     */
    std::string refresh();
    
    /**
     * Revoke the token
     */
    void revoke();
    
    /**
     * Check if token is valid
     * @return true if token is valid
     */
    bool isValid();
    
    /**
     * Get token information
     * @return Token information
     */
    std::shared_ptr<DHTTokenManager::TokenInfo> getTokenInfo();
};

/**
 * Token validation context for request processing
 */
class TokenValidationContext {
private:
    std::shared_ptr<DHTTokenManager> token_manager_;
    DHTTokenManager::TokenValidationResult validation_result_;
    bool validated_;

public:
    TokenValidationContext(std::shared_ptr<DHTTokenManager> token_manager,
                          const std::string& token,
                          const std::string& node_id,
                          const std::string& node_ip,
                          int node_port);
    
    /**
     * Check if validation was successful
     * @return true if validation was successful
     */
    bool isValid();
    
    /**
     * Get validation error message
     * @return Error message
     */
    std::string getErrorMessage();
    
    /**
     * Get token information
     * @return Token information
     */
    DHTTokenManager::TokenInfo getTokenInfo();
    
    /**
     * Get validation result
     * @return Complete validation result
     */
    const DHTTokenManager::TokenValidationResult& getValidationResult();
};

} // namespace dht_crawler
