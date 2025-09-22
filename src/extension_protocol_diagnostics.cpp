#include "extension_protocol_diagnostics.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace dht_crawler {

ExtensionProtocolDiagnostics::ExtensionProtocolDiagnostics(const DiagnosticsConfig& config) : config_(config) {
}

void ExtensionProtocolDiagnostics::logHandshakeAttempt(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    HandshakeAttempt attempt;
    attempt.peer_ip = peer_ip;
    attempt.peer_port = peer_port;
    attempt.info_hash = info_hash;
    attempt.timestamp = std::chrono::steady_clock::now();
    attempt.status = HandshakeStatus::INITIATED;
    
    handshake_attempts_.push_back(attempt);
    
    // Limit number of attempts
    if (handshake_attempts_.size() > config_.max_handshake_attempts) {
        handshake_attempts_.erase(handshake_attempts_.begin());
    }
}

void ExtensionProtocolDiagnostics::logCapabilityNegotiation(const std::string& peer_ip, int peer_port, 
                                                           const std::string& info_hash, 
                                                           const std::vector<std::string>& capabilities) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    CapabilityNegotiation negotiation;
    negotiation.peer_ip = peer_ip;
    negotiation.peer_port = peer_port;
    negotiation.info_hash = info_hash;
    negotiation.capabilities = capabilities;
    negotiation.timestamp = std::chrono::steady_clock::now();
    negotiation.status = NegotiationStatus::IN_PROGRESS;
    
    capability_negotiations_.push_back(negotiation);
    
    // Limit number of negotiations
    if (capability_negotiations_.size() > config_.max_negotiations) {
        capability_negotiations_.erase(capability_negotiations_.begin());
    }
}

void ExtensionProtocolDiagnostics::logMessage(const std::string& peer_ip, int peer_port, 
                                            const std::string& info_hash, 
                                            const std::string& message_type, 
                                            const std::vector<uint8_t>& message_data) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    ProtocolMessage message;
    message.peer_ip = peer_ip;
    message.peer_port = peer_port;
    message.info_hash = info_hash;
    message.message_type = message_type;
    message.message_data = message_data;
    message.timestamp = std::chrono::steady_clock::now();
    message.status = MessageStatus::RECEIVED;
    
    protocol_messages_.push_back(message);
    
    // Limit number of messages
    if (protocol_messages_.size() > config_.max_messages) {
        protocol_messages_.erase(protocol_messages_.begin());
    }
}

ExtensionProtocolDiagnostics::ErrorType ExtensionProtocolDiagnostics::classifyError(const std::string& error_message) {
    if (error_message.find("timeout") != std::string::npos) {
        return ErrorType::TIMEOUT;
    } else if (error_message.find("connection") != std::string::npos) {
        return ErrorType::CONNECTION;
    } else if (error_message.find("protocol") != std::string::npos) {
        return ErrorType::PROTOCOL;
    } else if (error_message.find("handshake") != std::string::npos) {
        return ErrorType::HANDSHAKE;
    } else if (error_message.find("capability") != std::string::npos) {
        return ErrorType::CAPABILITY;
    } else if (error_message.find("message") != std::string::npos) {
        return ErrorType::MESSAGE;
    } else {
        return ErrorType::UNKNOWN;
    }
}

bool ExtensionProtocolDiagnostics::detectCompatibility(const std::string& peer_ip, int peer_port, const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    // Check recent handshake attempts
    for (const auto& attempt : handshake_attempts_) {
        if (attempt.peer_ip == peer_ip && attempt.peer_port == peer_port && 
            attempt.info_hash == info_hash && attempt.status == HandshakeStatus::SUCCESS) {
            return true;
        }
    }
    
    // Check recent capability negotiations
    for (const auto& negotiation : capability_negotiations_) {
        if (negotiation.peer_ip == peer_ip && negotiation.peer_port == peer_port && 
            negotiation.info_hash == info_hash && negotiation.status == NegotiationStatus::SUCCESS) {
            return true;
        }
    }
    
    return false;
}

std::vector<ExtensionProtocolDiagnostics::HandshakeAttempt> ExtensionProtocolDiagnostics::getHandshakeAttempts() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return handshake_attempts_;
}

std::vector<ExtensionProtocolDiagnostics::CapabilityNegotiation> ExtensionProtocolDiagnostics::getCapabilityNegotiations() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return capability_negotiations_;
}

std::vector<ExtensionProtocolDiagnostics::ProtocolMessage> ExtensionProtocolDiagnostics::getProtocolMessages() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return protocol_messages_;
}

