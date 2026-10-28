// Replays one ITCH 5.0 trading day from an mmap'd file through the parser and
// reports how many messages of each kind it produced and how fast (stage 1).
// Stage 2 hands each message to the per-stock books.
//
//   build/clang-release/replay data/12302019.NASDAQ_ITCH50
//
// Exit status: 0 if the whole file parsed, 1 on an ITCHError, 2 on bad usage
// or a file that can't be mapped.

#include <chrono>
#include <cstdio>
#include <exception>
#include <variant>

#include "itch/itch_parser.h"
#include "util/mapped_file.h"

namespace {

// Indexed like itch::ITCHMessage's alternatives.
constexpr std::array<const char*, 8> kind_names{
    "A  AddOrder",     "F  AddOrderWithMPID", "E  ExecuteOrder",
    "C  ExecuteOrderWithPrice", "X  CancelOrder", "D  DeleteOrder",
    "U  ReplaceOrder", "   NotSupported"};
static_assert(kind_names.size() == std::variant_size_v<itch::ITCHMessage>);

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <file.NASDAQ_ITCH50>\n", argv[0]);
    return 2;
  }

  try {
    const MappedFile file{argv[1]};
    const auto bytes = file.get_bytes();
    itch::ITCHParser parser{bytes};

    std::array<std::uint64_t, kind_names.size()> counts{};
    std::uint64_t total = 0;
    int status = 0;

    const auto start = std::chrono::steady_clock::now();
    try {
      while (const auto message = parser.next()) {
        ++counts[message->index()];
        ++total;
      }
    } catch (const itch::ITCHError& e) {
      std::fprintf(stderr, "parse stopped after %llu messages: %s\n",
                   static_cast<unsigned long long>(total), e.what());
      status = 1;
    }
    const std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - start;

    std::printf("%-28s %14s\n", "message", "count");
    for (std::size_t i = 0; i < counts.size(); ++i)
      std::printf("%-28s %14llu\n", kind_names[i],
                  static_cast<unsigned long long>(counts[i]));
    std::printf("%-28s %14llu\n", "total",
                static_cast<unsigned long long>(total));
    std::printf("\n%.1f MB in %.3f s: %.1f M msg/s, %.0f MB/s\n",
                static_cast<double>(bytes.size()) / 1e6, elapsed.count(),
                static_cast<double>(total) / elapsed.count() / 1e6,
                static_cast<double>(bytes.size()) / elapsed.count() / 1e6);
    return status;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 2;
  }
}
