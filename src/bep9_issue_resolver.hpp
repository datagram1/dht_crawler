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

namespace dht_crawler {

/**
 * BEP 9 Issue Resolver for current extension protocol problems
 * Diagnoses why peers connect but fail during extension protocol negotiation
 * Implements extension protocol state machine debugging and failure pattern detection
 */
class BEP9IssueResolver {
public:
    enum class IssueType {
        HANDSHAKE_TIMEOUT,      // Handshake message timeout
        NEGOTIATION_FAILURE,     // Capability negotiation failure
        MESSAGE_PARSE_ERROR,    // Message parsing error
        PROTOCOL_VIOLATION,     // Protocol violation
        CONNECTION_DROP,        // Connection dropped during negotiation
        CAPABILITY_MISMATCH,    // Capability mismatch
        VERSION_INCOMPATIBILITY, // Version incompatibility
        PEER_REJECTION,         // Peer rejected extension protocol
        UNKNOWN_ERROR           // Unknown error
    };

    enum class ResolutionStrategy {
        RETRY_WITH_BACKOFF,     // Retry with exponential backoff
        FALLBACK_TO_BASIC,      // Fallback to basic BitTorrent protocol
        TRY_ALTERNATIVE_PEER,   // Try alternative peer
        ADJUST_TIMEOUTS,        // Adjust timeout values
        MODIFY_HANDSHAKE,       // Modify handshake approach
        SKIP_EXTENSION,         // Skip extension protocol for this peer
        ESCALATE_ERROR,         // Escalate error to higher level
        CUSTOM_RESOLUTION       // Custom resolution strategy
    };

    struct IssueConfig {
        std::chrono::milliseconds handshake_timeout{10000};     // 10 seconds
        std::chrono::milliseconds negotiation_timeout{15000};   // 15 seconds
        std::chrono::milliseconds message_timeout{5000};        // 5 seconds
        int max_retry_attempts = 3;
        std::chrono::milliseconds retry_delay{1000};           // 1 second
        double retry_backoff_multiplier = 2.0;
        bool enable_automatic_resolution = true;
        bool enable_peer_blacklisting = true;
        bool enable_timeout_adjustment = true;
        bool enable_handshake_modification = true;
        std::string log_file_path = "bep9_issues.log";
        size_t max_log_size = 5 * 1024 * 1024; // 5MB
    };

    struct IssueRecord {
        std::string issue_id;
        std::string peer_ip;
        int peer_port;
        std::string peer_id;
        IssueType issue_type;
        std::string issue_description;
        std::chrono::steady_clock::time_point occurred_at;
        std::chrono::steady_clock::time_point resolved_at;
        
        // Context information
        std::string session_id;
        std::string connection_id;
        std::map<std::string, std::string> context_data;
        
        // Resolution information
        ResolutionStrategy resolution_strategy;
        std::string resolution_description;
        bool resolution_successful;
        int resolution_attempts;
        std::chrono::milliseconds resolution_duration;
        
        // State machine information
        std::vector<std::string> state_transitions;
        std::vector<std::string> message_flow;
        std::vector<std::string> error_events;
        
        // Pattern information
        std::string error_pattern;
        std::string failure_category;
        std::map<std::string, std::string> pattern_metadata;
        
        // Metadata
        std::map<std::string, std::string> metadata;
    };

    struct ResolutionStatistics {
        int total_issues = 0;
        int resolved_issues = 0;
        int unresolved_issues = 0;
        int auto_resolved_issues = 0;
        int manual_resolved_issues = 0;
        std::map<IssueType, int> issues_by_type;
        std::map<ResolutionStrategy, int> resolutions_by_strategy;
        std::map<std::string, int> issues_by_peer;
        std::map<std::string, int> issues_by_pattern;
        double resolution_success_rate = 0.0;
        std::chrono::milliseconds avg_resolution_time;
        std::map<IssueType, double> success_rate_by_issue_type;
        std::map<ResolutionStrategy, double> success_rate_by_strategy;
    };

private:
    IssueConfig config_;
    std::map<std::string, std::shared_ptr<IssueRecord>> active_issues_;
    std::mutex issues_mutex_;
    
    std::map<std::string, std::vector<IssueRecord>> issue_history_;
    std::mutex history_mutex_;
    
