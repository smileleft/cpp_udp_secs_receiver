#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace secs {

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity), closed_(false) {}

    // 큐에 아이템 추가 (블로킹)
    bool push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 큐가 꽉 찬 경우 대기
        not_full_.wait(lock, [this] { 
            return queue_.size() < capacity_ || closed_; 
        });
        
        if (closed_) {
            return false;
        }
        
        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    // 큐에 아이템 추가 (논블로킹, 실패 시 false)
    bool try_push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (queue_.size() >= capacity_ || closed_) {
            return false;
        }
        
        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    // 큐에서 아이템 꺼내기 (타임아웃)
    std::optional<T> pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!not_empty_.wait_for(lock, timeout, [this] { 
            return !queue_.empty() || closed_; 
        })) {
            return std::nullopt;  // 타임아웃
        }
        
        if (closed_ && queue_.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return item;
    }

    // 큐 크기
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // 큐 닫기 (더 이상 push 불가)
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

private:
    const size_t capacity_;
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    bool closed_;
};

} // namespace secs
