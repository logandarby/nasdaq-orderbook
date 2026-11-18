#ifndef B61D991F_E34C_47AE_BB59_D524C5A0CE7F
#define B61D991F_E34C_47AE_BB59_D524C5A0CE7F

// Every order book variant, in one list. Tests, benchmarks and replay read
// this list, so adding a variant means: write its header, static_assert it
// against OrderBook, and add it here.

#include "book/map_book.h"
#include "book/order_book.h"

namespace book {
// A compile-time list of book types. Each one must satisfy OrderBook, so a
// variant that drifts from the interface fails here with a readable error.
template <OrderBook... Books>
struct BookList {};

using AllBooks = BookList<MapBook>;
}  // namespace book

#endif /* B61D991F_E34C_47AE_BB59_D524C5A0CE7F */
