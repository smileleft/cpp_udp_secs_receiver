#pragma once

#include <string>
#include <cstdint>
#include <cstdlib>

namespace secs {

struct Config {
    // Database
    std::string db_host;
    uint16_t db_port;
    std::string db_name;
    std::string db_user;
    std::string db_password;
    size_t db_pool_size;

    // UDP
    std::string udp_host;
    uint16_t udp_port;

    // Performance
    size_t queue_capacity;
    size_t worker_count;
    size_t batch_size;
    size_t batch_timeout_ms;

    static Config from_env() {
        Config cfg;
        
        // Database
        cfg.db_host = getenv_or("DB_HOST", "localhost");
        cfg.db_port = std::stoi(getenv_or("DB_PORT", "5432"));
        cfg.db_name = getenv_or("DB_NAME", "secs_db");
        cfg.db_user = getenv_or("DB_USER", "secs_user");
        cfg.db_password = getenv_or("DB_PASSWORD", "secspass");
        cfg.db_pool_size = std::stoul(getenv_or("DB_POOL_SIZE", "4"));

        // UDP
        cfg.udp_host = getenv_or("UDP_HOST", "0.0.0.0");
        cfg.udp_port = std::stoi(getenv_or("UDP_PORT", "5000"));

        // Performance
        cfg.queue_capacity = std::stoul(getenv_or("QUEUE_CAPACITY", "100000"));
        cfg.worker_count = std::stoul(getenv_or("WORKER_COUNT", "4"));
        cfg.batch_size = std::stoul(getenv_or("BATCH_SIZE", "100"));
        cfg.batch_timeout_ms = std::stoul(getenv_or("BATCH_TIMEOUT_MS", "50"));

        return cfg;
    }

private:
    static std::string getenv_or(const char* name, const char* default_val) {
        const char* val = std::getenv(name);
        return val ? std::string(val) : std::string(default_val);
    }
};

} // namespace secs
