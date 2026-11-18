#ifndef F2DA3744_1EA4_42CB_9CB1_E09C18C49A24
#define F2DA3744_1EA4_42CB_9CB1_E09C18C49A24

#include <map>
#include <unordered_map>
#include <vector>

#include "book/order_book.h"

namespace book {

class MapBook {
  static constexpr size_t stockLocateCount =
      std::numeric_limits<decltype(StockLocate::value)>::max() + 1;
  struct LocateOrderBook {
    std::map<Price4, Shares, std::greater<Price4>> bids;
    std::map<Price4, Shares> asks;
  };

  struct OrderInfo {
    Side side;
    Price4 price;
    Shares remainingShares;
  };

 private:
  template <typename F>
  decltype(auto) getLevelsOn(Side side, StockLocate locate, F&& f) {
    if (side == Side::Buy) {
      return f(locateToOrderBook[locate.value].bids);
    }
    return f(locateToOrderBook[locate.value].asks);
  }
  template <typename F>
  decltype(auto) getLevelsOn(Side side, StockLocate locate, F&& f) const {
    if (side == Side::Buy) {
      return f(locateToOrderBook[locate.value].bids);
    }
    return f(locateToOrderBook[locate.value].asks);
  }

 public:
  MapBook() : locateToOrderBook(stockLocateCount) {}

  void add(OrderRef order, StockLocate locate, Side side, Price4 price,
           Shares shares) {
    EXPECT(shares > Shares{0}, "Shares must be > 0");
    auto [_, inserted] = orderInfo.try_emplace(order, side, price, shares);
    EXPECT(inserted, "Order must not already exist");
    getLevelsOn(side, locate, [&](auto& levels) { levels[price] += shares; });
  }

  void execute(OrderRef order, StockLocate locate, Shares shares) {
    cancel(order, locate, shares);
  }

  void cancel(OrderRef order, StockLocate locate, Shares shares) {
    EXPECT(shares > Shares{0}, "Shares must be > 0");
    auto infoIt = orderInfo.find(order);
    EXPECT(infoIt != orderInfo.end(), "Order does not exist");
    auto& info = infoIt->second;
    EXPECT(info.remainingShares >= shares,
           "remainingShares must be >= executedShares");
    info.remainingShares -= shares;
    getLevelsOn(info.side, locate, [&](auto& orderLevels) {
      auto orderShares = orderLevels.find(info.price);
      EXPECT(orderShares != orderLevels.end(), "Price level {} does not exist.",
             info.price);
      EXPECT(orderShares->second >= shares,
             "Price level {} doesn't contain enough shares to execute",
             orderShares->first);
      if (orderShares->second == shares) {
        orderLevels.erase(orderShares);
      } else {
        orderShares->second -= shares;
      }
    });
    if (info.remainingShares == Shares{0}) {
      orderInfo.erase(infoIt);
    }
  }

  void remove(OrderRef order, StockLocate locate) {
    removeAndGetSide(order, locate);
  }

  void replace(OrderRef oldOrder, OrderRef newOrder, StockLocate locate,
               Price4 price, Shares shares) {
    const Side side = removeAndGetSide(oldOrder, locate);
    add(newOrder, locate, side, price, shares);
  }

  // Get the quantities at a certain price level, returning 0 if it doesn't
  // exist
  Shares quantityAt(StockLocate locate, Side side, Price4 price) const {
    return getLevelsOn(side, locate, [&](auto& levels) {
      auto it = levels.find(price);
      return it == levels.end() ? Shares{0} : it->second;
    });
  }

  // Returns how many price levels exist at a certain locate at a certain side
  std::size_t priceLevelCount(StockLocate locate, Side side) const {
    return getLevelsOn(side, locate,
                       [&](auto& levels) { return levels.size(); });
  }

  std::optional<PriceLevel> best(StockLocate locate, Side side) const {
    return getLevelsOn(side, locate, [&](auto& levels) {
      return levels.empty() ? std::nullopt
                            : std::optional{PriceLevel(levels.begin()->first,
                                                       levels.begin()->second)};
    });
  }

  // Returns if the order exists
  bool contains(OrderRef order) const {
    return orderInfo.find(order) != orderInfo.end();
  }

 private:
  Side removeAndGetSide(OrderRef order, StockLocate locate) {
    auto it = orderInfo.find(order);
    EXPECT(it != orderInfo.end(), "Order {} must exist", order);
    auto& info = it->second;
    Side side = info.side;
    getLevelsOn(info.side, locate, [&](auto& levels) {
      auto levelIt = levels.find(info.price);
      EXPECT(levelIt != levels.end(), "Price level {} must exist", info.price);
      Shares& levelShares = levelIt->second;
      EXPECT(levelShares >= info.remainingShares,
             "Price level {} does not have enough shares ({}) to remove "
             "(remining shares: {})",
             info.price, levelShares, info.remainingShares);
      if (levelShares == info.remainingShares) {
        levels.erase(levelIt);
      } else {
        levelShares -= info.remainingShares;
      }
    });
    orderInfo.erase(it);
    return side;
  }

 private:
  std::vector<LocateOrderBook> locateToOrderBook;
  std::unordered_map<OrderRef, OrderInfo> orderInfo;
};

static_assert(OrderBook<MapBook>);
}  // namespace book

#endif /* F2DA3744_1EA4_42CB_9CB1_E09C18C49A24 */
