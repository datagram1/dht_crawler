#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <thread>
#include <exception>

namespace dht_crawler {

/**
 * Centralized error management system
 * Provides error categorization, severity levels, context tracking, and recovery strategies
 */
class ErrorHandler {
public:
    enum class ErrorCategory {
        CONNECTION,        // Network connection errors
        PROTOCOL,          // Protocol-related errors
        VALIDATION,        // Data validation errors
        TIMEOUT,           // Timeout-related errors
        DATABASE,          // Database operation errors
        DHT,               // DHT operation errors
        METADATA,          // Metadata extraction errors
        PEER,              // Peer communication errors
        TRACKER,           // Tracker communication errors
        BOOTSTRAP,         // Bootstrap operation errors
        SYSTEM,            // System-level errors
        UNKNOWN            // Unknown error category
    };

    enum class ErrorSeverity {
        DEBUG,             // Debug information
        INFO,              // Informational messages
        WARNING,           // Warning messages
        ERROR,             // Error messages
        CRITICAL,          // Critical errors
        FATAL              // Fatal errors
    };

    enum class RecoveryStrategy {
        RETRY,             // Retry the operation
        FALLBACK,          // Use fallback mechanism
        SKIP,              // Skip the operation
        ABORT,             // Abort the operation
        ESCALATE,          // Escalate to higher level
        CUSTOM             // Custom recovery strategy
    };

    struct ErrorContext {
        std::string operation;
        std::string component;
        std::string request_id;
        std::string peer_ip;
        int peer_port;
        std::string infohash;
        std::map<std::string, std::string> metadata;
        std::chrono::steady_clock::time_point timestamp;
        std::string stack_trace;
        int error_code;
        std::string error_message;
    };

    struct ErrorRecord {
        std::string error_id;
        ErrorCategory category;
        ErrorSeverity severity;
        std::string message;
        ErrorContext context;
        RecoveryStrategy recovery_strategy;
        std::function<void()> recovery_callback;
        bool is_recovered = false;
        int retry_count = 0;
        std::chrono::steady_clock::time_point first_occurrence;
        std::chrono::steady_clock::time_point last_occurrence;
        std::vector<std::string> related_errors;
    };

    struct ErrorStatistics {
        std::map<ErrorCategory, int> error_counts_by_category;
        std::map<ErrorSeverity, int> error_counts_by_severity;
        std::map<std::string, int> error_counts_by_message;
        std::map<RecoveryStrategy, int> recovery_counts;
        int total_errors = 0;
        int recovered_errors = 0;
        int unrecovered_errors = 0;
        std::chrono::milliseconds avg_recovery_time;
        std::map<std::string, int> error_patterns;
    };

private:
    std::map<std::string, std::shared_ptr<ErrorRecord>> error_records_;
    std::mutex error_records_mutex_;
    
    std::queue<std::shared_ptr<ErrorRecord>> error_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    
    std::thread error_processing_thread_;
    std::atomic<bool> should_stop_{false};
    
    ErrorStatistics stats_;
    std::mutex stats_mutex_;
    
    // Error correlation
    std::map<std::string, std::vector<std::string>> error_correlations_;
    std::mutex correlation_mutex_;
    
    // Recovery strategies
    std::map<ErrorCategory, RecoveryStrategy> default_recovery_strategies_;
    std::map<std::string, std::function<void()>> custom_recovery_callbacks_;
    
    // Internal methods
    void errorProcessingLoop();
    void processError(std::shared_ptr<ErrorRecord> error_record);
    void updateStatistics(const ErrorRecord& error_record);
    void correlateErrors(std::shared_ptr<ErrorRecord> error_record);
    std::string generateErrorId();
    std::string getStackTrace();
    RecoveryStrategy determineRecoveryStrategy(ErrorCategory category, 
                                            ErrorSeverity severity,
                                            const std::string& message);

public:
    ErrorHandler();
    ~ErrorHandler();
    
    /**
     * Log an error with automatic categorization and recovery
     * @param category Error category
     * @param severity Error severity
     * @param message Error message
     * @param context Error context
     * @param recovery_callback Custom recovery callback (optional)
     * @return Error ID for tracking
     */
    std::string logError(ErrorCategory category,
                        ErrorSeverity severity,
                        const std::string& message,
                        const ErrorContext& context,
                        std::function<void()> recovery_callback = nullptr);
    
    /**
     * Log an error with automatic context generation
     * @param category Error category
     * @param severity Error severity
     * @param message Error message
     * @param operation Operation that failed
     * @param component Component that failed
     * @return Error ID for tracking
     */
    std::string logError(ErrorCategory category,
                        ErrorSeverity severity,
                        const std::string& message,
                        const std::string& operation,
                        const std::string& component);
    
    /**
     * Log an exception with automatic context extraction
     * @param exception Exception to log
     * @param context Error context
     * @return Error ID for tracking
     */
    std::string logException(const std::exception& exception,
                            const ErrorContext& context);
    
    /**
     * Attempt to recover from an error
     * @param error_id Error ID to recover from
     * @return true if recovery was attempted
     */
    bool attemptRecovery(const std::string& error_id);
    
    /**
     * Mark an error as recovered
     * @param error_id Error ID to mark as recovered
     * @return true if error was marked as recovered
     */
    bool markAsRecovered(const std::string& error_id);
    
    /**
     * Get error record by ID
     * @param error_id Error ID
     * @return Error record or nullptr if not found
     */
    std::shared_ptr<ErrorRecord> getErrorRecord(const std::string& error_id);
    
    /**
     * Get errors by category
     * @param category Error category
     * @return Vector of error records
     */
    std::vector<std::shared_ptr<ErrorRecord>> getErrorsByCategory(ErrorCategory category);
    
