// Google Benchmark harness. Book variants are registered here in stage 2.
// Run on the benchmark server through scripts/bench.sh, not directly.

#include <benchmark/benchmark.h>

static void BM_Placeholder(benchmark::State& state) {
  for (auto _ : state) {
    int x = 0;
    benchmark::DoNotOptimize(x);
  }
}
BENCHMARK(BM_Placeholder);

BENCHMARK_MAIN();
