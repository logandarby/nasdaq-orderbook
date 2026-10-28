// Unit tests for the ITCH message layouts and the parser. Messages are built
// byte by byte from the spec's field tables, independently of the decoder's
// offsets, so a wrong offset in itch_message.h shows up as a wrong field here.

#include "itch/itch_parser.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using namespace itch;

// Appends big-endian fields, the way ITCH puts them on the wire.
class Bytes {
 public:
  Bytes& u8(const std::uint8_t v) {
    data.push_back(std::byte{v});
    return *this;
  }
  Bytes& ch(const char c) { return u8(static_cast<std::uint8_t>(c)); }
  Bytes& u16(const std::uint16_t v) { return be(v, 2); }
  Bytes& u32(const std::uint32_t v) { return be(v, 4); }
  Bytes& u48(const std::uint64_t v) { return be(v, 6); }
  Bytes& u64(const std::uint64_t v) { return be(v, 8); }
  // Alpha field: ASCII, left-justified, padded with spaces.
  Bytes& alpha(const std::string_view s, const std::size_t width) {
    for (std::size_t i = 0; i < width; ++i) ch(i < s.size() ? s[i] : ' ');
    return *this;
  }
  Bytes& append(const std::vector<std::byte>& more) {
    data.insert(data.end(), more.begin(), more.end());
    return *this;
  }

  std::vector<std::byte> data;

 private:
  Bytes& be(const std::uint64_t v, const int width) {
    for (int i = width - 1; i >= 0; --i)
      u8(static_cast<std::uint8_t>(v >> (8 * i)));
    return *this;
  }
};

// Header shared by every message: type, stock locate, tracking number,
// timestamp.
Bytes header(const char type, const std::uint16_t locate) {
  Bytes b;
  b.ch(type)
      .u16(locate)
      .u16(/*tracking*/ 7)
      .u48(/*timestamp*/ 34'200'000'000'000);
  return b;
}

std::vector<std::byte> add_order(const std::uint64_t ref, const char side,
                                 const std::uint32_t shares,
                                 const std::uint32_t price,
                                 const std::uint16_t locate = 13) {
  return header('A', locate)
      .u64(ref)
      .ch(side)
      .u32(shares)
      .alpha("AAPL", 8)
      .u32(price)
      .data;
}

std::vector<std::byte> add_order_mpid(const std::uint64_t ref,
                                      const std::string_view mpid) {
  return header('F', 13)
      .u64(ref)
      .ch('S')
      .u32(300)
      .alpha("AAPL", 8)
      .u32(1'605'800)
      .alpha(mpid, 4)
      .data;
}

std::vector<std::byte> executed(const std::uint64_t ref,
                                const std::uint32_t shares) {
  return header('E', 13).u64(ref).u32(shares).u64(/*match*/ 555).data;
}

std::vector<std::byte> executed_with_price(const std::uint64_t ref,
                                           const std::uint32_t shares,
                                           const char printable,
                                           const std::uint32_t price) {
  return header('C', 13)
      .u64(ref)
      .u32(shares)
      .u64(/*match*/ 556)
      .ch(printable)
      .u32(price)
      .data;
}

std::vector<std::byte> cancel(const std::uint64_t ref,
                              const std::uint32_t shares) {
  return header('X', 13).u64(ref).u32(shares).data;
}

std::vector<std::byte> delete_order(const std::uint64_t ref) {
  return header('D', 13).u64(ref).data;
}

std::vector<std::byte> replace(const std::uint64_t original,
                               const std::uint64_t replacement,
                               const std::uint32_t shares,
                               const std::uint32_t price) {
  return header('U', 13)
      .u64(original)
      .u64(replacement)
      .u32(shares)
      .u32(price)
      .data;
}

// Puts the 2-byte length prefix in front of a message, as in the file.
std::vector<std::byte> framed(const std::vector<std::byte>& message) {
  Bytes b;
  b.u16(static_cast<std::uint16_t>(message.size())).append(message);
  return b.data;
}

