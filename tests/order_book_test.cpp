// Behaviour tests for every order book in AllBooks. Tests only use the
// OrderBook concept's interface (never a book's internals), so a new variant
// added to AllBooks is covered automatically.
//
// ITCH mapping: add = A/F, execute = E/C, cancel = X (partial cancel),
// remove = D (delete), replace = U (new ref and price/shares; side and locate
// carry over from the old order).

#include "book/all_books.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <optional>
#include <ostream>

// gtest printers, so failures show values instead of byte dumps. They must be
// in the types' namespace (global) for gtest to find them by ADL.
static void PrintTo(const Shares& s, std::ostream* os) {
  *os << "Shares{" << s.value << "}";
}
static void PrintTo(const PriceLevel& l, std::ostream* os) {
  *os << std::format("PriceLevel{{{}, {}}}", l.price, l.shares.value);
}

namespace {

// AllBooks is a BookList<...>; gtest wants ::testing::Types<...>.
template <class>
struct ToGTestTypes;
template <class... Books>
struct ToGTestTypes<book::BookList<Books...>> {
  using type = ::testing::Types<Books...>;
};
using BookTypes = ToGTestTypes<book::AllBooks>::type;

constexpr StockLocate kLoc{1};
constexpr StockLocate kOtherLoc{2};

OrderRef ref(std::uint64_t r) { return OrderRef{r}; }
Price4 px(std::uint32_t ticks) { return Price4{ticks}; }
Shares sh(std::uint32_t n) { return Shares{n}; }

template <class Book>
class OrderBookTest : public ::testing::Test {
 protected:
  Book book;
};
TYPED_TEST_SUITE(OrderBookTest, BookTypes);

// ---- Empty book --------------------------------------------------------

TYPED_TEST(OrderBookTest, EmptyBookHasNothing) {
  const auto& b = this->book;
  EXPECT_EQ(b.quantityAt(kLoc, Side::Buy, px(100)), sh(0));
  EXPECT_EQ(b.quantityAt(kLoc, Side::Sell, px(100)), sh(0));
  EXPECT_EQ(b.priceLevelCount(kLoc, Side::Buy), 0u);
  EXPECT_EQ(b.priceLevelCount(kLoc, Side::Sell), 0u);
  EXPECT_EQ(b.best(kLoc, Side::Buy), std::nullopt);
  EXPECT_EQ(b.best(kLoc, Side::Sell), std::nullopt);
  EXPECT_FALSE(b.contains(ref(1)));
}

// ---- add ---------------------------------------------------------------

TYPED_TEST(OrderBookTest, AddCreatesLevel) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  EXPECT_TRUE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(50));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

TYPED_TEST(OrderBookTest, AddsAtSamePriceShareOneLevel) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Buy, px(100), sh(30));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(80));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

TYPED_TEST(OrderBookTest, AddsAtDifferentPricesMakeSeparateLevels) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Sell, px(101), sh(30));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(100)), sh(50));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(101)), sh(30));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 2u);
}

TYPED_TEST(OrderBookTest, SidesAreSeparate) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(100)), sh(0));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 0u);
  EXPECT_EQ(this->book.best(kLoc, Side::Sell), std::nullopt);
}

TYPED_TEST(OrderBookTest, LocatesAreSeparate) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  EXPECT_EQ(this->book.quantityAt(kOtherLoc, Side::Buy, px(100)), sh(0));
  EXPECT_EQ(this->book.priceLevelCount(kOtherLoc, Side::Buy), 0u);
}

TYPED_TEST(OrderBookTest, HandlesHighestLocate) {
  const StockLocate last{UINT16_MAX};
  this->book.add(ref(1), last, Side::Buy, px(100), sh(50));
  EXPECT_EQ(this->book.quantityAt(last, Side::Buy, px(100)), sh(50));
}

// ---- best --------------------------------------------------------------

TYPED_TEST(OrderBookTest, BestBidIsHighestPrice) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(10));
  this->book.add(ref(2), kLoc, Side::Buy, px(102), sh(20));
  this->book.add(ref(3), kLoc, Side::Buy, px(101), sh(30));
  EXPECT_EQ(this->book.best(kLoc, Side::Buy), (PriceLevel{px(102), sh(20)}));
}