std::vector<ExtensionProtocolDiagnostics::HandshakeAttempt> ExtensionProtocolDiagnostics::getHandshakeAttemptsByPeer(const std::string& peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::vector<HandshakeAttempt> attempts;
    for (const auto& attempt : handshake_attempts_) {
        if (attempt.peer_ip == peer_ip && attempt.peer_port == peer_port) {
            attempts.push_back(attempt);
        }
    }
    
    return attempts;
}

std::vector<ExtensionProtocolDiagnostics::CapabilityNegotiation> ExtensionProtocolDiagnostics::getCapabilityNegotiationsByPeer(const std::string& peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::vector<CapabilityNegotiation> negotiations;
    for (const auto& negotiation : capability_negotiations_) {
        if (negotiation.peer_ip == peer_ip && negotiation.peer_port == peer_port) {
            negotiations.push_back(negotiation);
        }
    }
    
    return negotiations;
}

std::vector<ExtensionProtocolDiagnostics::ProtocolMessage> ExtensionProtocolDiagnostics::getProtocolMessagesByPeer(const std::string& peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::vector<ProtocolMessage> messages;
    for (const auto& message : protocol_messages_) {
        if (message.peer_ip == peer_ip && message.peer_port == peer_port) {
            messages.push_back(message);
        }
    }
    
    return messages;
}

std::vector<ExtensionProtocolDiagnostics::HandshakeAttempt> ExtensionProtocolDiagnostics::getHandshakeAttemptsByInfoHash(const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::vector<HandshakeAttempt> attempts;
    for (const auto& attempt : handshake_attempts_) {
        if (attempt.info_hash == info_hash) {
            attempts.push_back(attempt);
        }
    }
    
    return attempts;
}

std::vector<ExtensionProtocolDiagnostics::CapabilityNegotiation> ExtensionProtocolDiagnostics::getCapabilityNegotiationsByInfoHash(const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::vector<CapabilityNegotiation> negotiations;
    for (const auto& negotiation : capability_negotiations_) {
        if (negotiation.info_hash == info_hash) {
            negotiations.push_back(negotiation);
        }
    }
    
    return negotiations;
}

std::vector<ExtensionProtocolDiagnostics::ProtocolMessage> ExtensionProtocolDiagnostics::getProtocolMessagesByInfoHash(const std::string& info_hash) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::vector<ProtocolMessage> messages;
    for (const auto& message : protocol_messages_) {
        if (message.info_hash == info_hash) {
            messages.push_back(message);
        }
    }
    
    return messages;
}

int ExtensionProtocolDiagnostics::getHandshakeAttemptCount() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return static_cast<int>(handshake_attempts_.size());
}

int ExtensionProtocolDiagnostics::getCapabilityNegotiationCount() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return static_cast<int>(capability_negotiations_.size());
}

int ExtensionProtocolDiagnostics::getProtocolMessageCount() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    return static_cast<int>(protocol_messages_.size());
}

int ExtensionProtocolDiagnostics::getHandshakeAttemptCountByPeer(const std::string& peer_ip, int peer_port) {
    return static_cast<int>(getHandshakeAttemptsByPeer(peer_ip, peer_port).size());
}

int ExtensionProtocolDiagnostics::getCapabilityNegotiationCountByPeer(const std::string& peer_ip, int peer_port) {
    return static_cast<int>(getCapabilityNegotiationsByPeer(peer_ip, peer_port).size());
}

int ExtensionProtocolDiagnostics::getProtocolMessageCountByPeer(const std::string& peer_ip, int peer_port) {
    return static_cast<int>(getProtocolMessagesByPeer(peer_ip, peer_port).size());
}

int ExtensionProtocolDiagnostics::getHandshakeAttemptCountByInfoHash(const std::string& info_hash) {
    return static_cast<int>(getHandshakeAttemptsByInfoHash(info_hash).size());
}

int ExtensionProtocolDiagnostics::getCapabilityNegotiationCountByInfoHash(const std::string& info_hash) {
    return static_cast<int>(getCapabilityNegotiationsByInfoHash(info_hash).size());
}

int ExtensionProtocolDiagnostics::getProtocolMessageCountByInfoHash(const std::string& info_hash) {
    return static_cast<int>(getProtocolMessagesByInfoHash(info_hash).size());
}

double ExtensionProtocolDiagnostics::getHandshakeSuccessRate() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    if (handshake_attempts_.empty()) {
        return 0.0;
    }
    
    int success_count = 0;
    for (const auto& attempt : handshake_attempts_) {
        if (attempt.status == HandshakeStatus::SUCCESS) {
            success_count++;
        }
    }
    
    return static_cast<double>(success_count) / handshake_attempts_.size();
}

