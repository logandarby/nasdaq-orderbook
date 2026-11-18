#ifndef ABEB47EC_F472_4507_8C63_82B7ED294B49
#define ABEB47EC_F472_4507_8C63_82B7ED294B49

#include "market/types.h"

namespace book {

template <class B>
concept OrderBook = std::default_initializable<B> &&
                    requires(B& book, const B& cbook, const OrderRef order,
                             const OrderRef oldOrder, const OrderRef newOrder,
                             const StockLocate locate, const Side side,
                             const Price4 price, const Shares shares) {
                      {
                        book.add(order, locate, side, price, shares)
                      } -> std::same_as<void>;
                      {
                        book.execute(order, locate, shares)
                      } -> std::same_as<void>;
                      {
                        book.cancel(order, locate, shares)
                      } -> std::same_as<void>;
                      { book.remove(order, locate) } -> std::same_as<void>;
                      {
                        book.replace(oldOrder, newOrder, locate, price, shares)
                      } -> std::same_as<void>;
                      {
                        cbook.quantityAt(locate, side, price)
                      } -> std::same_as<Shares>;
                      {
                        cbook.priceLevelCount(locate, side)
                      } -> std::same_as<std::size_t>;
                      {
                        cbook.best(locate, side)
                      } -> std::same_as<std::optional<PriceLevel>>;
                      { cbook.contains(order) } -> std::same_as<bool>;
                    };
}  // namespace book

#endif /* ABEB47EC_F472_4507_8C63_82B7ED294B49 */