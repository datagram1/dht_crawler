#include "bep9_issue_resolver.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace dht_crawler {

BEP9IssueResolver::BEP9IssueResolver(const ResolverConfig& config) : config_(config) {
}

BEP9IssueResolver::IssueDiagnosis BEP9IssueResolver::diagnoseFailure(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    IssueDiagnosis diagnosis;
    diagnosis.peer_ip = peer_ip;
    diagnosis.peer_port = peer_port;
    diagnosis.info_hash = info_hash;
    diagnosis.timestamp = std::chrono::steady_clock::now();
    diagnosis.issue_type = IssueType::UNKNOWN;
    diagnosis.severity = IssueSeverity::LOW;
    diagnosis.description = "Unknown issue";
    diagnosis.recommendations = {};
    
    // Analyze recent failures for this peer
    std::vector<FailureRecord> recent_failures = getRecentFailures(peer_ip, peer_port, info_hash);
    
    if (recent_failures.empty()) {
        diagnosis.issue_type = IssueType::NO_DATA;
        diagnosis.description = "No failure data available";
        diagnosis.recommendations.push_back("Collect more failure data");
        return diagnosis;
    }
    
    // Analyze failure patterns
    FailurePattern pattern = analyzeFailurePattern(recent_failures);
    
    // Determine issue type based on pattern
    if (pattern.timeout_count > pattern.connection_count && pattern.timeout_count > pattern.protocol_count) {
        diagnosis.issue_type = IssueType::TIMEOUT;
        diagnosis.severity = IssueSeverity::HIGH;
        diagnosis.description = "Frequent timeout issues";
        diagnosis.recommendations.push_back("Increase timeout values");
        diagnosis.recommendations.push_back("Check network connectivity");
    } else if (pattern.connection_count > pattern.timeout_count && pattern.connection_count > pattern.protocol_count) {
        diagnosis.issue_type = IssueType::CONNECTION;
        diagnosis.severity = IssueSeverity::HIGH;
        diagnosis.description = "Connection issues";
        diagnosis.recommendations.push_back("Check peer availability");
        diagnosis.recommendations.push_back("Verify network configuration");
    } else if (pattern.protocol_count > pattern.timeout_count && pattern.protocol_count > pattern.connection_count) {
        diagnosis.issue_type = IssueType::PROTOCOL;
        diagnosis.severity = IssueSeverity::MEDIUM;
        diagnosis.description = "Protocol compatibility issues";
        diagnosis.recommendations.push_back("Check protocol version compatibility");
        diagnosis.recommendations.push_back("Verify message format");
    } else {
        diagnosis.issue_type = IssueType::MIXED;
        diagnosis.severity = IssueSeverity::MEDIUM;
        diagnosis.description = "Mixed failure types";
        diagnosis.recommendations.push_back("Investigate multiple failure causes");
    }
    
    // Store diagnosis
    issue_diagnoses_.push_back(diagnosis);
    
    // Limit number of diagnoses
    if (issue_diagnoses_.size() > config_.max_diagnoses) {
        issue_diagnoses_.erase(issue_diagnoses_.begin());
    }
    
    return diagnosis;
}

