#pragma once

#include "config.h"
#include "message.h"
#include "bounded_queue.h"
#include "parser.h"
#include "db_writer.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

namespace secs {

class WorkerPool {
public:
    WorkerPool(const Config& cfg, BoundedQueue<RawMessage>& queue)
        : config_(cfg)
        , queue_(queue)
        , running_(false)
    {}

    void start() {
        running_ = true;
        
        // Worker 스레드 생성
        for (size_t i = 0; i < config_.worker_count; ++i) {
            workers_.emplace_back([this, i]() {
                worker_main(i);
            });
        }
        
        spdlog::info("Worker Pool 시작: {} workers", config_.worker_count);
    }

    void stop() {
        running_ = false;
        
        // 모든 워커 종료 대기
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        spdlog::info("Worker Pool 종료");
    }

private:
    void worker_main(size_t worker_id) {
        try {
            // Worker별 전용 DB connection
            DatabaseWriter db_writer(config_);
            
            spdlog::info("Worker #{} 시작 (전용 DB connection 할당)", worker_id);
            
            MessageBatch batch;
            batch.reserve(config_.batch_size);
            
            auto batch_deadline = std::chrono::steady_clock::now() + 
                                 std::chrono::milliseconds(config_.batch_timeout_ms);
            
            while (running_) {
                // 큐에서 메시지 수집 (timeout)
                auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                    batch_deadline - std::chrono::steady_clock::now()
                );
                
                if (timeout.count() <= 0) {
                    timeout = std::chrono::milliseconds(1);
                }
                
                auto opt_msg = queue_.pop(timeout);
                
                if (opt_msg) {
                    // 파싱
                    auto parsed = MessageParser::parse(*opt_msg);
                    
                    // 배치에 추가
                    batch.raw_messages.push_back(std::move(*opt_msg));
                    batch.parsed_messages.push_back(parsed);
                }
                
                // 배치 처리 조건
                bool batch_full = batch.size() >= config_.batch_size;
                bool timeout_expired = std::chrono::steady_clock::now() >= batch_deadline;
                
                if ((batch_full || timeout_expired) && batch.size() > 0) {
                    // DB 삽입
                    db_writer.insert_batch(batch);
                    
                    spdlog::debug("Worker #{}: 배치 {}건 처리 완료", 
                                 worker_id, batch.size());
                    
                    // 배치 리셋
                    batch.clear();
                    batch_deadline = std::chrono::steady_clock::now() + 
                                    std::chrono::milliseconds(config_.batch_timeout_ms);
                }
            }
            
            // 종료 시 남은 배치 처리
            if (batch.size() > 0) {
                db_writer.insert_batch(batch);
                spdlog::info("Worker #{}: 종료 전 남은 배치 {}건 처리", 
                            worker_id, batch.size());
            }
            
            spdlog::info("Worker #{} 종료 (총 {}건 삽입)", 
                        worker_id, db_writer.total_inserted());
        }
        catch (const std::exception& e) {
            spdlog::error("Worker #{} 오류: {}", worker_id, e.what());
        }
    }

private:
    const Config& config_;
    BoundedQueue<RawMessage>& queue_;
    std::atomic<bool> running_;
    std::vector<std::thread> workers_;
};

} // namespace secs