TYPED_TEST(OrderBookTest, BestAskIsLowestPrice) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(10));
  this->book.add(ref(2), kLoc, Side::Sell, px(102), sh(20));
  this->book.add(ref(3), kLoc, Side::Sell, px(99), sh(30));
  EXPECT_EQ(this->book.best(kLoc, Side::Sell), (PriceLevel{px(99), sh(30)}));
}

TYPED_TEST(OrderBookTest, BestShowsLevelTotal) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(10));
  this->book.add(ref(2), kLoc, Side::Buy, px(100), sh(15));
  EXPECT_EQ(this->book.best(kLoc, Side::Buy), (PriceLevel{px(100), sh(25)}));
}

TYPED_TEST(OrderBookTest, BestMovesWhenTopLevelEmpties) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(10));
  this->book.add(ref(2), kLoc, Side::Buy, px(102), sh(20));
  this->book.execute(ref(2), kLoc, sh(20));
  EXPECT_EQ(this->book.best(kLoc, Side::Buy), (PriceLevel{px(100), sh(10)}));
}

// ---- execute -----------------------------------------------------------

TYPED_TEST(OrderBookTest, PartialExecuteReducesOrderAndLevel) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.execute(ref(1), kLoc, sh(20));
  EXPECT_TRUE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(30));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

// Catches the order's remaining shares not being written back: the second
// execute only fits if the first one was recorded on the order.
TYPED_TEST(OrderBookTest, RepeatedPartialExecutesUseRemainingShares) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.execute(ref(1), kLoc, sh(20));
  this->book.execute(ref(1), kLoc, sh(20));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(10));
  this->book.execute(ref(1), kLoc, sh(10));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 0u);
}

TYPED_TEST(OrderBookTest, FullExecuteRemovesOrderAndLevel) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(50));
  this->book.execute(ref(1), kLoc, sh(50));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(100)), sh(0));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 0u);
  EXPECT_EQ(this->book.best(kLoc, Side::Sell), std::nullopt);
}

TYPED_TEST(OrderBookTest, FullExecuteKeepsLevelWithOtherOrders) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Buy, px(100), sh(30));
  this->book.execute(ref(1), kLoc, sh(50));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_TRUE(this->book.contains(ref(2)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(30));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

TYPED_TEST(OrderBookTest, ExecuteLeavesOtherLevelsAlone) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Buy, px(99), sh(40));
  this->book.execute(ref(1), kLoc, sh(10));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(99)), sh(40));
}

// A filled order's ref is free again (it can't be reused within a day in
// real ITCH, but the book shouldn't keep a stale entry for it).
TYPED_TEST(OrderBookTest, FilledOrderRefCanBeAddedAgain) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.execute(ref(1), kLoc, sh(50));
  this->book.add(ref(1), kLoc, Side::Sell, px(105), sh(5));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(105)), sh(5));
}

// ---- cancel (X: partial cancel) ----------------------------------------

TYPED_TEST(OrderBookTest, PartialCancelReducesOrderAndLevel) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(50));
  this->book.cancel(ref(1), kLoc, sh(20));
  EXPECT_TRUE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(100)), sh(30));
}

TYPED_TEST(OrderBookTest, CancelThenExecuteUsesRemainingShares) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(50));
  this->book.cancel(ref(1), kLoc, sh(20));
  this->book.execute(ref(1), kLoc, sh(30));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 0u);
}

// Design choice (yours): an X for all remaining shares leaves nothing to
// trade. This only checks the level; whether `contains` stays true until the
// D arrives is up to you.
TYPED_TEST(OrderBookTest, CancelAllRemainingEmptiesLevel) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(50));
  this->book.cancel(ref(1), kLoc, sh(50));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(100)), sh(0));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 0u);
}

// ---- remove (D: delete) ------------------------------------------------

TYPED_TEST(OrderBookTest, RemoveDeletesOrderAndEmptyLevel) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.remove(ref(1), kLoc);
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(0));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 0u);
}

TYPED_TEST(OrderBookTest, RemoveTakesOnlyRemainingShares) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Buy, px(100), sh(30));
  this->book.execute(ref(1), kLoc, sh(20));
  this->book.remove(ref(1), kLoc);
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(30));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