BEP9IssueResolver::MessageFlowAnalysis BEP9IssueResolver::analyzeMessageFlow(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    MessageFlowAnalysis analysis;
    analysis.peer_ip = peer_ip;
    analysis.peer_port = peer_port;
    analysis.info_hash = info_hash;
    analysis.timestamp = std::chrono::steady_clock::now();
    analysis.total_messages = 0;
    analysis.successful_messages = 0;
    analysis.failed_messages = 0;
    analysis.timeout_messages = 0;
    analysis.message_types = {};
    analysis.flow_efficiency = 0.0;
    analysis.bottlenecks = {};
    
    // Analyze message flow for this peer
    std::vector<MessageRecord> messages = getMessagesForPeer(peer_ip, peer_port, info_hash);
    
    if (messages.empty()) {
        analysis.flow_efficiency = 0.0;
        analysis.bottlenecks.push_back("No message data available");
        return analysis;
    }
    
    analysis.total_messages = static_cast<int>(messages.size());
    
    for (const auto& message : messages) {
        switch (message.status) {
            case MessageStatus::SUCCESS:
                analysis.successful_messages++;
                break;
            case MessageStatus::FAILED:
                analysis.failed_messages++;
                break;
            case MessageStatus::TIMEOUT:
                analysis.timeout_messages++;
                break;
        }
        
        analysis.message_types[message.message_type]++;
    }
    
    // Calculate flow efficiency
    if (analysis.total_messages > 0) {
        analysis.flow_efficiency = static_cast<double>(analysis.successful_messages) / analysis.total_messages;
    }
    
    // Identify bottlenecks
    if (analysis.timeout_messages > analysis.total_messages * 0.5) {
        analysis.bottlenecks.push_back("High timeout rate");
    }
    
    if (analysis.failed_messages > analysis.total_messages * 0.3) {
        analysis.bottlenecks.push_back("High failure rate");
    }
    
    if (analysis.message_types.find("handshake") != analysis.message_types.end() && 
        analysis.message_types["handshake"] > analysis.total_messages * 0.4) {
        analysis.bottlenecks.push_back("Excessive handshake attempts");
    }
    
    // Store analysis
    message_flow_analyses_.push_back(analysis);
    
    // Limit number of analyses
    if (message_flow_analyses_.size() > config_.max_analyses) {
        message_flow_analyses_.erase(message_flow_analyses_.begin());
    }
    
    return analysis;
}

BEP9IssueResolver::FailurePattern BEP9IssueResolver::detectFailurePattern(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    FailurePattern pattern;
    pattern.peer_ip = peer_ip;
    pattern.peer_port = peer_port;
    pattern.info_hash = info_hash;
    pattern.timestamp = std::chrono::steady_clock::now();
    pattern.timeout_count = 0;
    pattern.connection_count = 0;
    pattern.protocol_count = 0;
    pattern.handshake_count = 0;
    pattern.capability_count = 0;
    pattern.message_count = 0;
    pattern.total_failures = 0;
    pattern.pattern_type = PatternType::UNKNOWN;
    
    // Get recent failures for this peer
    std::vector<FailureRecord> failures = getRecentFailures(peer_ip, peer_port, info_hash);
    
    if (failures.empty()) {
        pattern.pattern_type = PatternType::NO_DATA;
        return pattern;
    }
    
    pattern.total_failures = static_cast<int>(failures.size());
    
    for (const auto& failure : failures) {
        switch (failure.failure_type) {
            case FailureType::TIMEOUT:
                pattern.timeout_count++;
                break;
            case FailureType::CONNECTION:
                pattern.connection_count++;
                break;
            case FailureType::PROTOCOL:
                pattern.protocol_count++;
                break;
            case FailureType::HANDSHAKE:
                pattern.handshake_count++;
                break;
            case FailureType::CAPABILITY:
                pattern.capability_count++;
                break;
            case FailureType::MESSAGE:
                pattern.message_count++;
                break;
        }
    }
    
    // Determine pattern type
    if (pattern.timeout_count > pattern.total_failures * 0.6) {
        pattern.pattern_type = PatternType::TIMEOUT_DOMINANT;
    } else if (pattern.connection_count > pattern.total_failures * 0.6) {
        pattern.pattern_type = PatternType::CONNECTION_DOMINANT;
    } else if (pattern.protocol_count > pattern.total_failures * 0.6) {
        pattern.pattern_type = PatternType::PROTOCOL_DOMINANT;
    } else if (pattern.handshake_count > pattern.total_failures * 0.6) {
        pattern.pattern_type = PatternType::HANDSHAKE_DOMINANT;
    } else if (pattern.capability_count > pattern.total_failures * 0.6) {
        pattern.pattern_type = PatternType::CAPABILITY_DOMINANT;
    } else if (pattern.message_count > pattern.total_failures * 0.6) {
        pattern.pattern_type = PatternType::MESSAGE_DOMINANT;
    } else {
        pattern.pattern_type = PatternType::MIXED;
    }
    
    // Store pattern
    failure_patterns_.push_back(pattern);
    
    // Limit number of patterns
    if (failure_patterns_.size() > config_.max_patterns) {
        failure_patterns_.erase(failure_patterns_.begin());
    }
    
    return pattern;
}

