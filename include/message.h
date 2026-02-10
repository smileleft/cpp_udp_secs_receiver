#pragma once

#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

namespace secs {

using json = nlohmann::json;

// 원본 UDP 패킷
struct RawMessage {
    std::vector<uint8_t> data;
    
    RawMessage() = default;
    
    explicit RawMessage(const uint8_t* buf, size_t len) 
        : data(buf, buf + len) {}
    
    size_t size() const { return data.size(); }
    const uint8_t* bytes() const { return data.data(); }
};

// 파싱된 메시지 (공통 필드)
struct ParsedMessage {
    int stream;
    int function;
    std::string timestamp;
    int device_id;
    std::string system_bytes;
    json raw_body;  // 원본 body (DB JSONB 저장용)
    
    virtual ~ParsedMessage() = default;
    virtual std::string table_name() const = 0;
    
    // 공통 필드 추출
    static void extract_common(const json& msg, ParsedMessage& out) {
        out.stream = msg.value("stream", 0);
        out.function = msg.value("function", 0);
        out.timestamp = msg.value("timestamp", "");
        out.device_id = msg.value("deviceId", 0);
        out.system_bytes = msg.value("systemBytes", "");
        out.raw_body = msg.value("body", json::object());
    }
};

// S2F49 – Carrier Transfer Command
struct S2F49Message : public ParsedMessage {
    int txn_code;
    std::string txn_id;
    std::string command_type;
    std::string command_id;
    int priority;
    std::string carrier_id;
    std::string source;
    std::string dest;
    std::string source_type;
    std::string dest_type;
    
    std::string table_name() const override { 
        return "s2f49_transfer_commands"; 
    }
};

// 배치 처리용 컨테이너
struct MessageBatch {
    std::vector<RawMessage> raw_messages;
    std::vector<std::shared_ptr<ParsedMessage>> parsed_messages;
    
    void reserve(size_t n) {
        raw_messages.reserve(n);
        parsed_messages.reserve(n);
    }
    
    size_t size() const {
        return raw_messages.size();
    }
    
    void clear() {
        raw_messages.clear();
        parsed_messages.clear();
    }
};

} // namespace secs
