// Unit tests for the ITCH message layouts and the parser. Messages are built
// byte by byte from the spec's field tables, independently of the decoder's
// offsets, so a wrong offset in itch_message.h shows up as a wrong field here.

#include "itch/itch_parser.h"

#include "itch_builders.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using namespace itch;
using namespace itch_test;

// --- Message layouts -------------------------------------------------------

TEST(ItchMessage, SizesMatchSpec) {
  EXPECT_EQ(add_order(1, 'B', 1, 1).size(), message::AddOrder::size);
  EXPECT_EQ(add_order_mpid(1, "ABCD").size(), message::AddOrderWithMPID::size);
  EXPECT_EQ(executed(1, 1).size(), message::ExecuteOrder::size);
  EXPECT_EQ(executed_with_price(1, 1, 'Y', 1).size(),
            message::ExecuteOrderWithPrice::size);
  EXPECT_EQ(cancel(1, 1).size(), message::CancelOrder::size);
  EXPECT_EQ(delete_order(1).size(), message::DeleteOrder::size);
  EXPECT_EQ(replace(1, 2, 1, 1).size(), message::ReplaceOrder::size);
}

TEST(ItchMessage, AddOrderFields) {
  const auto m = std::get<message::AddOrder>(
      parse_one(add_order(0x0102030405060708, 'B', 2000, 1'605'800, 496)));
  EXPECT_EQ(m.decode_order_reference_number().value, 0x0102030405060708u);
  EXPECT_EQ(m.decode_side(), Side::Buy);
  EXPECT_EQ(m.decode_shares().value, 2000u);
  EXPECT_EQ(m.decode_price(), Price4{1'605'800});
  EXPECT_EQ(m.decode_stock_locate().value, 496u);
}

TEST(ItchMessage, AddOrderSellSide) {
  const auto m =
      std::get<message::AddOrder>(parse_one(add_order(1, 'S', 100, 500)));
  EXPECT_EQ(m.decode_side(), Side::Sell);
}

TEST(ItchMessage, AddOrderWithMPIDFields) {
  const auto m = std::get<message::AddOrderWithMPID>(
      parse_one(add_order_mpid(42, "LEHM")));
  EXPECT_EQ(m.decode_order_reference_number().value, 42u);
  EXPECT_EQ(m.decode_side(), Side::Sell);
  EXPECT_EQ(m.decode_shares().value, 300u);
  EXPECT_EQ(m.decode_price(), Price4{1'605'800});
  EXPECT_EQ(m.decode_attribution(), "LEHM");
}

TEST(ItchMessage, ExecuteOrderFields) {
  const auto m =
      std::get<message::ExecuteOrder>(parse_one(executed(8881, 600)));
  EXPECT_EQ(m.decode_stock_locate().value, 13u);
  EXPECT_EQ(m.decode_order_reference_number().value, 8881u);
  EXPECT_EQ(m.decode_executed_shares().value, 600u);
  EXPECT_EQ(m.decode_match_number().value, 555u);
}