// Parses exactly one framed message and checks that the input then ends.
ITCHMessage parse_one(const std::vector<std::byte>& message) {
  const auto input = framed(message);
  ITCHParser parser{input};
  auto first = parser.next();
  EXPECT_TRUE(first.has_value());
  EXPECT_FALSE(parser.next().has_value());
  return first.value();
}

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
  EXPECT_EQ(m.decode_order_reference_number(), 0x0102030405060708u);
  EXPECT_EQ(m.decode_side(), Side::Buy);
  EXPECT_EQ(m.decode_shares(), 2000u);
  EXPECT_EQ(m.decode_price(), Price4{1'605'800});
  EXPECT_EQ(m.decode_stock_locate(), 496u);
}

TEST(ItchMessage, AddOrderSellSide) {
  const auto m =
      std::get<message::AddOrder>(parse_one(add_order(1, 'S', 100, 500)));
  EXPECT_EQ(m.decode_side(), Side::Sell);
}

TEST(ItchMessage, AddOrderWithMPIDFields) {
  const auto m = std::get<message::AddOrderWithMPID>(
      parse_one(add_order_mpid(42, "LEHM")));
  EXPECT_EQ(m.decode_order_reference_number(), 42u);
  EXPECT_EQ(m.decode_side(), Side::Sell);
  EXPECT_EQ(m.decode_shares(), 300u);
  EXPECT_EQ(m.decode_price(), Price4{1'605'800});
  EXPECT_EQ(m.decode_attribution(), "LEHM");
}

TEST(ItchMessage, ExecuteOrderFields) {
  const auto m =
      std::get<message::ExecuteOrder>(parse_one(executed(8881, 600)));
  EXPECT_EQ(m.decode_stock_locate(), 13u);
  EXPECT_EQ(m.decode_order_reference_number(), 8881u);
  EXPECT_EQ(m.decode_executed_shares(), 600u);
  EXPECT_EQ(m.decode_match_number(), 555u);
}

TEST(ItchMessage, ExecuteOrderWithPriceFields) {
  const auto m = std::get<message::ExecuteOrderWithPrice>(
      parse_one(executed_with_price(8881, 100, 'N', 504'600)));
  EXPECT_EQ(m.decode_order_reference_number(), 8881u);
  EXPECT_EQ(m.decode_executed_shares(), 100u);
  EXPECT_EQ(m.decode_match_number(), 556u);
  EXPECT_EQ(m.decode_printable(), Printable::No);
  EXPECT_EQ(m.decode_execution_price(), Price4{504'600});
}

TEST(ItchMessage, CancelOrderFields) {
  const auto m = std::get<message::CancelOrder>(parse_one(cancel(77, 25)));
  EXPECT_EQ(m.decode_stock_locate(), 13u);
  EXPECT_EQ(m.decode_order_reference_number(), 77u);
  EXPECT_EQ(m.decode_cancelled_shares(), 25u);
}

TEST(ItchMessage, DeleteOrderFields) {
  const auto m = std::get<message::DeleteOrder>(parse_one(delete_order(78)));
  EXPECT_EQ(m.decode_stock_locate(), 13u);
  EXPECT_EQ(m.decode_order_reference_number(), 78u);
}

TEST(ItchMessage, ReplaceOrderFields) {
  const auto m = std::get<message::ReplaceOrder>(
      parse_one(replace(100, 200, 50, 1'000'000)));
  EXPECT_EQ(m.decode_stock_locate(), 13u);
  EXPECT_EQ(m.decode_original_order_reference_number(), 100u);
  EXPECT_EQ(m.decode_new_order_reference_number(), 200u);
  EXPECT_EQ(m.decode_shares(), 50u);
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
  EXPECT_EQ(
      std::get<message::DeleteOrder>(*second).decode_order_reference_number(),
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
