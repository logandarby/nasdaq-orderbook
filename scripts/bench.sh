#!/usr/bin/env bash
# Build and run the benchmarks on the dedicated server, and save the raw
# results with everything needed to reproduce them in results/<run>/.
#
# Usage: scripts/bench.sh [extra Google Benchmark flags]
# Env:   PRESET    build preset (default clang-release)
#        BENCH_CPU core to pin to (default 2; should be an isolated core)
#        REPS      repetitions per benchmark (default 10)
#
# One-time server setup (needs root, then a reboot):
#   - kernel command line: isolcpus=<cores> nohz_full=<cores> rcu_nocbs=<cores>
#   - CPU governor: cpupower frequency-set -g performance
#   - turbo off for stable numbers:
#       Intel: echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
#       AMD:   echo 0 > /sys/devices/system/cpu/cpufreq/boost
#   - perf access: sysctl kernel.perf_event_paranoid=1
# This script only warns if these are not in place; it does not change them.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
preset="${PRESET:-clang-release}"
cpu="${BENCH_CPU:-2}"
reps="${REPS:-10}"

cmake --preset "$preset" >/dev/null
cmake --build --preset "$preset" --target book_bench

sha="$(git rev-parse --short HEAD)"
if ! git diff --quiet HEAD; then
  sha="$sha-dirty"
  echo "warning: uncommitted changes; results can't be tied to a commit" >&2
fi
out="results/$(date +%Y%m%d-%H%M%S)-$preset-$sha"
mkdir -p "$out"

# Check machine state. Warn, don't fail.
cpufreq=/sys/devices/system/cpu/cpu$cpu/cpufreq
governor="$(cat "$cpufreq/scaling_governor" 2>/dev/null || echo unknown)"
isolated="$(cat /sys/devices/system/cpu/isolated 2>/dev/null || true)"
no_turbo="$(cat /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || echo n/a)"
boost="$(cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null || echo n/a)"
[[ "$governor" == performance ]] || echo "warning: governor is '$governor', not 'performance'" >&2
[[ -n "$isolated" ]] || echo "warning: no isolated cores (isolcpus not set)" >&2
[[ "$no_turbo" == 1 || "$boost" == 0 ]] || echo "warning: turbo/boost appears to be on" >&2

cache="build/$preset/CMakeCache.txt"
cxx="$(grep -m1 '^CMAKE_CXX_COMPILER:' "$cache" | cut -d= -f2)"
cxx_flags="$(grep -m1 '^CMAKE_CXX_FLAGS:' "$cache" | cut -d= -f2-)"

{
  echo "date:       $(date -Iseconds)"
  echo "commit:     $sha"
  echo "preset:     $preset"
  echo "compiler:   $cxx ($("$cxx" --version | head -1))"
  echo "cxx flags:  $cxx_flags"
  echo "pinned cpu: $cpu"
  echo "isolated:   ${isolated:-none}"
  echo "governor:   $governor"
  echo "no_turbo:   $no_turbo   boost: $boost"
  echo "kernel:     $(uname -r)"
  echo "cmdline:    $(cat /proc/cmdline)"
  echo
  lscpu
  echo
  free -h
} >"$out/machine.txt"

# Pull the day's data into the page cache so the run measures the code, not the disk.
for f in data/*.NASDAQ_ITCH50; do
  [[ -f "$f" ]] && cat "$f" >/dev/null
done

taskset -c "$cpu" "build/$preset/book_bench" \
  --benchmark_repetitions="$reps" \
  --benchmark_report_aggregates_only=true \
  --benchmark_out="$out/book_bench.json" \
  --benchmark_out_format=json \
  "$@" | tee "$out/book_bench.txt"

echo "results: $out"
