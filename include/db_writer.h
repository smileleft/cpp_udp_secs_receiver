#pragma once

#include "config.h"
#include "message.h"
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>
#include <memory>
#include <sstream>

namespace secs {

class DatabaseWriter {
public:
    explicit DatabaseWriter(const Config& cfg) 
        : config_(cfg)
        , total_inserted_(0)
    {
        // Connection string 생성
        std::ostringstream oss;
        oss << "host=" << cfg.db_host
            << " port=" << cfg.db_port
            << " dbname=" << cfg.db_name
            << " user=" << cfg.db_user
            << " password=" << cfg.db_password;
        
        conn_str_ = oss.str();
        
        // Connection 생성
        conn_ = std::make_unique<pqxx::connection>(conn_str_);
        
        spdlog::info("DB 연결 성공: {}:{}/{}", 
                     cfg.db_host, cfg.db_port, cfg.db_name);
    }

    // 배치 단위 삽입
    void insert_batch(const MessageBatch& batch) {
        if (batch.size() == 0) {
            return;
        }
        
        try {
            pqxx::work txn(*conn_);
            
            for (size_t i = 0; i < batch.size(); ++i) {
                const auto& raw_msg = batch.raw_messages[i];
                const auto& parsed = batch.parsed_messages[i];
                
                // 1. secs_raw_messages 삽입
                int raw_id = insert_raw_message(txn, raw_msg, parsed);
                
                // 2. 파싱된 테이블 삽입
                if (parsed) {
                    insert_parsed_message(txn, parsed, raw_id);
                }
            }
            
            txn.commit();
            total_inserted_ += batch.size();
        }
        catch (const pqxx::sql_error& e) {
            spdlog::error("DB 오류: {} - Query: {}", e.what(), e.query());
            throw;
        }
        catch (const std::exception& e) {
            spdlog::error("DB 배치 삽입 실패: {}", e.what());
            throw;
        }
    }

    uint64_t total_inserted() const { return total_inserted_; }

private:
    int insert_raw_message(pqxx::work& txn, const RawMessage& raw, 
                          const std::shared_ptr<ParsedMessage>& parsed) {
        
        // Raw JSON 파싱하여 timestamp 추출
        std::string timestamp_val;
        int stream_val = 0;
        int function_val = 0;
        bool wbit_val = false;
        int device_id_val = 0;
        std::string system_bytes_val;
        std::string raw_body_str = "{}";
        
        try {
            std::string raw_json(raw.bytes(), raw.bytes() + raw.size());
            json msg = json::parse(raw_json);
            
            timestamp_val = msg.value("timestamp", "");
            stream_val = msg.value("stream", 0);
            function_val = msg.value("function", 0);
            wbit_val = msg.value("wbit", false);
            device_id_val = msg.value("deviceId", 0);
            system_bytes_val = msg.value("systemBytes", "");
            
            if (msg.contains("body")) {
                raw_body_str = msg["body"].dump();
            }
        }
        catch (const json::exception& e) {
            spdlog::warn("Raw JSON 파싱 실패 (timestamp 추출): {}", e.what());
            // 파싱 실패 시 현재 시간 사용
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
            timestamp_val = ss.str() + "Z";
        }
        
        // timestamp가 비어있으면 현재 시간 사용
        if (timestamp_val.empty()) {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
            timestamp_val = ss.str() + "Z";
        }
        
        std::string query = 
            "INSERT INTO secs_raw_messages "
            "(timestamp, stream, function, wbit, device_id, system_bytes, ptype, stype, raw_body) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9::jsonb) "
            "RETURNING id";
        
        pqxx::result r = txn.exec_params(
            query,
            timestamp_val,
            stream_val,
            function_val,
            wbit_val,
            device_id_val,
            system_bytes_val,
            0,  // ptype
            0,  // stype
            raw_body_str
        );
        
        return r[0][0].as<int>();
    }

    void insert_parsed_message(pqxx::work& txn, const std::shared_ptr<ParsedMessage>& parsed, int raw_id) {
        if (auto s2f49 = std::dynamic_pointer_cast<S2F49Message>(parsed)) {
            insert_s2f49(txn, s2f49, raw_id);
        }
        // 다른 메시지 타입들...
    }

    void insert_s2f49(pqxx::work& txn, const std::shared_ptr<S2F49Message>& msg, int raw_id) {
        std::string query = 
            "INSERT INTO s2f49_transfer_commands "
            "(raw_message_id, timestamp, device_id, system_bytes, "
            " txn_code, txn_id, command_type, command_id, priority, "
            " carrier_id, source, dest, source_type, dest_type) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)";
        
        txn.exec_params(
            query,
            raw_id,
            msg->timestamp,
            msg->device_id,
            msg->system_bytes,
            msg->txn_code,
            msg->txn_id,
            msg->command_type,
            msg->command_id,
            msg->priority,
            msg->carrier_id,
            msg->source,
            msg->dest,
            msg->source_type,
            msg->dest_type
        );
    }

    void insert_s6f11(pqxx::work& txn, const std::shared_ptr<S6F11Message>& msg, int raw_id) {
        std::string query = 
            "INSERT INTO s6f11_event_reports "
            "(raw_message_id, timestamp, device_id, system_bytes, "
            " event_report_id, event_id, data_items) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb)";
        
        txn.exec_params(
            query,
            raw_id,
            msg->timestamp,
            msg->device_id,
            msg->system_bytes,
            msg->event_report_id,
            msg->event_id,
            msg->data_items.dump()
        );
    }

private:
    const Config& config_;
    std::string conn_str_;
    std::unique_ptr<pqxx::connection> conn_;
    uint64_t total_inserted_;
};

} // namespace secs
