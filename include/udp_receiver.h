#pragma once

#include "config.h"
#include "message.h"
#include "bounded_queue.h"
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <atomic>
#include <array>

namespace secs {

using boost::asio::ip::udp;

class UdpReceiver {
public:
    UdpReceiver(const Config& cfg, BoundedQueue<RawMessage>& queue)
        : config_(cfg)
        , queue_(queue)
        , socket_(io_context_)
        , running_(false)
        , total_received_(0)
        , total_bytes_(0)
    {}

    void start() {
        // UDP 소켓 바인딩
        udp::endpoint endpoint(
            boost::asio::ip::address::from_string(config_.udp_host),
            config_.udp_port
        );
        
        socket_.open(udp::v4());
        socket_.bind(endpoint);
        
        // SO_RCVBUF 크기 증가 (25MB)
        boost::asio::socket_base::receive_buffer_size option(25 * 1024 * 1024);
        socket_.set_option(option);
        
        spdlog::info("UDP 수신 시작: {}:{}", config_.udp_host, config_.udp_port);
        
        running_ = true;
        start_receive();
        
        // io_context 실행 (블로킹)
        io_context_.run();
    }

    void stop() {
        running_ = false;
        socket_.close();
        io_context_.stop();
        
        spdlog::info("UDP 수신 중단 (총 {}건, {} bytes)", 
                     total_received_.load(), 
                     total_bytes_.load());
    }

    uint64_t total_received() const { return total_received_.load(); }
    uint64_t total_bytes() const { return total_bytes_.load(); }

private:
    void start_receive() {
        socket_.async_receive_from(
            boost::asio::buffer(recv_buffer_),
            remote_endpoint_,
            [this](const boost::system::error_code& ec, std::size_t bytes_recvd) {
                handle_receive(ec, bytes_recvd);
            }
        );
    }

    void handle_receive(const boost::system::error_code& ec, std::size_t bytes_recvd) {
        if (!ec && running_) {
            // 통계 업데이트
            total_received_++;
            total_bytes_ += bytes_recvd;
            
            // 큐에 추가 (논블로킹)
            RawMessage msg(recv_buffer_.data(), bytes_recvd);
            if (!queue_.try_push(std::move(msg))) {
                // 큐 오버플로우 → 메시지 드롭
                spdlog::warn("큐 오버플로우 - 메시지 드롭 (qsize={})", queue_.size());
            }
            
            // 다음 수신 대기
            start_receive();
        }
        else if (ec) {
            spdlog::error("UDP 수신 오류: {}", ec.message());
        }
    }

private:
    const Config& config_;
    BoundedQueue<RawMessage>& queue_;
    
    boost::asio::io_context io_context_;
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<uint8_t, 65536> recv_buffer_;  // 64KB 버퍼
    
    std::atomic<bool> running_;
    std::atomic<uint64_t> total_received_;
    std::atomic<uint64_t> total_bytes_;
};

} // namespace secs
