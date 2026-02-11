# cpp_udp_secs_receiver
secs parser with cpp listen to udp port

## implement goal

## project structure

```bash
secs-cpp/
├── CMakeLists.txt          # CMake build config
├── Dockerfile              # Docker image build file
├── README.md               # README
├── QUICKSTART.md           # Quick Start Guide
├── .env.example            # .env example
├── include/                # header files
│   ├── config.h            # config for header
│   ├── message.h           # message structure
│   ├── bounded_queue.h     # Thread-safe queue
│   ├── udp_receiver.h      # UDP reciever (Boost.Asio)
│   ├── parser.h            # JSON parser
│   ├── db_writer.h         # PostgreSQL Writer
│   └── worker_pool.h       # Worker Pool
├── src/                    # source file
│   ├── main.cpp            # entrypoint
│   └── *.cpp              
└── scripts/
    └── build.sh            # build script
```

## install build dependency

```bash
sudo apt-get install -y \
    build-essential cmake g++ \
    libboost-all-dev libpqxx-dev libspdlog-dev
```

## how to build

```bash
# configure env
export DB_HOST=localhost
export DB_PASSWORD=secspass
# for best performance
export WORKER_COUNT=6
export BATCH_SIZE=150
export BATCH_TIMEOUT_MS=30
export DB_POOL_SIZE=6

# build
cd cpp_udp_secs_receiver
chmod +x scripts/build.sh
./scripts/build.sh
```

## how to run

```bash
./build/cpp_udp_secs_receiver
```
