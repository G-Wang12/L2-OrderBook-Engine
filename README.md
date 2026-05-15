# L2 OrderBook High-Frequency Execution Engine

A C++20, low-latency **market-data → strategy** engine skeleton built around the core HFT loop: ingest L2 updates over WSS, normalize them into a small internal representation, hand off across threads via a lock-free SPSC queue, and run a busy-wait strategy loop with microsecond-level latency visibility.

The strategy layer is intentionally modular: you can run a single alpha signal or an ensemble of multiple signals in the same process (selected at startup), combine their confidence scores, and emit mock fills/PnL to CSV for rapid iteration.

**Current Status**: Working end-to-end pipeline (local replay/mock tooling included) with a pluggable strategy/alpha framework. Not a production execution system yet (no real venue auth/subscribe, reconnect/ping-pong, OMS, or risk).

## Features

- **C++20** with strict compiler enforcement (`-Wall -Wextra -Wpedantic -Werror`)
- **Performance**: Release builds use `-O3 -march=native` for maximum speed on target hardware
- **SIMD JSON parsing**: [simdjson](https://github.com/simdjson/simdjson) on-demand parsing with a fixed-capacity buffer (no per-message allocations; one memcpy into a padded buffer)
- **Networking (WSS)**: Boost.Asio + Boost.Beast over OpenSSL (TLS)
- **Concurrency**: dedicated network thread + dedicated strategy thread, coordinated via a lock-free SPSC queue
- **Testing**: GoogleTest integration with CMake FetchContent
- **Build System**: Modern CMake 3.20+ with FetchContent for dependencies
- **Modular Signals (Alphas)**: Strategy runs one or more pluggable alpha signals and combines their confidence scores
- **Latency Visibility**: CSV logs include per-tick processing latency (`latency_us`) so you can quantify signal overhead

## Quick Start

### Prerequisites

- CMake ≥ 3.20
- C++20 compiler (GCC 11+, Clang 13+, MSVC 2019+)
- Boost (system + thread components) — install via:
  ```bash
  # macOS
  brew install boost
  # Ubuntu
  sudo apt install libboost-system-dev libboost-thread-dev
  ```
- OpenSSL (TLS) — install via:
  ```bash
  # macOS
  brew install openssl@3
  # Ubuntu
  sudo apt install libssl-dev
  ```
- Git

### Build & Run

```bash
# Clone and configure
git clone <your-repo-url>
cd low-latency-prediction-market-engine

# Clean previous build (important after dependency/search-path changes)
rm -rf build

# Configure (uses FetchContent for simdjson + GoogleTest)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build (engine + tests)
cmake --build build -j$(getconf _NPROCESSORS_ONLN)

# Run the engine
# Note: running with no args defaults to example.com:443 and uses a placeholder subscribe payload,
# so for a deterministic local run you usually want the mock or replay server below.

# Select which strategy signals (alphas) to run (default is both)
./build/engine --strategy momentum
./build/engine --strategy ofi
./build/engine --strategy both

# Run tests
ctest --test-dir build -V
```

#### Deterministic local run (recommended)

1. Generate a local TLS cert/key (used by the Python WSS servers):

```bash
./tools/gen_self_signed_cert.sh
```

2. In one terminal, start a local websocket server:

```bash
source .venv/bin/activate
python tools/mock_wss_server.py
# or
python tools/replay_server.py
```

3. In another terminal, point the engine at it:

```bash
./build/engine --strategy both 127.0.0.1 8765 /
```

### Recommended Environment Setup (Conda + Python tooling)

This repo has two independent “worlds”:

- **C++ build/run** (CMake + Clang/GCC + system/Homebrew libs)
- **Python tooling** (used for local websocket tooling in `tools/`: recorder, replay server, mock server, dashboard)

To keep the C++ toolchain deterministic on macOS, it’s recommended to **deactivate Conda `base`** in the terminal you use for building/running C++:

```bash
conda deactivate
```

For the Python mock server, use a repo-local virtualenv instead of installing packages into Conda `base`:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip websockets certifi
```

Practical workflow: run the Python server in one terminal (with `(.venv)` active), and build/run the engine in another terminal (with neither `(.venv)` nor `(base)` active).

### Recording real Polymarket data (JSONL)

The recorder in `tools/record_polymarket.py` connects to Polymarket’s public Market Channel websocket and appends every incoming JSON message to `historical_data.jsonl`.

1. Ensure the repo-local venv has `websockets` installed:

```bash
source .venv/bin/activate
python -m pip install -U pip websockets certifi
```

2. Edit `ASSET_IDS` in `tools/record_polymarket.py` (these are Polymarket _asset IDs / token IDs_).

3. Run the recorder:

```bash
source .venv/bin/activate
python tools/record_polymarket.py
```

### Replaying recorded Polymarket data (local TLS websocket)

The replay server in `tools/replay_server.py` serves `historical_data.jsonl` over a local TLS websocket and replays messages in order.

It prefers replaying the original on-the-wire websocket payload when available (`raw_message` / `raw` fields), and uses `local_timestamp_ns` to reproduce short-term burstiness.

1. Ensure the repo-local venv has dependencies installed:

```bash
source .venv/bin/activate
python -m pip install -U pip websockets
```

2. Ensure you have a local TLS cert/key for the server:

```bash
./tools/gen_self_signed_cert.sh
```

This generates `tools/cert.pem` and `tools/key.pem` (used by the replay server).

3. Run the replay server:

```bash
source .venv/bin/activate
python tools/replay_server.py
```

Optional: cap inter-message sleep time to avoid multi-minute gaps if your JSONL contains time discontinuities (default is `0.5` seconds):

```bash
REPLAY_MAX_SLEEP_S=0.1 python tools/replay_server.py
```

4. Point the engine at the replay server:

```bash
./build/engine --strategy both 127.0.0.1 8765 /
```

At this point `trading_log.csv` should start accumulating rows, and the Streamlit dashboard should stop showing the “headers only” warning.

### Real-time Dashboard (Streamlit)

The Streamlit dashboard reads a CSV file named `trading_log.csv` (by default).

Required columns (used for plots/metrics):

`timestamp_us,event_type,price,size,realized_pnl`

Additional columns written by the engine:

- `latency_us`: tick-to-log processing latency in microseconds (captures alpha + decision overhead)
- `strategy`: strategy name for metadata rows

Event types:

- `T`: trade event
- `P`: periodic mark-to-market point
- `M`: metadata (e.g., active strategy name)

1. Install dashboard deps (recommended inside the repo-local `(.venv)`):

```bash
source .venv/bin/activate
python -m pip install -U streamlit pandas plotly streamlit-autorefresh
```

2. Run the dashboard:

```bash
streamlit run tools/dashboard.py
```

3. Point it at a different log file (optional):

```bash
TRADING_LOG_PATH=/path/to/trading_log.csv streamlit run tools/dashboard.py
```

Note: the dashboard will show a warning until `trading_log.csv` exists and has data.

The sidebar also shows the **Active Strategy** (parsed from metadata rows written at engine startup).

Optional (quick sanity check): generate a tiny sample log file:

```bash
python - <<'PY'
import csv
import time

rows = [
  (0, 'P', 0.0, 0, 0.0),
  (500_000, 'T', 0.59, 10, 0.0),
  (1_000_000, 'P', 0.0, 0, 0.12),
  (1_500_000, 'T', 0.57, -10, 0.12),
  (2_000_000, 'P', 0.0, 0, 0.20),
]

with open('trading_log.csv', 'w', newline='') as f:
  w = csv.writer(f)
  w.writerow(['timestamp_us', 'event_type', 'price', 'size', 'realized_pnl'])
  base = int(time.time() * 1_000_000)
  for t, et, price, size, pnl in rows:
    w.writerow([base + t, et, price, size, pnl])
print('wrote trading_log.csv')
PY
```

**Note for macOS users in conda `(base)`**: conda often injects search paths that can cause a mixed Boost install to be detected (e.g., Homebrew BoostConfig + conda `boost_system`). The build defaults to ignoring `CONDA_PREFIX` during dependency discovery; override with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLLPME_IGNORE_CONDA_PREFIX=OFF
```

**Expected output**: the engine is mostly quiet when healthy. You should see `trading_log.csv` being written and the dashboard updating; on Ctrl+C you should see `Shutdown complete.`

## Project Structure

```
.
├── CMakeLists.txt          # Root build configuration (strict flags, deps)
├── include/                # Public API headers (orderbook, engine interfaces)
├── src/
│   ├── main.cpp            # Entry point / wiring / shutdown
│   ├── strategy_engine.cpp # Strategy thread loop
│   └── websocket_client.cpp # Boost.Beast WSS client
├── tests/
│   └── test_main.cpp       # GoogleTest suite
├── tools/
│   ├── mock_wss_server.py   # Local TLS websocket tick generator
│   ├── record_polymarket.py  # Record Polymarket Market Channel JSONL
│   ├── replay_server.py      # Replay historical_data.jsonl over local WSS
│   └── dashboard.py         # Streamlit dashboard for trading_log.csv
├── .gitignore
├── README.md
└── build/                  # Generated (ignored)
```

## Development

### Adding Components

1. Place headers in `include/`
2. Implementation in `src/`
3. Tests in `tests/`
4. Update `CMakeLists.txt` targets as modules grow (consider `add_library` for core engine)

### Testing

```bash
# After building (see Build & Run)
ctest --test-dir build -V
```

### Performance Tuning

- Profile with `perf`, Valgrind, or Tracy
- Ensure `-march=native` matches production hardware
- Monitor cache misses and branch prediction in hot paths (order matching, risk checks)

## Dependencies (Managed by CMake)

- **simdjson** (v3.6.3): Zero-allocation JSON parsing
- **GoogleTest** (v1.15.2): Unit testing
- **Boost**: system + thread (required)
- **OpenSSL**: TLS for `wss://`

## GitHub Setup

This repository includes:

- Comprehensive `.gitignore` for C++/CMake/IDE artifacts
- GitHub Actions CI (see `.github/workflows/ci.yml`) for automated builds/tests across platforms
- Modern CMake with dependency management and macOS/conda-friendly Boost discovery

### CI/CD Recommendations

- Add GitHub Actions workflow for:
  - Linux (Ubuntu) + macOS matrix
  - Release builds with sanitizers (`-fsanitize=address,undefined`)
  - Benchmarking and performance regression tests
- Use `clang-tidy` and `include-what-you-use` for static analysis
- Consider `conan` or `vcpkg` for full dependency management in larger projects

## Roadmap

- [ ] More realistic order book model (multi-asset routing, sequencing/consistency checks)
- [ ] Venue-specific subscribe/auth + TLS verification policy
- [ ] Reconnect/backoff and ping/pong in the C++ websocket client
- [ ] Matching engine with low-latency priority queues
- [ ] Risk management and position tracking
- [ ] Benchmark suite (latency histograms, throughput)
- [ ] Python bindings (via pybind11) for research

## License

This project is currently unlicensed (all rights reserved).

## Contributing

Not currently accepting external contributions.

---

_Built for high-frequency prediction market execution. Questions? Open an issue._
