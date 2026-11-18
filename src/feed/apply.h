#ifndef F08E270D_9880_48C7_BEBF_1144A0DF7DDA
#define F08E270D_9880_48C7_BEBF_1144A0DF7DDA

#include "book/order_book.h"
#include "common.h"
#include "itch/itch_message.h"
#include "util/overloaded.h"

namespace feed {
template <book::OrderBook Book>
void applyMessageToBook(Book& book, const itch::ITCHMessage& message) {
  using namespace itch::message;
  std::visit(overloaded{
                 [&](const AddOrder& order) {
                   book.add(order.decode_order_reference_number(),
                            order.decode_stock_locate(), order.decode_side(),
                            order.decode_price(), order.decode_shares());
                 },
                 [&](const AddOrderWithMPID& order) {
                   book.add(order.decode_order_reference_number(),
                            order.decode_stock_locate(), order.decode_side(),
                            order.decode_price(), order.decode_shares());
                 },
                 [&](const ExecuteOrder& order) {
                   book.execute(order.decode_order_reference_number(),
                                order.decode_stock_locate(),
                                order.decode_executed_shares());
                 },
                 [&](const ExecuteOrderWithPrice& order) {
                   book.execute(order.decode_order_reference_number(),
                                order.decode_stock_locate(),
                                order.decode_executed_shares());
                 },
                 [&](const CancelOrder& order) {
                   book.cancel(order.decode_order_reference_number(),
                               order.decode_stock_locate(),
                               order.decode_cancelled_shares());
                 },
                 [&](const DeleteOrder& order) {
                   book.remove(order.decode_order_reference_number(),
                               order.decode_stock_locate());
                 },
                 [&](const ReplaceOrder& order) {
                   book.replace(order.decode_original_order_reference_number(),
                                order.decode_new_order_reference_number(),
                                order.decode_stock_locate(),
                                order.decode_price(), order.decode_shares());
                 },
                 [&](const NotSupported&) {},
             },
             message);
}
}  // namespace feed

#endif /* F08E270D_9880_48C7_BEBF_1144A0DF7DDA */