// ---- replace (U) -------------------------------------------------------

TYPED_TEST(OrderBookTest, ReplaceMovesToNewRefAndPrice) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.replace(ref(1), ref(2), kLoc, px(101), sh(40));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_TRUE(this->book.contains(ref(2)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(0));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(101)), sh(40));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

TYPED_TEST(OrderBookTest, ReplaceKeepsSide) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(50));
  this->book.replace(ref(1), ref(2), kLoc, px(101), sh(40));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(101)), sh(40));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 0u);
}

TYPED_TEST(OrderBookTest, ReplaceAtSamePriceChangesShares) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Buy, px(100), sh(30));
  this->book.replace(ref(1), ref(3), kLoc, px(100), sh(10));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(40));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

// The old order's *remaining* shares leave the old level, not its original
// size.
TYPED_TEST(OrderBookTest, ReplaceAfterPartialExecuteRemovesRemaining) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Buy, px(100), sh(30));
  this->book.execute(ref(1), kLoc, sh(20));
  this->book.replace(ref(1), ref(3), kLoc, px(99), sh(25));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(100)), sh(30));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(99)), sh(25));
}

TYPED_TEST(OrderBookTest, ReplacedOrderCanBeExecuted) {
  this->book.add(ref(1), kLoc, Side::Sell, px(100), sh(50));
  this->book.replace(ref(1), ref(2), kLoc, px(101), sh(40));
  this->book.execute(ref(2), kLoc, sh(40));
  EXPECT_FALSE(this->book.contains(ref(2)));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 0u);
}

// ---- Invariant checks (EXPECT aborts) ----------------------------------
// Only meaningful when EXPECT is compiled in (debug and sanitizer presets).
// The *DeathTest suite name makes gtest run these first, before any threads.

#if ORDERBOOK_ENABLE_EXPECT

template <class Book>
class OrderBookDeathTest : public OrderBookTest<Book> {};
TYPED_TEST_SUITE(OrderBookDeathTest, BookTypes);

constexpr const char* kExpectFailed = "EXPECT.*failed";

TYPED_TEST(OrderBookDeathTest, AddZeroSharesAborts) {
  EXPECT_DEATH(this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(0)),
               kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, AddDuplicateRefAborts) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  EXPECT_DEATH(this->book.add(ref(1), kLoc, Side::Sell, px(101), sh(10)),
               kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, AddOverflowingLevelAborts) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(UINT32_MAX));
  EXPECT_DEATH(this->book.add(ref(2), kLoc, Side::Buy, px(100), sh(1)),
               kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, ExecuteUnknownOrderAborts) {
  EXPECT_DEATH(this->book.execute(ref(1), kLoc, sh(10)), kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, ExecuteZeroSharesAborts) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  EXPECT_DEATH(this->book.execute(ref(1), kLoc, sh(0)), kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, ExecuteMoreThanRemainingAborts) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.execute(ref(1), kLoc, sh(30));
  EXPECT_DEATH(this->book.execute(ref(1), kLoc, sh(21)), kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, CancelUnknownOrderAborts) {
  EXPECT_DEATH(this->book.cancel(ref(1), kLoc, sh(10)), kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, CancelMoreThanRemainingAborts) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  EXPECT_DEATH(this->book.cancel(ref(1), kLoc, sh(51)), kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, RemoveUnknownOrderAborts) {
  EXPECT_DEATH(this->book.remove(ref(1), kLoc), kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, ReplaceUnknownOrderAborts) {
  EXPECT_DEATH(this->book.replace(ref(1), ref(2), kLoc, px(100), sh(10)),
               kExpectFailed);
}

TYPED_TEST(OrderBookDeathTest, ReplaceToExistingRefAborts) {
  this->book.add(ref(1), kLoc, Side::Buy, px(100), sh(50));
  this->book.add(ref(2), kLoc, Side::Buy, px(101), sh(50));
  EXPECT_DEATH(this->book.replace(ref(1), ref(2), kLoc, px(102), sh(10)),
               kExpectFailed);
}

#endif  // ORDERBOOK_ENABLE_EXPECT

}  // namespace