void BEP9IssueResolver::logDisconnectionReason(const std::string& peer_ip, int peer_port, 
                                               const std::string& info_hash, 
                                               const std::string& reason) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    DisconnectionRecord record;
    record.peer_ip = peer_ip;
    record.peer_port = peer_port;
    record.info_hash = info_hash;
    record.reason = reason;
    record.timestamp = std::chrono::steady_clock::now();
    
    disconnection_records_.push_back(record);
    
    // Limit number of records
    if (disconnection_records_.size() > config_.max_disconnections) {
        disconnection_records_.erase(disconnection_records_.begin());
    }
}

std::vector<BEP9IssueResolver::IssueDiagnosis> BEP9IssueResolver::getIssueDiagnoses() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return issue_diagnoses_;
}

std::vector<BEP9IssueResolver::MessageFlowAnalysis> BEP9IssueResolver::getMessageFlowAnalyses() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return message_flow_analyses_;
}

std::vector<BEP9IssueResolver::FailurePattern> BEP9IssueResolver::getFailurePatterns() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return failure_patterns_;
}

std::vector<BEP9IssueResolver::DisconnectionRecord> BEP9IssueResolver::getDisconnectionRecords() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return disconnection_records_;
}

std::vector<BEP9IssueResolver::IssueDiagnosis> BEP9IssueResolver::getIssueDiagnosesByPeer(const std::string& peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    std::vector<IssueDiagnosis> diagnoses;
    for (const auto& diagnosis : issue_diagnoses_) {
        if (diagnosis.peer_ip == peer_ip && diagnosis.peer_port == peer_port) {
            diagnoses.push_back(diagnosis);
        }
    }
    
    return diagnoses;
}

std::vector<BEP9IssueResolver::MessageFlowAnalysis> BEP9IssueResolver::getMessageFlowAnalysesByPeer(const std::string& peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    std::vector<MessageFlowAnalysis> analyses;
    for (const auto& analysis : message_flow_analyses_) {
        if (analysis.peer_ip == peer_ip && analysis.peer_port == peer_port) {
            analyses.push_back(analysis);
        }
    }
    
    return analyses;
}

std::vector<BEP9IssueResolver::FailurePattern> BEP9IssueResolver::getFailurePatternsByPeer(const std::string& peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    std::vector<FailurePattern> patterns;
    for (const auto& pattern : failure_patterns_) {
        if (pattern.peer_ip == peer_ip && pattern.peer_port == peer_port) {
            patterns.push_back(pattern);
        }
    }
    
    return patterns;
}

std::vector<BEP9IssueResolver::DisconnectionRecord> BEP9IssueResolver::getDisconnectionRecordsByPeer(const std::string& peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    std::vector<DisconnectionRecord> records;
    for (const auto& record : disconnection_records_) {
        if (record.peer_ip == peer_ip && record.peer_port == peer_port) {
            records.push_back(record);
        }
    }
    
    return records;
}

int BEP9IssueResolver::getIssueDiagnosisCount() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return static_cast<int>(issue_diagnoses_.size());
}

int BEP9IssueResolver::getMessageFlowAnalysisCount() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return static_cast<int>(message_flow_analyses_.size());
}

int BEP9IssueResolver::getFailurePatternCount() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return static_cast<int>(failure_patterns_.size());
}

int BEP9IssueResolver::getDisconnectionRecordCount() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    return static_cast<int>(disconnection_records_.size());
}

double BEP9IssueResolver::getAverageFlowEfficiency() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    if (message_flow_analyses_.empty()) {
        return 0.0;
    }
    
    double total_efficiency = 0.0;
    for (const auto& analysis : message_flow_analyses_) {
        total_efficiency += analysis.flow_efficiency;
    }
    
    return total_efficiency / message_flow_analyses_.size();
}

std::map<BEP9IssueResolver::IssueType, int> BEP9IssueResolver::getIssueTypeCounts() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    std::map<IssueType, int> counts;
    for (const auto& diagnosis : issue_diagnoses_) {
        counts[diagnosis.issue_type]++;
    }
    
    return counts;
}

std::map<BEP9IssueResolver::IssueSeverity, int> BEP9IssueResolver::getIssueSeverityCounts() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    std::map<IssueSeverity, int> counts;
    for (const auto& diagnosis : issue_diagnoses_) {
        counts[diagnosis.severity]++;
    }
    
    return counts;
}

