# cpp_udp_secs_receiver
secs parser with cpp listen to udp port

## implement goal

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
export WORKER_COUNT=4

# build
cd cpp_udp_secs_receiver
chmod +x scripts/build.sh
./scripts/build.sh
```

## how to run