    std::map<std::string, std::set<std::string>> peer_blacklist_;
    std::mutex blacklist_mutex_;
    
    std::map<std::string, int> peer_issue_counts_;
    std::mutex peer_counts_mutex_;
    
    ResolutionStatistics stats_;
    std::mutex stats_mutex_;
    
    // Pattern detection
    std::map<std::string, std::vector<IssueRecord>> issue_patterns_;
    std::mutex patterns_mutex_;
    
    // Background processing
    std::thread resolution_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable resolution_condition_;
    
    // Internal methods
    void resolutionLoop();
    void processIssueResolution(const std::string& issue_id);
    ResolutionStrategy determineResolutionStrategy(const IssueRecord& issue);
    bool executeResolutionStrategy(const std::string& issue_id, ResolutionStrategy strategy);
    void updateStatistics(const IssueRecord& issue, bool success);
    void detectIssuePatterns(const IssueRecord& issue);
    std::string generateIssueId();
    std::string issueTypeToString(IssueType type);
    std::string resolutionStrategyToString(ResolutionStrategy strategy);
    void addToBlacklist(const std::string& peer_ip, int peer_port, const std::string& reason);
    void removeFromBlacklist(const std::string& peer_ip, int peer_port);
    bool isBlacklisted(const std::string& peer_ip, int peer_port);
    void updatePeerIssueCount(const std::string& peer_ip, int peer_port);

public:
    BEP9IssueResolver(const IssueConfig& config = IssueConfig{});
    ~BEP9IssueResolver();
    
    /**
     * Report a BEP 9 issue
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param issue_type Issue type
     * @param issue_description Issue description
     * @param context_data Additional context data
     * @return Issue ID
     */
    std::string reportIssue(const std::string& peer_ip,
                           int peer_port,
                           const std::string& peer_id,
                           IssueType issue_type,
                           const std::string& issue_description,
                           const std::map<std::string, std::string>& context_data = {});
    
    /**
     * Report a handshake timeout issue
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param timeout_duration Timeout duration
     * @return Issue ID
     */
    std::string reportHandshakeTimeout(const std::string& peer_ip,
                                      int peer_port,
                                      const std::string& peer_id,
                                      std::chrono::milliseconds timeout_duration);
    
    /**
     * Report a negotiation failure issue
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param failure_reason Failure reason
     * @param supported_extensions Supported extensions
     * @return Issue ID
     */
    std::string reportNegotiationFailure(const std::string& peer_ip,
                                        int peer_port,
                                        const std::string& peer_id,
                                        const std::string& failure_reason,
                                        const std::map<std::string, int>& supported_extensions);
    
    /**
     * Report a message parse error
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param parse_error Parse error description
     * @param message_data Raw message data
     * @return Issue ID
     */
    std::string reportMessageParseError(const std::string& peer_ip,
                                       int peer_port,
                                       const std::string& peer_id,
                                       const std::string& parse_error,
                                       const std::string& message_data);
    
    /**
     * Report a connection drop issue
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @param peer_id Peer ID
     * @param drop_reason Drop reason
     * @param connection_duration Connection duration
     * @return Issue ID
     */
    std::string reportConnectionDrop(const std::string& peer_ip,
                                    int peer_port,
                                    const std::string& peer_id,
                                    const std::string& drop_reason,
                                    std::chrono::milliseconds connection_duration);
    
    /**
     * Resolve an issue manually
     * @param issue_id Issue ID
     * @param resolution_strategy Resolution strategy
     * @param resolution_description Resolution description
     * @return true if resolution was successful
     */
    bool resolveIssue(const std::string& issue_id,
                     ResolutionStrategy resolution_strategy,
                     const std::string& resolution_description);
    
    /**
     * Get issue information
     * @param issue_id Issue ID
     * @return Issue information or nullptr if not found
     */
    std::shared_ptr<IssueRecord> getIssue(const std::string& issue_id);
    
    /**
     * Get active issues
     * @return Vector of active issue information
     */
    std::vector<std::shared_ptr<IssueRecord>> getActiveIssues();
    
    /**
     * Get issues by type
     * @param issue_type Issue type
     * @return Vector of issue information
     */
    std::vector<std::shared_ptr<IssueRecord>> getIssuesByType(IssueType issue_type);
    