double ExtensionProtocolDiagnostics::getCapabilityNegotiationSuccessRate() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    if (capability_negotiations_.empty()) {
        return 0.0;
    }
    
    int success_count = 0;
    for (const auto& negotiation : capability_negotiations_) {
        if (negotiation.status == NegotiationStatus::SUCCESS) {
            success_count++;
        }
    }
    
    return static_cast<double>(success_count) / capability_negotiations_.size();
}

double ExtensionProtocolDiagnostics::getProtocolMessageSuccessRate() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    if (protocol_messages_.empty()) {
        return 0.0;
    }
    
    int success_count = 0;
    for (const auto& message : protocol_messages_) {
        if (message.status == MessageStatus::SUCCESS) {
            success_count++;
        }
    }
    
    return static_cast<double>(success_count) / protocol_messages_.size();
}

std::map<ExtensionProtocolDiagnostics::ErrorType, int> ExtensionProtocolDiagnostics::getErrorCounts() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::map<ErrorType, int> error_counts;
    
    // Count errors in handshake attempts
    for (const auto& attempt : handshake_attempts_) {
        if (attempt.status == HandshakeStatus::FAILED) {
            ErrorType error_type = classifyError(attempt.error_message);
            error_counts[error_type]++;
        }
    }
    
    // Count errors in capability negotiations
    for (const auto& negotiation : capability_negotiations_) {
        if (negotiation.status == NegotiationStatus::FAILED) {
            ErrorType error_type = classifyError(negotiation.error_message);
            error_counts[error_type]++;
        }
    }
    
    // Count errors in protocol messages
    for (const auto& message : protocol_messages_) {
        if (message.status == MessageStatus::FAILED) {
            ErrorType error_type = classifyError(message.error_message);
            error_counts[error_type]++;
        }
    }
    
    return error_counts;
}

std::map<std::string, int> ExtensionProtocolDiagnostics::getMessageTypeCounts() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::map<std::string, int> message_counts;
    for (const auto& message : protocol_messages_) {
        message_counts[message.message_type]++;
    }
    
    return message_counts;
}

std::map<std::string, int> ExtensionProtocolDiagnostics::getCapabilityCounts() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    std::map<std::string, int> capability_counts;
    for (const auto& negotiation : capability_negotiations_) {
        for (const auto& capability : negotiation.capabilities) {
            capability_counts[capability]++;
        }
    }
    
    return capability_counts;
}

void ExtensionProtocolDiagnostics::clearExpiredData() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Clear expired handshake attempts
    for (auto it = handshake_attempts_.begin(); it != handshake_attempts_.end();) {
        if (now - it->timestamp > std::chrono::milliseconds(config_.data_retention)) {
            it = handshake_attempts_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clear expired capability negotiations
    for (auto it = capability_negotiations_.begin(); it != capability_negotiations_.end();) {
        if (now - it->timestamp > std::chrono::milliseconds(config_.data_retention)) {
            it = capability_negotiations_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clear expired protocol messages
    for (auto it = protocol_messages_.begin(); it != protocol_messages_.end();) {
        if (now - it->timestamp > std::chrono::milliseconds(config_.data_retention)) {
            it = protocol_messages_.erase(it);
        } else {
            ++it;
        }
    }
}

void ExtensionProtocolDiagnostics::clearAllData() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    handshake_attempts_.clear();
    capability_negotiations_.clear();
    protocol_messages_.clear();
}

void ExtensionProtocolDiagnostics::updateConfig(const DiagnosticsConfig& config) {
    config_ = config;
}

ExtensionProtocolDiagnostics::DiagnosticsConfig ExtensionProtocolDiagnostics::getConfig() const {
    return config_;
}

std::map<std::string, std::string> ExtensionProtocolDiagnostics::getHealthStatus() {
    std::map<std::string, std::string> status;
    
    status["handshake_attempts"] = std::to_string(getHandshakeAttemptCount());
    status["capability_negotiations"] = std::to_string(getCapabilityNegotiationCount());
    status["protocol_messages"] = std::to_string(getProtocolMessageCount());
    status["handshake_success_rate"] = std::to_string(getHandshakeSuccessRate());
    status["capability_success_rate"] = std::to_string(getCapabilityNegotiationSuccessRate());
    status["message_success_rate"] = std::to_string(getProtocolMessageSuccessRate());
    status["data_retention"] = std::to_string(config_.data_retention);
    status["max_handshake_attempts"] = std::to_string(config_.max_handshake_attempts);
    status["max_negotiations"] = std::to_string(config_.max_negotiations);
    status["max_messages"] = std::to_string(config_.max_messages);
    
    return status;
}

} // namespace dht_crawler