TEST(ItchMessage, ExecuteOrderWithPriceFields) {
  const auto m = std::get<message::ExecuteOrderWithPrice>(
      parse_one(executed_with_price(8881, 100, 'N', 504'600)));
  EXPECT_EQ(m.decode_order_reference_number().value, 8881u);
  EXPECT_EQ(m.decode_executed_shares().value, 100u);
  EXPECT_EQ(m.decode_match_number().value, 556u);
  EXPECT_EQ(m.decode_printable(), Printable::No);
  EXPECT_EQ(m.decode_execution_price(), Price4{504'600});
}

TEST(ItchMessage, CancelOrderFields) {
  const auto m = std::get<message::CancelOrder>(parse_one(cancel(77, 25)));
  EXPECT_EQ(m.decode_stock_locate().value, 13u);
  EXPECT_EQ(m.decode_order_reference_number().value, 77u);
  EXPECT_EQ(m.decode_cancelled_shares().value, 25u);
}

TEST(ItchMessage, DeleteOrderFields) {
  const auto m = std::get<message::DeleteOrder>(parse_one(delete_order(78)));
  EXPECT_EQ(m.decode_stock_locate().value, 13u);
  EXPECT_EQ(m.decode_order_reference_number().value, 78u);
}

TEST(ItchMessage, ReplaceOrderFields) {
  const auto m = std::get<message::ReplaceOrder>(
      parse_one(replace(100, 200, 50, 1'000'000)));
  EXPECT_EQ(m.decode_stock_locate().value, 13u);
  EXPECT_EQ(m.decode_original_order_reference_number().value, 100u);
  EXPECT_EQ(m.decode_new_order_reference_number().value, 200u);
  EXPECT_EQ(m.decode_shares().value, 50u);
  EXPECT_EQ(m.decode_price(), Price4{1'000'000});
}

// --- Field validation (DecodeError) ---------------------------------------

TEST(ItchMessage, BadSideThrowsDecodeError) {
  const auto m =
      std::get<message::AddOrder>(parse_one(add_order(1, 'Q', 100, 500)));
  try {
    (void)m.decode_side();
    FAIL() << "expected DecodeError";
  } catch (const DecodeError& e) {
    EXPECT_EQ(e.message_type, 'A');
    EXPECT_EQ(e.field, "side");
    EXPECT_EQ(e.offset, 19u);
    EXPECT_EQ(e.value, static_cast<std::uint64_t>('Q'));
  }
}

TEST(ItchMessage, MaxPriceIsValid) {
  const auto m = std::get<message::AddOrder>(
      parse_one(add_order(1, 'B', 100, Price4::max_ticks)));
  EXPECT_EQ(m.decode_price().ticks(), Price4::max_ticks);
}

TEST(ItchMessage, PriceAboveMaxThrowsDecodeError) {
  const auto m = std::get<message::AddOrder>(
      parse_one(add_order(1, 'B', 100, Price4::max_ticks + 1)));
  EXPECT_THROW((void)m.decode_price(), DecodeError);
}

TEST(ItchMessage, BadPrintableThrowsDecodeError) {
  const auto m = std::get<message::ExecuteOrderWithPrice>(
      parse_one(executed_with_price(1, 1, 'X', 500)));
  EXPECT_THROW((void)m.decode_printable(), DecodeError);
}

#if ORDERBOOK_ENABLE_EXPECT
// Building a message from bytes of another type is a parser bug, so it's an
// EXPECT (abort), not an exception. Only checked when EXPECT is enabled.
TEST(ItchMessageDeathTest, WrongTypeByteAborts) {
  auto wrong = delete_order(1);  // 'D', 19 bytes
  wrong[0] = std::byte{'X'};
  const std::span<const std::byte, message::DeleteOrder::size> bytes{wrong};
  EXPECT_DEATH(message::DeleteOrder{bytes}, "Wrong message type");
}
#endif

// --- Price4 ------------------------------------------------------------

TEST(Price4, RejectsAboveMax) {
  EXPECT_NO_THROW(Price4{Price4::max_ticks});
  EXPECT_THROW(Price4{Price4::max_ticks + 1}, std::out_of_range);
}

TEST(Price4, Formats) {
  EXPECT_EQ(std::format("{}", Price4{1'605'800}), "160.5800");
  EXPECT_EQ(std::format("{}", Price4{1}), "0.0001");
  EXPECT_EQ(std::format("{}", Price4{0}), "0.0000");
  EXPECT_EQ(std::format("{}", Price4{Price4::max_ticks}), "200000.0000");
}

// --- Parser ----------------------------------------------------------------

TEST(ItchParser, EmptyInputHasNoMessages) {
  const std::vector<std::byte> input;
  ITCHParser parser{input};
  EXPECT_FALSE(parser.next().has_value());
}

TEST(ItchParser, ReadsMessagesInOrder) {
  Bytes input;
  input.append(framed(add_order(1, 'B', 100, 500)))
      .append(framed(executed(1, 40)))
      .append(framed(cancel(1, 10)))
      .append(framed(replace(1, 2, 70, 600)))
      .append(framed(delete_order(2)));
  ITCHParser parser{input.data};

  std::vector<ITCHMessage> got;
  while (auto m = parser.next()) got.push_back(*m);

  ASSERT_EQ(got.size(), 5u);
  EXPECT_TRUE(std::holds_alternative<message::AddOrder>(got[0]));
  EXPECT_TRUE(std::holds_alternative<message::ExecuteOrder>(got[1]));
  EXPECT_TRUE(std::holds_alternative<message::CancelOrder>(got[2]));
  EXPECT_TRUE(std::holds_alternative<message::ReplaceOrder>(got[3]));
  EXPECT_TRUE(std::holds_alternative<message::DeleteOrder>(got[4]));
}

TEST(ItchParser, UnknownTypeIsNotSupported) {
  // 'S' (System Event, 12 bytes) isn't a book message.
  Bytes system_event;
  system_event.ch('S').u16(0).u16(0).u48(0).ch('O');
  const auto m = parse_one(system_event.data);
  ASSERT_TRUE(std::holds_alternative<message::NotSupported>(m));
  EXPECT_EQ(std::get<message::NotSupported>(m).message_type, 'S');
}

TEST(ItchParser, ContinuesAfterNotSupported) {
  Bytes input;
  Bytes system_event;
  system_event.ch('S').u16(0).u16(0).u48(0).ch('O');
  input.append(framed(system_event.data)).append(framed(delete_order(5)));
  ITCHParser parser{input.data};
  ASSERT_TRUE(parser.next().has_value());
  const auto second = parser.next();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(std::get<message::DeleteOrder>(*second)
                .decode_order_reference_number()
                .value,
            5u);
  EXPECT_FALSE(parser.next().has_value());
}

// --- Framing errors -------------------------------------------------------

TEST(ItchParser, HalfALengthPrefixIsFramingError) {
  const std::vector<std::byte> input{std::byte{0x00}};
  ITCHParser parser{input};
  EXPECT_THROW((void)parser.next(), FramingError);
}

TEST(ItchParser, TruncatedMessageIsFramingError) {
  auto input = framed(delete_order(1));
  input.pop_back();  // lose the last byte of the message
  ITCHParser parser{input};
  EXPECT_THROW((void)parser.next(), FramingError);
}

TEST(ItchParser, TruncatedLastMessageIsFramingError) {
  Bytes input;
  input.append(framed(delete_order(1))).append(framed(delete_order(2)));
  input.data.resize(input.data.size() - 5);
  ITCHParser parser{input.data};
  ASSERT_TRUE(parser.next().has_value());  // the first one is complete
  EXPECT_THROW((void)parser.next(), FramingError);
}

TEST(ItchParser, LengthNotMatchingTypeIsFramingError) {
  auto message = delete_order(1);
  message.push_back(std::byte{0});  // a 'D' that claims 20 bytes
  const auto input = framed(message);
  ITCHParser parser{input};
  EXPECT_THROW((void)parser.next(), FramingError);
}

TEST(ItchParser, ZeroLengthIsFramingError) {
  const std::vector<std::byte> input{std::byte{0x00}, std::byte{0x00}};
  ITCHParser parser{input};
  EXPECT_THROW((void)parser.next(), FramingError);
}

TEST(ItchParser, AllInputErrorsAreITCHErrors) {
  // What the fuzzer harness relies on: one base class to catch.
  const std::vector<std::byte> input{std::byte{0x00}};
  ITCHParser parser{input};
  EXPECT_THROW((void)parser.next(), ITCHError);
}

}  // namespace
