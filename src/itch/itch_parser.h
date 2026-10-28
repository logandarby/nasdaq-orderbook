#ifndef ABCE1F67_6776_4289_876F_89246A52340C
#define ABCE1F67_6776_4289_876F_89246A52340C

#include "itch_message.h"

namespace itch {

namespace util {

template <typename M>
concept WireMessage = requires {
  { M::type } -> std::convertible_to<char>;
  { M::size } -> std::convertible_to<size_t>;
};

}  // namespace util

// Thrown when the framing around messages is wrong: a length prefix or a
// message cut off by the end of the input, or a length that doesn't match the
// message type's size.
class FramingError : public ITCHError {
 public:
  FramingError(std::string_view what_failed, std::size_t at_position,
               std::size_t expected_bytes, std::size_t actual_bytes)
      : ITCHError(std::format(
            "ITCH framing error at byte {}: {} (expected {} bytes, got {})",
            at_position, what_failed, expected_bytes, actual_bytes)),
        problem(what_failed),
        position(at_position),
        expected(expected_bytes),
        actual(actual_bytes) {}

  // For problems that aren't a byte count, e.g. a length prefix of 0.
  // expected and actual are 0.
  FramingError(std::string_view what_failed, std::size_t at_position)
      : ITCHError(std::format("ITCH framing error at byte {}: {}", at_position,
                              what_failed)),
        problem(what_failed),
        position(at_position),
        expected(0),
        actual(0) {}

  std::string_view problem;  // points at a string literal
  std::size_t position;      // byte offset in the input
  std::size_t expected;
  std::size_t actual;
};

class ITCHParser {
 public:
  static constexpr size_t message_length_bytes = 2;

  ITCHParser(std::span<const std::byte> src) : bytes{src}, ptr{0}, count{0} {}

  /**
   * Parses the next message in the byte span. Returns optional::nullopt if the
   * end of the span is reached. Throws DecoderError if a problem is
   * encountered.
   */
  std::optional<ITCHMessage> next() {
    if (ptr == bytes.size()) return std::nullopt;
    const auto length = util::read_be<uint16_t, 0>(
        take(message_length_bytes, "take length bytes"));
    if (!length)
      throw FramingError(
          "invalid length header of 0 bytes has been encountered", ptr - 2);
    const auto msg = take(length, "take message bytes off length");
    const auto decoded = decode(msg);
    count++;
    return decoded;
  }

 private:
  // Decodes a subspan based on its first byte
  // Length of msg must match expected length of itch message
  ITCHMessage decode(std::span<const std::byte> msg) {
    EXPECT(msg.size() > 0, "0 length message");
    const char type = std::to_integer<char>(msg[0]);
    switch (type) {
      case message::AddOrder::type:
        return decode_as<message::AddOrder>(msg);
      case message::AddOrderWithMPID::type:
        return decode_as<message::AddOrderWithMPID>(msg);
      case message::ExecuteOrder::type:
        return decode_as<message::ExecuteOrder>(msg);
      case message::ExecuteOrderWithPrice::type:
        return decode_as<message::ExecuteOrderWithPrice>(msg);
      case message::CancelOrder::type:
        return decode_as<message::CancelOrder>(msg);
      case message::DeleteOrder::type:
        return decode_as<message::DeleteOrder>(msg);
      case message::ReplaceOrder::type:
        return decode_as<message::ReplaceOrder>(msg);
      default:
        return message::NotSupported{type};
    }
  }

  // Decodes a message into the appropriate itch message
  // The size of msg must match the expected size of M
  template <util::WireMessage M>
  M decode_as(std::span<const std::byte> msg) {
    if (msg.size() != M::size)
      throw FramingError(
          "Size of byte span not equal to size of itch::message type", ptr,
          M::size, msg.size());
    return M(msg.first<M::size>());  // Safe because of earlier bounds check
  }

  // Returns a subspan of the next n bytes. Also has an error message we can use
  std::span<const std::byte> take(
      const std::size_t n, std::string_view operation = "take operation") {
    if (bytes.size() - ptr < n) [[unlikely]] {
      throw FramingError(operation, ptr, n, bytes.size() - ptr);
    }
    const auto out = bytes.subspan(ptr, n);
    ptr += n;
    return out;
  }

  std::span<const std::byte> bytes;
  std::size_t ptr;
  std::size_t count;
};

}  // namespace itch

#endif /* ABCE1F67_6776_4289_876F_89246A52340C */