    /**
     * Get issues by peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of issue information
     */
    std::vector<std::shared_ptr<IssueRecord>> getIssuesByPeer(const std::string& peer_ip, int peer_port);
    
    /**
     * Get issue history for a peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Vector of historical issue information
     */
    std::vector<IssueRecord> getIssueHistory(const std::string& peer_ip, int peer_port);
    
    /**
     * Get resolution statistics
     * @return Current resolution statistics
     */
    ResolutionStatistics getStatistics();
    
    /**
     * Reset resolution statistics
     */
    void resetStatistics();
    
    /**
     * Update issue configuration
     * @param config New issue configuration
     */
    void updateConfig(const IssueConfig& config);
    
    /**
     * Enable or disable automatic resolution
     * @param enable true to enable automatic resolution
     */
    void setAutomaticResolutionEnabled(bool enable);
    
    /**
     * Check if automatic resolution is enabled
     * @return true if automatic resolution is enabled
     */
    bool isAutomaticResolutionEnabled();
    
    /**
     * Enable or disable peer blacklisting
     * @param enable true to enable peer blacklisting
     */
    void setPeerBlacklistingEnabled(bool enable);
    
    /**
     * Check if peer blacklisting is enabled
     * @return true if peer blacklisting is enabled
     */
    bool isPeerBlacklistingEnabled();
    
    /**
     * Get blacklisted peers
     * @return Map of blacklisted peers and reasons
     */
    std::map<std::string, std::set<std::string>> getBlacklistedPeers();
    
    /**
     * Check if peer is blacklisted
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return true if peer is blacklisted
     */
    bool isPeerBlacklisted(const std::string& peer_ip, int peer_port);
    
    /**
     * Remove peer from blacklist
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return true if peer was removed from blacklist
     */
    bool removePeerFromBlacklist(const std::string& peer_ip, int peer_port);
    
    /**
     * Get issue patterns
     * @return Map of issue patterns and their frequencies
     */
    std::map<std::string, std::vector<IssueRecord>> getIssuePatterns();
    
    /**
     * Get issue pattern for a specific issue
     * @param issue_id Issue ID
     * @return Issue pattern or empty string if not found
     */
    std::string getIssuePattern(const std::string& issue_id);
    
    /**
     * Get resolution success rate
     * @return Resolution success rate (0.0 to 1.0)
     */
    double getResolutionSuccessRate();
    
    /**
     * Get resolution success rate by issue type
     * @param issue_type Issue type
     * @return Resolution success rate (0.0 to 1.0)
     */
    double getResolutionSuccessRate(IssueType issue_type);
    
    /**
     * Get resolution success rate by strategy
     * @param strategy Resolution strategy
     * @return Resolution success rate (0.0 to 1.0)
     */
    double getResolutionSuccessRate(ResolutionStrategy strategy);
    
    /**
     * Get average resolution time
     * @return Average resolution time
     */
    std::chrono::milliseconds getAverageResolutionTime();
    
    /**
     * Get issue count by peer
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Issue count
     */
    int getIssueCountByPeer(const std::string& peer_ip, int peer_port);
    
    /**
     * Get issue count by type
     * @param issue_type Issue type
     * @return Issue count
     */
    int getIssueCountByType(IssueType issue_type);
    
    /**
     * Start issue resolver
     */
    void start();
    
    /**
     * Stop issue resolver
     */
    void stop();
    
    /**
     * Check if resolver is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Export issue data to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportIssueData(const std::string& filename);
    
    /**
     * Export statistics to file
     * @param filename Output filename
     * @return true if export was successful
     */
    bool exportStatistics(const std::string& filename);
    
    /**
     * Get resolver health status
     * @return Health status information
     */
    std::map<std::string, std::string> getHealthStatus();
    
    /**
     * Clear issue history
     */
    void clearIssueHistory();
    
    /**
     * Clear active issues
     */
    void clearActiveIssues();
    
    /**
     * Force resolution of all active issues
     */
    void forceResolution();
    
    /**
     * Get issue count
     * @return Total number of active issues
     */
    int getIssueCount();
    
    /**
     * Get resolved issue count
     * @return Number of resolved issues
     */
    int getResolvedIssueCount();
    
    /**
     * Get unresolved issue count
     * @return Number of unresolved issues
     */
    int getUnresolvedIssueCount();
    