    /**
     * Get errors by severity
     * @param severity Error severity
     * @return Vector of error records
     */
    std::vector<std::shared_ptr<ErrorRecord>> getErrorsBySeverity(ErrorSeverity severity);
    
    /**
     * Get recent errors
     * @param count Number of recent errors to return
     * @return Vector of recent error records
     */
    std::vector<std::shared_ptr<ErrorRecord>> getRecentErrors(int count = 10);
    
    /**
     * Get error statistics
     * @return Current error statistics
     */
    ErrorStatistics getStatistics();
    
    /**
     * Reset error statistics
     */
    void resetStatistics();
    
    /**
     * Set default recovery strategy for a category
     * @param category Error category
     * @param strategy Recovery strategy
     */
    void setDefaultRecoveryStrategy(ErrorCategory category, RecoveryStrategy strategy);
    
    /**
     * Set custom recovery callback
     * @param error_pattern Error pattern to match
     * @param callback Recovery callback
     */
    void setCustomRecoveryCallback(const std::string& error_pattern,
                                  std::function<void()> callback);
    
    /**
     * Get error correlations
     * @param error_id Error ID
     * @return Vector of related error IDs
     */
    std::vector<std::string> getErrorCorrelations(const std::string& error_id);
    
    /**
     * Clear old error records
     * @param older_than Time threshold for clearing
     */
    void clearOldErrors(std::chrono::hours older_than = std::chrono::hours(24));
    
    /**
     * Export error records to file
     * @param filename Output filename
     * @param format Export format (JSON, CSV, etc.)
     * @return true if export was successful
     */
    bool exportErrors(const std::string& filename, const std::string& format = "JSON");
    
    /**
     * Start error processing
     */
    void start();
    
    /**
     * Stop error processing
     */
    void stop();
    
    /**
     * Check if error handler is running
     * @return true if running
     */
    bool isRunning();
    
    /**
     * Get error count by category
     * @param category Error category
     * @return Error count
     */
    int getErrorCount(ErrorCategory category);
    
    /**
     * Get error count by severity
     * @param severity Error severity
     * @return Error count
     */
    int getErrorCount(ErrorSeverity severity);
    
    /**
     * Check if there are critical errors
     * @return true if there are critical errors
     */
    bool hasCriticalErrors();
    
    /**
     * Get critical error count
     * @return Number of critical errors
     */
    int getCriticalErrorCount();
    
    /**
     * Get error rate (errors per minute)
     * @return Error rate
     */
    double getErrorRate();
    
    /**
     * Get recovery success rate
     * @return Recovery success rate (0.0 to 1.0)
     */
    double getRecoverySuccessRate();
};

/**
 * Error handler factory for different error handling strategies
 */
class ErrorHandlerFactory {
public:
    enum class ErrorHandlingStrategy {
        STRICT,      // Strict error handling with immediate recovery
        MODERATE,    // Moderate error handling with delayed recovery
        LENIENT,     // Lenient error handling with minimal recovery
        CUSTOM       // Custom error handling configuration
    };
    
    static std::unique_ptr<ErrorHandler> createErrorHandler(
        ErrorHandlingStrategy strategy
    );
    
    static std::map<ErrorHandler::ErrorCategory, ErrorHandler::RecoveryStrategy> 
    getDefaultRecoveryStrategies(ErrorHandlingStrategy strategy);
};

/**
 * RAII error context wrapper for automatic error tracking
 */
class ErrorContextGuard {
private:
    std::shared_ptr<ErrorHandler> error_handler_;
    ErrorHandler::ErrorContext context_;
    std::string operation_;
    std::string component_;

public:
    ErrorContextGuard(std::shared_ptr<ErrorHandler> error_handler,
                     const std::string& operation,
                     const std::string& component,
                     const std::map<std::string, std::string>& metadata = {});
    
    ~ErrorContextGuard();
    
    /**
     * Log an error in this context
     * @param category Error category
     * @param severity Error severity
     * @param message Error message
     * @return Error ID
     */
    std::string logError(ErrorHandler::ErrorCategory category,
                        ErrorHandler::ErrorSeverity severity,
                        const std::string& message);
    
    /**
     * Update context metadata
     * @param key Metadata key
     * @param value Metadata value
     */
    void updateMetadata(const std::string& key, const std::string& value);
    
    /**
     * Get current context
     * @return Error context
     */
    const ErrorHandler::ErrorContext& getContext() const;
};

/**
 * Error correlation analyzer for pattern detection
 */
class ErrorCorrelationAnalyzer {
private:
    std::shared_ptr<ErrorHandler> error_handler_;
    
public:
    ErrorCorrelationAnalyzer(std::shared_ptr<ErrorHandler> error_handler);
    
    /**
     * Analyze error patterns
     * @return Map of error patterns and their frequencies
     */
    std::map<std::string, int> analyzeErrorPatterns();
    
    /**
     * Detect error clusters
     * @param time_window Time window for clustering
     * @return Vector of error clusters
     */
    std::vector<std::vector<std::string>> detectErrorClusters(
        std::chrono::minutes time_window = std::chrono::minutes(5)
    );
    
    /**
     * Predict potential errors based on patterns
     * @return Vector of predicted error patterns
     */
    std::vector<std::string> predictPotentialErrors();
    
    /**
     * Get error trend analysis
     * @param time_period Time period for trend analysis
     * @return Trend analysis results
     */
    std::map<std::string, double> getErrorTrends(
        std::chrono::hours time_period = std::chrono::hours(24)
    );
};

} // namespace dht_crawler
