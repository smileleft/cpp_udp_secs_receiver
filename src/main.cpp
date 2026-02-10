#include "config.h"
#include "bounded_queue.h"
#include "udp_receiver.h"
#include "worker_pool.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <csignal>
#include <atomic>
#include <thread>

namespace {
    std::atomic<bool> g_shutdown{false};
    
    void signal_handler(int signal) {
        spdlog::info("종료 시그널 수신: {}", signal);
        g_shutdown = true;
    }
}

int main(int argc, char* argv[]) {
    // 로깅 설정
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    std::string separator(60, '=');
	spdlog::info(separator);
    spdlog::info("SECS UDP Receiver (C++) v1.0.0");
	spdlog::info(separator);
    
    // 시그널 핸들러 등록
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // 설정 로드
        auto config = secs::Config::from_env();
        
        spdlog::info("설정:");
        spdlog::info("  UDP: {}:{}", config.udp_host, config.udp_port);
        spdlog::info("  DB:  {}:{}/{}", config.db_host, config.db_port, config.db_name);
        spdlog::info("  성능: Queue={}, Workers={}, Batch={}, Timeout={}ms",
                    config.queue_capacity, config.worker_count, 
                    config.batch_size, config.batch_timeout_ms);
        
        // 메시지 큐 생성
        secs::BoundedQueue<secs::RawMessage> queue(config.queue_capacity);
        spdlog::info("메시지 큐 생성 완료 (capacity={})", config.queue_capacity);
        
        // Worker Pool 시작
        secs::WorkerPool worker_pool(config, queue);
        worker_pool.start();
        
        // UDP 수신 시작 (별도 스레드)
        secs::UdpReceiver receiver(config, queue);
		std::thread udp_thread([&receiver]() {
    		receiver.start();
		});
        
        spdlog::info("SECS UDP Receiver 시작 완료");
 		spdlog::info(separator);
        
        // 종료 시그널 대기
        while (!g_shutdown) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
 		spdlog::info(separator);
        spdlog::info("종료 중...");
        
        // 그레이스풀 종료
        queue.close();
        worker_pool.stop();
        
        if (udp_thread.joinable()) {
            udp_thread.join();
        }
        
        spdlog::info("SECS UDP Receiver 종료 완료");
        
        return 0;
    }
    catch (const std::exception& e) {
        spdlog::error("치명적 오류: {}", e.what());
        return 1;
    }
}