std::map<BEP9IssueResolver::PatternType, int> BEP9IssueResolver::getPatternTypeCounts() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    std::map<PatternType, int> counts;
    for (const auto& pattern : failure_patterns_) {
        counts[pattern.pattern_type]++;
    }
    
    return counts;
}

void BEP9IssueResolver::clearExpiredData() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Clear expired issue diagnoses
    for (auto it = issue_diagnoses_.begin(); it != issue_diagnoses_.end();) {
        if (now - it->timestamp > std::chrono::milliseconds(config_.data_retention)) {
            it = issue_diagnoses_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clear expired message flow analyses
    for (auto it = message_flow_analyses_.begin(); it != message_flow_analyses_.end();) {
        if (now - it->timestamp > std::chrono::milliseconds(config_.data_retention)) {
            it = message_flow_analyses_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clear expired failure patterns
    for (auto it = failure_patterns_.begin(); it != failure_patterns_.end();) {
        if (now - it->timestamp > std::chrono::milliseconds(config_.data_retention)) {
            it = failure_patterns_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clear expired disconnection records
    for (auto it = disconnection_records_.begin(); it != disconnection_records_.end();) {
        if (now - it->timestamp > std::chrono::milliseconds(config_.data_retention)) {
            it = disconnection_records_.erase(it);
        } else {
            ++it;
        }
    }
}

void BEP9IssueResolver::clearAllData() {
    std::lock_guard<std::mutex> lock(resolver_mutex_);
    
    issue_diagnoses_.clear();
    message_flow_analyses_.clear();
    failure_patterns_.clear();
    disconnection_records_.clear();
}

void BEP9IssueResolver::updateConfig(const ResolverConfig& config) {
    config_ = config;
}

BEP9IssueResolver::ResolverConfig BEP9IssueResolver::getConfig() const {
    return config_;
}

std::map<std::string, std::string> BEP9IssueResolver::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["issue_diagnoses"] = std::to_string(getIssueDiagnosisCount());
    status["message_flow_analyses"] = std::to_string(getMessageFlowAnalysisCount());
    status["failure_patterns"] = std::to_string(getFailurePatternCount());
    status["disconnection_records"] = std::to_string(getDisconnectionRecordCount());
    status["average_flow_efficiency"] = std::to_string(getAverageFlowEfficiency());
    status["data_retention"] = std::to_string(config_.data_retention);
    status["max_diagnoses"] = std::to_string(config_.max_diagnoses);
    status["max_analyses"] = std::to_string(config_.max_analyses);
    status["max_patterns"] = std::to_string(config_.max_patterns);
    status["max_disconnections"] = std::to_string(config_.max_disconnections);
    
    return status;
}

std::vector<BEP9IssueResolver::FailureRecord> BEP9IssueResolver::getRecentFailures(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    // This would typically query a failure database or cache
    // For now, return empty vector
    return {};
}

BEP9IssueResolver::FailurePattern BEP9IssueResolver::analyzeFailurePattern(const std::vector<FailureRecord>& failures) {
    FailurePattern pattern;
    pattern.timestamp = std::chrono::steady_clock::now();
    pattern.timeout_count = 0;
    pattern.connection_count = 0;
    pattern.protocol_count = 0;
    pattern.handshake_count = 0;
    pattern.capability_count = 0;
    pattern.message_count = 0;
    pattern.total_failures = static_cast<int>(failures.size());
    pattern.pattern_type = PatternType::UNKNOWN;
    
    for (const auto& failure : failures) {
        switch (failure.failure_type) {
            case FailureType::TIMEOUT:
                pattern.timeout_count++;
                break;
            case FailureType::CONNECTION:
                pattern.connection_count++;
                break;
            case FailureType::PROTOCOL:
                pattern.protocol_count++;
                break;
            case FailureType::HANDSHAKE:
                pattern.handshake_count++;
                break;
            case FailureType::CAPABILITY:
                pattern.capability_count++;
                break;
            case FailureType::MESSAGE:
                pattern.message_count++;
                break;
        }
    }
    
    return pattern;
}

std::vector<BEP9IssueResolver::MessageRecord> BEP9IssueResolver::getMessagesForPeer(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    // This would typically query a message database or cache
    // For now, return empty vector
    return {};
}

} // namespace dht_crawler
