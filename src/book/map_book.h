#ifndef F2DA3744_1EA4_42CB_9CB1_E09C18C49A24
#define F2DA3744_1EA4_42CB_9CB1_E09C18C49A24

#include <map>
#include <unordered_map>
#include <vector>

#include "book/order_book.h"

struct LocateOrderBook {
  std::map<Price4, Shares> bids;
  std::map<Price4, Shares> asks;
};

struct OrderInfo {
  Side side;
  Price4 price;
  Shares remainingShares;
};

class MapBook {
  static constexpr size_t stockLocateCount =
      std::numeric_limits<decltype(StockLocate::value)>::max() + 1;

 public:
  MapBook() : locateToOrderBook(stockLocateCount) {}

  void add(OrderRef order, StockLocate locate, Side side, Price4 price,
           Shares shares) {}
  void execute(OrderRef order, StockLocate locate, Shares shares) {}
  void cancel(OrderRef order, StockLocate locate, Shares shares) {}
  void remove(OrderRef order, StockLocate locate) {}
  void replace(OrderRef oldOrder, OrderRef newOrder, StockLocate locate,
               Price4 price, Shares shares) {}
  Shares quantityAt(StockLocate locate, Side side, Price4 price) const {
    return {0};
  }
  std::size_t priceLevelCount(StockLocate locate, Side side) const { return 0; }
  std::optional<PriceLevel> best(StockLocate locate, Side side) const {
    return {PriceLevel{Price4{0}, Shares{0}}};
  }
  bool contains(StockLocate locate, OrderRef order) const { return false; }

 private:
  std::map<Price4, Shares>& getLevelsOn(Side side, StockLocate locate) {
    if (side == Side::Buy) {
      return locateToOrderBook[locate.value].bids;
    }
    return locateToOrderBook[locate.value].asks;
  }

 private:
  std::vector<LocateOrderBook> locateToOrderBook;
  std::unordered_map<OrderRef, OrderInfo> orderInfo;
};

static_assert(OrderBook<MapBook>);

#endif /* F2DA3744_1EA4_42CB_9CB1_E09C18C49A24 */
