# nasdaq-orderbook

Rebuilds per-stock limit order books from a full trading day of Nasdaq TotalView-ITCH 5.0 data, then measures how different book layouts and inter-thread queues perform on that data.

**Status:** This project is under construction! Scaffolding & parser only. Books and queues aren't written yet. Check out [NOTES.md](NOTES.md) for progress.

## How this repo was set up

The scaffolding (CMake presets, CI, scripts, empty test, benchmark and fuzz harnesses) was generated with an AI assistant (Claude Code). I write the parser, the order book variants and the queues myself, and I make every data-structure and memory-ordering decision myself, with a measurement behind it. Each experiment is recorded in [NOTES.md](NOTES.md).

## Build

Requires CMake 3.25+ and Clang or GCC with C++20.

```sh
cmake --preset clang-asan && cmake --build --preset clang-asan && ctest --preset clang-asan
```

Presets: `clang-asan` / `gcc-asan` (debug, ASan + UBSan), `clang-tsan`, `clang-release` / `gcc-release` (benchmarks), `clang-fuzz`.

Data isn't committed. `scripts/fetch_data.sh [MMDDYYYY]` downloads a trading day into `data/`.

## Testing

- **Correctness first:** every book variant must produce exactly the same book as a `std::map` reference over the full day (differential test), and must pass the invariant checks (no negative sizes, no unknown order IDs).
- **CI** builds and tests with GCC and Clang under ASan + UBSan, with warnings as errors. It runs on a small real-data slice (`scripts/make_fixture.py`), never on the full day.
- **Fuzzing:** libFuzzer runs against the ITCH parser (`clang-fuzz`).

## Benchmarking

All published numbers come from one dedicated bare-metal Linux server, never from CI, a VM or WSL. `scripts/bench.sh` pins to an isolated core, checks the governor and turbo, warms the page cache, and saves the raw JSON together with the machine, compiler and commit in `results/`. Release builds are benchmarked with both Clang and GCC.