    /**
     * Get issue recommendations
     * @return Vector of issue resolution recommendations
     */
    std::vector<std::string> getIssueRecommendations();
    
    /**
     * Get peer compatibility score
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Compatibility score (0.0 to 1.0)
     */
    double getPeerCompatibilityScore(const std::string& peer_ip, int peer_port);
    
    /**
     * Get issue trend analysis
     * @param time_period Time period for trend analysis
     * @return Trend analysis results
     */
    std::map<std::string, double> getIssueTrends(
        std::chrono::hours time_period = std::chrono::hours(24)
    );
};

/**
 * BEP 9 issue resolver factory for different resolution strategies
 */
class BEP9IssueResolverFactory {
public:
    enum class ResolutionStrategy {
        AGGRESSIVE,  // Aggressive resolution with immediate action
        MODERATE,    // Moderate resolution with balanced approach
        CONSERVATIVE, // Conservative resolution with careful approach
        CUSTOM       // Custom resolution configuration
    };
    
    static std::unique_ptr<BEP9IssueResolver> createResolver(
        ResolutionStrategy strategy,
        const BEP9IssueResolver::IssueConfig& config = BEP9IssueResolver::IssueConfig{}
    );
    
    static BEP9IssueResolver::IssueConfig getDefaultConfig(ResolutionStrategy strategy);
};

/**
 * RAII issue tracking wrapper for automatic issue management
 */
class IssueTrackingGuard {
private:
    std::shared_ptr<BEP9IssueResolver> resolver_;
    std::string issue_id_;
    bool resolved_;

public:
    IssueTrackingGuard(std::shared_ptr<BEP9IssueResolver> resolver,
                      const std::string& peer_ip,
                      int peer_port,
                      const std::string& peer_id,
                      BEP9IssueResolver::IssueType issue_type,
                      const std::string& issue_description);
    
    ~IssueTrackingGuard();
    
    /**
     * Resolve the issue
     * @param resolution_strategy Resolution strategy
     * @param resolution_description Resolution description
     */
    void resolve(BEP9IssueResolver::ResolutionStrategy resolution_strategy,
                const std::string& resolution_description);
    
    /**
     * Get issue ID
     * @return Issue ID
     */
    const std::string& getIssueId() const;
    
    /**
     * Get issue information
     * @return Issue information
     */
    std::shared_ptr<BEP9IssueResolver::IssueRecord> getIssueInfo();
    
    /**
     * Check if issue is resolved
     * @return true if resolved
     */
    bool isResolved();
};

/**
 * BEP 9 issue analyzer for pattern detection and recommendations
 */
class BEP9IssueAnalyzer {
private:
    std::shared_ptr<BEP9IssueResolver> resolver_;
    
public:
    BEP9IssueAnalyzer(std::shared_ptr<BEP9IssueResolver> resolver);
    
    /**
     * Analyze issue patterns
     * @return Map of patterns and their frequencies
     */
    std::map<std::string, int> analyzeIssuePatterns();
    
    /**
     * Detect issue clusters
     * @param time_window Time window for clustering
     * @return Vector of issue clusters
     */
    std::vector<std::vector<std::string>> detectIssueClusters(
        std::chrono::minutes time_window = std::chrono::minutes(10)
    );
    
    /**
     * Predict potential issues
     * @return Vector of predicted issue patterns
     */
    std::vector<std::string> predictPotentialIssues();
    
    /**
     * Get issue trend analysis
     * @param time_period Time period for trend analysis
     * @return Trend analysis results
     */
    std::map<std::string, double> getIssueTrends(
        std::chrono::hours time_period = std::chrono::hours(24)
    );
    
    /**
     * Get peer reliability score
     * @param peer_ip Peer IP address
     * @param peer_port Peer port
     * @return Reliability score (0.0 to 1.0)
     */
    double getPeerReliabilityScore(const std::string& peer_ip, int peer_port);
    
    /**
     * Get issue resolution recommendations
     * @return Vector of recommendations
     */
    std::vector<std::string> getResolutionRecommendations();
    
    /**
     * Get extension protocol health score
     * @return Health score (0.0 to 1.0)
     */
    double getExtensionProtocolHealthScore();
};

} // namespace dht_crawler
