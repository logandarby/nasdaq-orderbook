#ifndef A7E3C1F0_5B2D_4E8A_9C61_3D4F2B8E7A10
#define A7E3C1F0_5B2D_4E8A_9C61_3D4F2B8E7A10

// EXPECT(condition, format, args...): checks an invariant. On failure, prints
// the condition, a std::format message and the source location, then aborts
// (so ASan and the fuzzer report a stack trace).
//
//   EXPECT(src[0] == std::byte{'A'}, "wrong message type {}",
//          std::to_integer<int>(src[0]));
//
// Enabled unless NDEBUG is defined (Release and RelWithDebInfo define it).
// Define ORDERBOOK_ENABLE_EXPECT=1 or 0 to override, e.g. to keep checks on in
// the fuzzer. When disabled, the condition and message are not evaluated.

#include <cstdio>
#include <cstdlib>
#include <format>
#include <source_location>
#include <string>

#ifndef ORDERBOOK_ENABLE_EXPECT
#ifdef NDEBUG
#define ORDERBOOK_ENABLE_EXPECT 0
#else
#define ORDERBOOK_ENABLE_EXPECT 1
#endif
#endif

namespace Expect {

// Out of line and cold, so the failure path stays out of the hot code.
[[noreturn, gnu::cold, gnu::noinline]] inline void fail(
    const char* condition, const std::string& message,
    const std::source_location loc) {
  const std::string text =
      std::format("{}:{}: in {}: EXPECT({}) failed: {}\n", loc.file_name(),
                  loc.line(), loc.function_name(), condition, message);
  std::fputs(text.c_str(), stderr);
  std::abort();
}

}  // namespace Expect

#if ORDERBOOK_ENABLE_EXPECT
#define EXPECT(condition, ...)                             \
  do {                                                     \
    if (!(condition)) [[unlikely]]                         \
      ::Expect::fail(#condition, std::format(__VA_ARGS__), \
                     std::source_location::current());     \
  } while (0)
#else
// sizeof keeps the condition compiled (so it can't rot) without evaluating it.
#define EXPECT(condition, ...)  \
  do {                          \
    (void)sizeof(!(condition)); \
  } while (0)
#endif

#endif /* A7E3C1F0_5B2D_4E8A_9C61_3D4F2B8E7A10 */
