#ifndef D4F9DCB6_26C2_4C9C_9DC4_7428EE3D6E51
#define D4F9DCB6_26C2_4C9C_9DC4_7428EE3D6E51

#include "market/types.h"

// Notes:
// Each message is of variable length depending on the message type
// Instruments are identified by a code, with intent for indexing into an array.
//  No guarantee it remains after the day is finished
// All integer fields are big endian (network byte order) and unsigned (unless
// otherwise noted) Alpha fields are ASCII, left justified, padded on the right
// w/ spaces. Prices are integral, with associated precision. Supplied in fixed
// point format.
//  Ex, Price(4) has a precision of 4. Max value of Price(4) is 200,000.0000
namespace itch {

// Base of every error caused by bad input bytes. Catch this (e.g. in the
// fuzzer) to accept all input-caused errors and nothing else.
class ITCHError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Thrown when a message's bytes don't match the spec (e.g. an unknown
// buy/sell indicator). On real data this means a decoder bug or a corrupt
// file, so it is fatal. The reader can catch it, add the file position, and
// rethrow.
class DecodeError : public ITCHError {
 public:
  DecodeError(char msg_type, std::string_view field_name,
              std::size_t field_offset, std::uint64_t bad_value)
      : ITCHError(std::format(
            "ITCH decode error: message '{}', field {} at offset {}: bad "
            "value {:#x}",
            msg_type, field_name, field_offset, bad_value)),
        message_type(msg_type),
        field(field_name),
        offset(field_offset),
        value(bad_value) {}

  char message_type;
  std::string_view field;  // points at a string literal
  std::size_t offset;      // within the message
  std::uint64_t value;     // the decoded field
};

namespace util {

// Encodes a a field in a byte array. Encodes the size to be decoded, and its
// offset
template <std::unsigned_integral T, std::size_t Offset>
struct Field {
  using type = T;
  static constexpr std::size_t offset = Offset;
  static constexpr std::size_t size = sizeof(T);
};

template <typename F>
concept FieldDescriptor = requires {
  typename F::type;
  { F::offset } -> std::convertible_to<size_t>;
};

template <std::size_t N>
std::array<std::byte, N> copy_in(std::span<const std::byte, N> src) {
  std::array<std::byte, N> out;
  std::ranges::copy(src, out.begin());
  return out;
}

template <typename T, std::size_t Offset, std::size_t N>
concept FieldInBounds =
    std::unsigned_integral<T> &&
    (sizeof(T) == 8 || sizeof(T) == 4 || sizeof(T) == 2 || sizeof(T) == 1) &&
    Offset + sizeof(T) <= N;

template <std::unsigned_integral T, std::size_t Offset, std::size_t N>
  requires FieldInBounds<T, Offset, N>
T read_be(std::span<const std::byte, N> bytes) {
  auto field = bytes.template subspan<Offset, sizeof(T)>();
  T v;
  std::memcpy(&v, field.data(), sizeof(T));
  if constexpr (sizeof(T) == 1)
    return v;
  else if constexpr (sizeof(T) == 2)
    return __builtin_bswap16(v);
  else if constexpr (sizeof(T) == 4)
    return __builtin_bswap32(v);
  else
    return __builtin_bswap64(v);
}

template <std::unsigned_integral T, std::size_t Offset, std::size_t N>
  requires FieldInBounds<T, Offset, N>
T read_be(const std::array<std::byte, N>& bytes) {
  return read_be<T, Offset>(std::span{bytes});
}

template <FieldDescriptor F, size_t N>
typename F::type read_be(const std::array<std::byte, N>& bytes) {
  return read_be<typename F::type, F::offset>(bytes);
}

constexpr std::optional<Side> alpha_to_side(const char c) {
  switch (c) {
    case 'B':
      return Side::Buy;
    case 'S':
      return Side::Sell;
    default:
      return std::nullopt;
  }
}
constexpr std::optional<Printable> alpha_to_printable(const char c) {
  switch (c) {
    case 'Y':
      return Printable::Yes;
    case 'N':
      return Printable::No;
    default:
      return std::nullopt;
  }
}
}  // namespace util

namespace message {

struct NotSupported {
  // A miscellaneous message that may be dropped, since we aren't supporting it
  char message_type;
};

namespace detail {

template <char Type, std::size_t Size>
class MessageBase {
 public:
  static constexpr std::size_t size = Size;
  static constexpr char type = Type;

 protected:
  using stock = util::Field<uint16_t, 1>;

 public:
  explicit MessageBase(std::span<const std::byte, size> src)
      : bytes{util::copy_in(src)} {
    EXPECT(src.size() == size, "Incorrect message size. Expected {}, got {}",
           size, src.size());
    EXPECT(src[0] == std::byte{type}, "Wrong message type {:#04x}",
           std::to_integer<int>(src[0]));
  }

  auto decode_stock_locate() const {
    return StockLocate{util::read_be<stock>(bytes)};
  }

 protected:
  std::array<std::byte, size> bytes;
};

// Shared layout of the two Add Order variants: 'A' (36 bytes) and 'F' (40
// bytes, the same fields plus a 4-byte MPID attribution at offset 36).
template <char Type, std::size_t Size>
class BasicAddOrder : public MessageBase<Type, Size> {
 private:
  using MessageBase<Type, Size>::bytes;
  using order_reference = util::Field<uint64_t, 11>;
  using shares = util::Field<uint32_t, 20>;
  using price = util::Field<uint32_t, 32>;
  using side = util::Field<uint8_t, 19>;
  // Alpha, not an integer, so it stays a plain offset and size.
  static constexpr std::size_t attribution_offset = 36;  // 'F' only
  static constexpr std::size_t attribution_size = 4;

 public:
  /**
   * An Add Order Message indicates that a new order has been accepted by the
   * Nasdaq system and was added to the displayable book. The message includes a
   * day-unique Order Reference Number used by Nasdaq to track the order.
   * Nasdaq will support two variations of the Add Order message format.
   */
  explicit BasicAddOrder(std::span<const std::byte, Size> src)
      : MessageBase<Type, Size>(src) {}

  auto decode_order_reference_number() const {
    return OrderRef{util::read_be<order_reference>(bytes)};
  }

  auto decode_shares() const { return Shares{util::read_be<shares>(bytes)}; }

  Price4 decode_price() const {
    const auto ticks = util::read_be<price>(bytes);
    try {
      return Price4{ticks};
    } catch (const std::out_of_range&) {
      // Rethrow with the message context Price4 doesn't have.
      throw DecodeError(Type, "price", price::offset, ticks);
    }
  }

  Side decode_side() const {
    const auto raw = util::read_be<side>(bytes);
    if (const auto decoded = util::alpha_to_side(static_cast<char>(raw)))
        [[likely]]
      return *decoded;
    throw DecodeError(Type, "side", side::offset, raw);
  }

  // The market participant's 4-character ID. Points into this message's bytes,
  // so it's only valid while the message exists.
  std::string_view decode_attribution() const
    requires(Type == 'F')
  {
    static_assert(attribution_offset + attribution_size <= Size);
    return {reinterpret_cast<const char*>(bytes.data()) + attribution_offset,
            attribution_size};
  }
};

// Shared layout of the two Order Executed variants: 'E' (31 bytes) and 'C'
// (36 bytes, the same fields plus a printable flag and the execution price).
template <char Type, std::size_t Size>
class BasicExecuteOrder : public MessageBase<Type, Size> {
 private:
  using MessageBase<Type, Size>::bytes;
  using order_reference = util::Field<uint64_t, 11>;
  using executed_shares = util::Field<uint32_t, 19>;
  using match_number = util::Field<uint64_t, 23>;
  using printable = util::Field<uint8_t, 31>;         // 'C' only
  using execution_price = util::Field<uint32_t, 32>;  // 'C' only

 public:
  explicit BasicExecuteOrder(std::span<const std::byte, Size> src)
      : MessageBase<Type, Size>(src) {}

  auto decode_order_reference_number() const {
    return OrderRef{util::read_be<order_reference>(bytes)};
  }

  auto decode_executed_shares() const {
    return Shares{util::read_be<executed_shares>(bytes)};
  }

  auto decode_match_number() const {
    return MatchNumber{util::read_be<match_number>(bytes)};
  }

  Price4 decode_execution_price() const
    requires(Type == 'C')
  {
    const auto ticks = util::read_be<execution_price>(bytes);
    try {
      return Price4{ticks};
    } catch (const std::out_of_range&) {
      // Rethrow with the message context Price4 doesn't have.
      throw DecodeError(Type, "execution price", execution_price::offset,
                        ticks);
    }
  }

  Printable decode_printable() const
    requires(Type == 'C')
  {
    const auto raw = util::read_be<printable>(bytes);
    if (const auto decoded = util::alpha_to_printable(static_cast<char>(raw)))
        [[likely]]
      return *decoded;
    throw DecodeError(Type, "printable", printable::offset, raw);
  }
};

}  // namespace detail

using AddOrder = detail::BasicAddOrder<'A', 36>;
using AddOrderWithMPID = detail::BasicAddOrder<'F', 40>;

/**
 * This message is sent whenever an order on the book is executed in whole or in
 * part. It is possible to receive several Order Executed Messages for the same
 * order reference number if that order is executed in several parts. The
 * multiple Order Executed Messages on the same order are cumulative.
 *
 * By combining the executions from both types of Order Executed Messages and
 * the Trade Message, it is possible to build a complete view of all non-cross
 * executions that happen on Nasdaq. Cross execution information is available
 * in one bulk print per symbol via the Cross Trade Message.
 */
using ExecuteOrder = detail::BasicExecuteOrder<'E', 31>;
/**
 * This message is sent whenever an order on the book is executed in whole or in
 * part at a price different from the initial display price. Since the
 * execution price is different than the display price of the original Add
 * Order, Nasdaq includes a price field within this execution message.
 *
 * It is possible to receive multiple Order Executed and Order Executed With
 * Price messages for the same order if that order is executed in several parts.
 * The multiple Order Executed messages on the same order are cumulative.
 *
 * These executions may be marked as non-printable. If the execution is marked
 * as non-printed, it means that the shares will be included into a later bulk
 * print (e.g., in the case of cross executions). If a firm is looking to use
 * the data in time-and-sales displays or volume calculations, Nasdaq recommends
 * that firms ignore messages marked as non-printable to prevent double
 * counting.
 */
using ExecuteOrderWithPrice = detail::BasicExecuteOrder<'C', 36>;

class CancelOrder : public detail::MessageBase<'X', 23> {
 private:
  using order_reference = util::Field<uint64_t, 11>;
  using cancelled_shares = util::Field<uint32_t, 19>;

 public:
  /**
   * This message is sent whenever an order on the book is modified as a result
   * of a partial cancellation.
   */
  explicit CancelOrder(std::span<const std::byte, size> src)
      : MessageBase(src) {}

  auto decode_order_reference_number() const {
    return OrderRef{util::read_be<order_reference>(bytes)};
  }

  auto decode_cancelled_shares() const {
    return Shares{util::read_be<cancelled_shares>(bytes)};
  }
};

// Order Delete ('D'): remove the whole order from the book.
class DeleteOrder : public detail::MessageBase<'D', 19> {
 private:
  using order_reference = util::Field<uint64_t, 11>;

 public:
  /**
   * This message is sent whenever an order on the book is being cancelled. All
   * remaining shares are no longer accessible so the order must be removed
   * from the book.
   */
  explicit DeleteOrder(std::span<const std::byte, size> src)
      : MessageBase(src) {}

  auto decode_order_reference_number() const {
    return OrderRef{util::read_be<order_reference>(bytes)};
  }
};

// Order Replace ('U'): remove the original order and add a new one with a new
// reference number, shares and price. The side and stock aren't in the
// message: they stay those of the original order, so the book looks them up.
class ReplaceOrder : public detail::MessageBase<'U', 35> {
 private:
  using original_order_reference = util::Field<uint64_t, 11>;
  using new_order_reference = util::Field<uint64_t, 19>;
  using shares = util::Field<uint32_t, 27>;
  using price = util::Field<uint32_t, 31>;

 public:
  /**
   * This message is sent whenever an order on the book has been
   * cancel-replaced. All remaining shares from the original order are no longer
   * accessible, and must be removed. The new order details are provided for
   * the replacement, along with a new order reference number which will be
   * used henceforth. Since the side, stock symbol and attribution (if any)
   * cannot be changed by an Order Replace event, these fields are not included
   * in the message. Firms should retain the side, stock symbol and MPID from
   * the original Add Order message.
   */
  explicit ReplaceOrder(std::span<const std::byte, size> src)
      : MessageBase(src) {}

  auto decode_original_order_reference_number() const {
    return OrderRef{util::read_be<original_order_reference>(bytes)};
  }

  auto decode_new_order_reference_number() const {
    return OrderRef{util::read_be<new_order_reference>(bytes)};
  }

  auto decode_shares() const { return Shares{util::read_be<shares>(bytes)}; }

  Price4 decode_price() const {
    const auto ticks = util::read_be<price>(bytes);
    try {
      return Price4{ticks};
    } catch (const std::out_of_range&) {
      // Rethrow with the message context Price4 doesn't have.
      throw DecodeError(type, "price", price::offset, ticks);
    }
  }
};

}  // namespace message

using ITCHMessage =
    std::variant<message::AddOrder, message::AddOrderWithMPID,
                 message::ExecuteOrder, message::ExecuteOrderWithPrice,
                 message::CancelOrder, message::DeleteOrder,
                 message::ReplaceOrder, message::NotSupported>;

}  // namespace itch

#endif /* D4F9DCB6_26C2_4C9C_9DC4_7428EE3D6E51 */
