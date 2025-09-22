#include "error_handler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace dht_crawler {

ErrorHandler::ErrorHandler(const ErrorConfig& config) : config_(config) {
    if (config_.log_to_file) {
        log_file_.open(config_.log_filename, std::ios::app);
    }
}

ErrorHandler::~ErrorHandler() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void ErrorHandler::logError(const std::string& error_message, 
                          ErrorSeverity severity,
                          const std::string& component,
                          const std::map<std::string, std::string>& context) {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    
    ErrorInfo error_info;
    error_info.id = generateErrorId();
    error_info.message = error_message;
    error_info.severity = severity;
    error_info.component = component;
    error_info.context = context;
    error_info.timestamp = std::chrono::steady_clock::now();
    error_info.count = 1;
    
    // Check if this is a duplicate error
    auto it = error_counts_.find(error_message);
    if (it != error_counts_.end()) {
        it->second++;
        error_info.count = it->second;
    } else {
        error_counts_[error_message] = 1;
    }
    
    errors_.push_back(error_info);
    
    // Log to console if enabled
    if (config_.log_to_console) {
        logToConsole(error_info);
    }
    
    // Log to file if enabled
    if (config_.log_to_file && log_file_.is_open()) {
        logToFile(error_info);
    }
    
    // Update statistics
    updateStatistics(error_info);
}

std::string ErrorHandler::handleError(const std::string& error_message,
                                     ErrorSeverity severity,
                                     const std::string& component,
                                     const std::map<std::string, std::string>& context) {
    logError(error_message, severity, component, context);
    
    // Determine recovery strategy based on severity
    std::string recovery_strategy = determineRecoveryStrategy(severity, component);
    
    // Execute recovery strategy
    executeRecoveryStrategy(recovery_strategy, error_message, component);
    
    return recovery_strategy;
}

std::string ErrorHandler::generateErrorId() {
    static std::atomic<int> counter(0);
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "ERR_" << timestamp << "_" << counter.fetch_add(1);
    return ss.str();
}

void ErrorHandler::logToConsole(const ErrorInfo& error_info) {
    std::string severity_str = severityToString(error_info.severity);
    std::string timestamp_str = formatTimestamp(error_info.timestamp);
    
    std::cout << "[" << timestamp_str << "] [" << severity_str << "] [" 
              << error_info.component << "] " << error_info.message;
    
    if (error_info.count > 1) {
        std::cout << " (count: " << error_info.count << ")";
    }
    
    std::cout << std::endl;
    
    // Print context if available
    if (!error_info.context.empty()) {
        std::cout << "  Context: ";
        for (const auto& pair : error_info.context) {
            std::cout << pair.first << "=" << pair.second << " ";
        }
        std::cout << std::endl;
    }
}

void ErrorHandler::logToFile(const ErrorInfo& error_info) {
    std::string severity_str = severityToString(error_info.severity);
    std::string timestamp_str = formatTimestamp(error_info.timestamp);
    
    log_file_ << "[" << timestamp_str << "] [" << severity_str << "] [" 
              << error_info.component << "] " << error_info.message;
    
    if (error_info.count > 1) {
        log_file_ << " (count: " << error_info.count << ")";
    }
    
    log_file_ << std::endl;
    
    // Write context if available
    if (!error_info.context.empty()) {
        log_file_ << "  Context: ";
        for (const auto& pair : error_info.context) {
            log_file_ << pair.first << "=" << pair.second << " ";
        }
        log_file_ << std::endl;
    }
    
    log_file_.flush();
}

std::string ErrorHandler::severityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::INFO: return "INFO";
        case ErrorSeverity::WARNING: return "WARNING";
        case ErrorSeverity::ERROR: return "ERROR";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string ErrorHandler::formatTimestamp(const std::chrono::steady_clock::time_point& timestamp) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

void ErrorHandler::updateStatistics(const ErrorInfo& error_info) {
    stats_.total_errors++;
    
    switch (error_info.severity) {
        case ErrorSeverity::INFO:
            stats_.info_errors++;
            break;
        case ErrorSeverity::WARNING:
            stats_.warning_errors++;
            break;
        case ErrorSeverity::ERROR:
            stats_.error_errors++;
            break;
        case ErrorSeverity::CRITICAL:
            stats_.critical_errors++;
            break;
    }
    
    stats_.errors_by_component[error_info.component]++;
    stats_.errors_by_severity[error_info.severity]++;
    
    stats_.last_error_time = error_info.timestamp;
}

std::string ErrorHandler::determineRecoveryStrategy(ErrorSeverity severity, const std::string& component) {
    if (severity == ErrorSeverity::CRITICAL) {
        return "restart_component";
    } else if (severity == ErrorSeverity::ERROR) {
        return "retry_operation";
    } else if (severity == ErrorSeverity::WARNING) {
        return "log_and_continue";
    } else {
        return "ignore";
    }
}

void ErrorHandler::executeRecoveryStrategy(const std::string& strategy,
                                        const std::string& error_message,
                                        const std::string& component) {
    if (strategy == "restart_component") {
        // In a real implementation, this would restart the component
        logError("Component restart requested: " + component, ErrorSeverity::INFO, "ErrorHandler");
    } else if (strategy == "retry_operation") {
        // In a real implementation, this would retry the operation
        logError("Operation retry requested: " + error_message, ErrorSeverity::INFO, "ErrorHandler");
    } else if (strategy == "log_and_continue") {
        // Already logged, just continue
    }
    // For "ignore", do nothing
}

int ErrorHandler::getErrorCount() const {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    return stats_.total_errors;
}

int ErrorHandler::getWarningCount() const {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    return stats_.warning_errors;
}

int ErrorHandler::getCriticalCount() const {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    return stats_.critical_errors;
}

std::vector<ErrorHandler::ErrorInfo> ErrorHandler::getRecentErrors(int count) const {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    
    std::vector<ErrorInfo> recent_errors;
    int start_index = std::max(0, static_cast<int>(errors_.size()) - count);
    
    for (int i = start_index; i < static_cast<int>(errors_.size()); ++i) {
        recent_errors.push_back(errors_[i]);
    }
    
    return recent_errors;
}

std::map<std::string, int> ErrorHandler::getErrorsByComponent() const {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    return stats_.errors_by_component;
}

std::map<ErrorSeverity, int> ErrorHandler::getErrorsBySeverity() const {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    return stats_.errors_by_severity;
}

ErrorHandler::ErrorStatistics ErrorHandler::getStatistics() const {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    return stats_;
}

void ErrorHandler::clearErrors() {
    std::lock_guard<std::mutex> lock(errors_mutex_);
    errors_.clear();
    error_counts_.clear();
    stats_ = ErrorStatistics{};
}

void ErrorHandler::updateConfig(const ErrorConfig& config) {
    config_ = config;
    
    // Reopen log file if filename changed
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    if (config_.log_to_file) {
        log_file_.open(config_.log_filename, std::ios::app);
    }
}

ErrorHandler::ErrorConfig ErrorHandler::getConfig() const {
    return config_;
}

std::map<std::string, std::string> ErrorHandler::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["total_errors"] = std::to_string(getErrorCount());
    status["warning_errors"] = std::to_string(getWarningCount());
    status["critical_errors"] = std::to_string(getCriticalCount());
    status["log_to_console"] = config_.log_to_console ? "true" : "false";
    status["log_to_file"] = config_.log_to_file ? "true" : "false";
    
    return status;
}

} // namespace dht_crawler
