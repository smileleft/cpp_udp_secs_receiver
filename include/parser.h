#pragma once

#include "message.h"
#include <spdlog/spdlog.h>
#include <memory>

namespace secs {

class MessageParser {
public:
    // RawMessage → ParsedMessage 변환
    static std::shared_ptr<ParsedMessage> parse(const RawMessage& raw) {
        try {
            // JSON 파싱
            std::string_view data_view(
                reinterpret_cast<const char*>(raw.data.data()),
                raw.data.size()
            );
            json msg = json::parse(data_view);
            
            int stream = msg.value("stream", 0);
            int function = msg.value("function", 0);
            
            // Stream/Function에 따라 파서 선택
            if (stream == 2 && function == 49) {
                return parse_s2f49(msg);
            }
            // 다른 메시지 타입들...
            
            spdlog::warn("지원되지 않는 메시지: S{}F{}", stream, function);
            return nullptr;
        }
        catch (const json::exception& e) {
            spdlog::error("JSON 파싱 실패: {}", e.what());
            return nullptr;
        }
    }

private:
    // S2F49 파서
    static std::shared_ptr<ParsedMessage> parse_s2f49(const json& msg) {
        auto parsed = std::make_shared<S2F49Message>();
        
        // 공통 필드
        ParsedMessage::extract_common(msg, *parsed);
        
        // Body 파싱
        const json& body = msg["body"];
        if (!body.is_object() || body["type"] != "L") {
            return nullptr;
        }
        
        const json& body_list = body["value"];
        if (!body_list.is_array() || body_list.size() < 4) {
            return nullptr;
        }
        
        // L[0]: txn_code (U2)
        parsed->txn_code = get_numeric(body_list[0]);
        
        // L[1]: txn_id (A)
        parsed->txn_id = get_string(body_list[1]);
        
        // L[2]: command_type (A)
        parsed->command_type = get_string(body_list[2]);
        
        // L[3]: Named sections
        const json& sections = body_list[3];
        auto section_map = extract_named_sections(sections);
        
        // COMMANDINFO
        if (section_map.count("COMMANDINFO")) {
            const auto& cmd_info = section_map["COMMANDINFO"];
            parsed->command_id = cmd_info.value("COMMANDID", "");
            parsed->priority = cmd_info.value("PRIORITY", 0);
        }
        
        // TRANSFERINFO
        if (section_map.count("TRANSFERINFO")) {
            const auto& transfer_info = section_map["TRANSFERINFO"];
            parsed->carrier_id = transfer_info.value("CARRIERID", "");
            parsed->source = transfer_info.value("SOURCE", "");
            parsed->dest = transfer_info.value("DEST", "");
            parsed->source_type = transfer_info.value("SOURCETYPE", "");
            parsed->dest_type = transfer_info.value("DESTTYPE", "");
        }
        
        return parsed;
    }
    
    // 헬퍼 함수들
    static std::string get_string(const json& node) {
        if (node.is_object() && node["type"] == "A") {
            return node.value("value", "");
        }
        return "";
    }
    
    static int get_numeric(const json& node) {
        if (node.is_object() && node.contains("value")) {
            std::string type = node.value("type", "");
            if (type.starts_with("U") || type.starts_with("I") || type.starts_with("F")) {
                const json& val = node["value"];
                if (val.is_array() && !val.empty()) {
                    return val[0].get<int>();
                }
            }
        }
        return 0;
    }
    
    // Named sections 추출
    static std::map<std::string, json> extract_named_sections(const json& container) {
        std::map<std::string, json> result;
        
        if (!container.is_object() || container["type"] != "L") {
            return result;
        }
        
        const json& sections = container["value"];
        if (!sections.is_array()) {
            return result;
        }
        
        for (const auto& section : sections) {
            if (!section.is_object() || section["type"] != "L") {
                continue;
            }
            
            const json& items = section["value"];
            if (!items.is_array() || items.size() < 2) {
                continue;
            }
            
            std::string name = get_string(items[0]);
            if (name.empty()) {
                continue;
            }
            
            // 키-값 쌍 추출
            json kv_map = json::object();
            const json& kv_list = items[1];
            if (kv_list.is_object() && kv_list["type"] == "L") {
                const json& pairs = kv_list["value"];
                if (pairs.is_array()) {
                    for (const auto& pair : pairs) {
                        if (pair.is_object() && pair["type"] == "L") {
                            const json& kv = pair["value"];
                            if (kv.is_array() && kv.size() >= 2) {
                                std::string key = get_string(kv[0]);
                                if (!key.empty()) {
                                    // value는 타입에 따라 처리
                                    if (kv[1]["type"] == "A") {
                                        kv_map[key] = get_string(kv[1]);
                                    } else {
                                        kv_map[key] = get_numeric(kv[1]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            result[name] = kv_map;
        }
        
        return result;
    }
};

} // namespace secs
