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
        std::string raw_json(raw.bytes(), raw.bytes() + raw.size());
        
        std::string query = 
            "INSERT INTO secs_raw_messages "
            "(timestamp, stream, function, wbit, device_id, system_bytes, ptype, stype, raw_body) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9::jsonb) "
            "RETURNING id";
        
        pqxx::result r = txn.exec_params(
            query,
            parsed ? parsed->timestamp : "",
            parsed ? parsed->stream : 0,
            parsed ? parsed->function : 0,
            false,  // wbit
            parsed ? parsed->device_id : 0,
            parsed ? parsed->system_bytes : "",
            0,  // ptype
            0,  // stype
            parsed ? parsed->raw_body.dump() : "{}"
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

private:
    const Config& config_;
    std::string conn_str_;
    std::unique_ptr<pqxx::connection> conn_;
    uint64_t total_inserted_;
};

} // namespace secs
