#ifndef C3735BD1_6C82_4CAB_916F_F1DFBBE0DAF0
#define C3735BD1_6C82_4CAB_916F_F1DFBBE0DAF0

#include "common.h"

struct StockLocate {
  uint16_t value;
};
static_assert(sizeof(StockLocate) == sizeof(std::uint16_t));
static_assert(std::is_trivially_copyable_v<StockLocate>);

struct Shares {
  uint32_t value;
};
static_assert(sizeof(Shares) == sizeof(std::uint32_t));
static_assert(std::is_trivially_copyable_v<Shares>);

struct OrderRef {
  uint64_t value;
};
static_assert(sizeof(OrderRef) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<OrderRef>);

struct MatchNumber {
  uint64_t value;
};
static_assert(sizeof(MatchNumber) == sizeof(std::uint64_t));
static_assert(std::is_trivially_copyable_v<MatchNumber>);

enum class Printable {
  Yes,
  No,
};

enum class Side {
  Buy,
  Sell,
};

// A Price(4): an integer number of 0.0001 ticks. Construction enforces the
// spec's maximum of 200,000.0000, so every Price4 is valid.
class Price4 {
 public:
  static constexpr std::uint32_t max_ticks = 2'000'000'000;

  constexpr explicit Price4(const std::uint32_t ticks) : ticks_(ticks) {
    if (ticks > max_ticks) [[unlikely]]
      throw std::out_of_range("Price(4) above 200,000.0000");
  }

  constexpr std::uint32_t ticks() const { return ticks_; }

  auto operator<=>(Price4 const&) const = default;

 private:
  std::uint32_t ticks_;
};
static_assert(sizeof(Price4) == sizeof(std::uint32_t));
static_assert(std::is_trivially_copyable_v<Price4>);

template <>
struct std::formatter<Price4> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const Price4& p, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}.{:04}", p.ticks() / 10000,
                          p.ticks() % 10000);
  }
};

#endif /* C3735BD1_6C82_4CAB_916F_F1DFBBE0DAF0 */
