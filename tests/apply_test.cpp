// Tests for apply(): each ITCH message type must reach the right book call
// with the right fields (ref, locate, side, price, shares). Messages are built
// from bytes and parsed, as in the parser tests, then applied to every book in
// AllBooks.
//
// Mapping under test: A, F -> add; E, C -> execute (C at the order's resting
// price, spec 1.4.2); X -> cancel; D -> remove; U -> replace;
// NotSupported -> nothing.

#include "feed/apply.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "book/all_books.h"
#include "itch_builders.h"

namespace {

using namespace itch_test;

template <class>
struct ToGTestTypes;
template <class... Books>
struct ToGTestTypes<book::BookList<Books...>> {
  using type = ::testing::Types<Books...>;
};

constexpr std::uint16_t kLocate = 496;
constexpr StockLocate kLoc{kLocate};

Price4 px(std::uint32_t ticks) { return Price4{ticks}; }
Shares sh(std::uint32_t n) { return Shares{n}; }
OrderRef ref(std::uint64_t r) { return OrderRef{r}; }

template <class Book>
class ApplyTest : public ::testing::Test {
 protected:
  // Parses one message from its bytes and applies it to the book.
  // Qualified on purpose: ITCHMessage is a std::variant, so an unqualified
  // call also finds std::apply by argument-dependent lookup.
  void feed(const std::vector<std::byte>& message) {
    feed::applyMessageToBook(book, parse_one(message));
  }

  Book book;
};
TYPED_TEST_SUITE(ApplyTest, ToGTestTypes<book::AllBooks>::type);

// ---- A: AddOrder -------------------------------------------------------

TYPED_TEST(ApplyTest, AddOrderBuyAddsBid) {
  this->feed(add_order(1, 'B', 100, 1'605'800, kLocate));
  EXPECT_TRUE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(1'605'800)).value, 100u);
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 0u);
}

TYPED_TEST(ApplyTest, AddOrderSellAddsAsk) {
  this->feed(add_order(1, 'S', 100, 1'605'800, kLocate));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(1'605'800)).value, 100u);
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 0u);
}

// The book is indexed by the message's locate, not a fixed one.
TYPED_TEST(ApplyTest, AddOrderUsesMessageLocate) {
  this->feed(add_order(1, 'B', 100, 500, /*locate*/ 7));
  EXPECT_EQ(this->book.quantityAt(StockLocate{7}, Side::Buy, px(500)).value,
            100u);
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(500)).value, 0u);
}

// ---- F: AddOrderWithMPID -----------------------------------------------

TYPED_TEST(ApplyTest, AddOrderWithMPIDAdds) {
  this->feed(add_order_mpid(1, "LEHM", 'B', 250, 99'000, kLocate));
  EXPECT_TRUE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(99'000)).value, 250u);
}

TYPED_TEST(ApplyTest, AddOrderWithMPIDKeepsSide) {
  this->feed(add_order_mpid(1, "LEHM", 'S', 250, 99'000, kLocate));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(99'000)).value, 250u);
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 0u);
}

// ---- E: ExecuteOrder ---------------------------------------------------

TYPED_TEST(ApplyTest, ExecuteOrderReducesOrder) {
  this->feed(add_order(1, 'B', 100, 500, kLocate));
  this->feed(executed(1, 30, kLocate));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(500)).value, 70u);
  EXPECT_TRUE(this->book.contains(ref(1)));
}

TYPED_TEST(ApplyTest, ExecuteOrderFullyFillsOrder) {
  this->feed(add_order(1, 'B', 100, 500, kLocate));
  this->feed(executed(1, 100, kLocate));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 0u);
}

// ---- C: ExecuteOrderWithPrice ------------------------------------------

TYPED_TEST(ApplyTest, ExecuteWithPriceReducesOrder) {
  this->feed(add_order(1, 'S', 100, 500, kLocate));
  this->feed(executed_with_price(1, 40, 'Y', 500, kLocate));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(500)).value, 60u);
}

// The C message's price is the execution price, which can differ from the
// display price. The shares come off the order's resting level.
TYPED_TEST(ApplyTest, ExecuteWithPriceUsesRestingPrice) {
  this->feed(add_order(1, 'S', 100, 500, kLocate));
  this->feed(executed_with_price(1, 40, 'N', 495, kLocate));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(500)).value, 60u);
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(495)).value, 0u);
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 1u);
}

// ---- X: CancelOrder ----------------------------------------------------

TYPED_TEST(ApplyTest, CancelOrderReducesOrder) {
  this->feed(add_order(1, 'B', 100, 500, kLocate));
  this->feed(cancel(1, 25, kLocate));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(500)).value, 75u);
  EXPECT_TRUE(this->book.contains(ref(1)));
}

// ---- D: DeleteOrder ----------------------------------------------------

TYPED_TEST(ApplyTest, DeleteOrderRemovesOrder) {
  this->feed(add_order(1, 'B', 100, 500, kLocate));
  this->feed(add_order(2, 'B', 40, 500, kLocate));
  this->feed(delete_order(1, kLocate));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(500)).value, 40u);
}

// ---- U: ReplaceOrder ---------------------------------------------------

// Checks the field order too: original ref is removed, new ref is added.
TYPED_TEST(ApplyTest, ReplaceOrderMovesOrder) {
  this->feed(add_order(1, 'S', 100, 500, kLocate));
  this->feed(replace(/*original*/ 1, /*new*/ 2, 80, 510, kLocate));
  EXPECT_FALSE(this->book.contains(ref(1)));
  EXPECT_TRUE(this->book.contains(ref(2)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(500)).value, 0u);
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Sell, px(510)).value, 80u);
}

// ---- Not supported -----------------------------------------------------

TYPED_TEST(ApplyTest, NotSupportedChangesNothing) {
  this->feed(add_order(1, 'B', 100, 500, kLocate));
  this->feed(system_event());
  EXPECT_TRUE(this->book.contains(ref(1)));
  EXPECT_EQ(this->book.quantityAt(kLoc, Side::Buy, px(500)).value, 100u);
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
}

// ---- A short sequence --------------------------------------------------

// Every message type once, in a realistic order; ends with a known book.
TYPED_TEST(ApplyTest, SequenceEndsWithExpectedBook) {
  this->feed(add_order(1, 'B', 100, 500, kLocate));
  this->feed(add_order(2, 'B', 50, 499, kLocate));
  this->feed(add_order_mpid(3, "LEHM", 'S', 200, 505, kLocate));
  this->feed(system_event());
  this->feed(executed(1, 20, kLocate));                       // bid 500: 80
  this->feed(executed_with_price(3, 50, 'Y', 505, kLocate));  // ask 505: 150
  this->feed(cancel(2, 10, kLocate));                         // bid 499: 40
  this->feed(replace(1, 4, 90, 501, kLocate));  // bid 500 -> 501: 90
  this->feed(delete_order(2, kLocate));         // bid 499 gone

  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Buy), 1u);
  EXPECT_EQ(this->book.best(kLoc, Side::Buy), (PriceLevel{px(501), sh(90)}));
  EXPECT_EQ(this->book.priceLevelCount(kLoc, Side::Sell), 1u);
  EXPECT_EQ(this->book.best(kLoc, Side::Sell), (PriceLevel{px(505), sh(150)}));
}

}  // namespace
