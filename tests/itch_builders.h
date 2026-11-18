#ifndef D4A1F6C2_7E3B_4C59_A0D8_5B2E9F1C3A76
#define D4A1F6C2_7E3B_4C59_A0D8_5B2E9F1C3A76

// Test-only builders for ITCH 5.0 messages. Messages are built byte by byte
// from the spec's field tables, independently of the decoder's offsets, so a
// wrong offset in itch_message.h shows up as a wrong field in the tests.
// Shared by the parser tests and the applyMessageToBook() tests.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "itch/itch_parser.h"

namespace itch_test {

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

inline constexpr std::uint16_t kDefaultLocate = 13;

// Header shared by every message: type, stock locate, tracking number,
// timestamp.
inline Bytes header(const char type, const std::uint16_t locate) {
  Bytes b;
  b.ch(type)
      .u16(locate)
      .u16(/*tracking*/ 7)
      .u48(/*timestamp*/ 34'200'000'000'000);
  return b;
}

inline std::vector<std::byte> add_order(
    const std::uint64_t ref, const char side, const std::uint32_t shares,
    const std::uint32_t price, const std::uint16_t locate = kDefaultLocate) {
  return header('A', locate)
      .u64(ref)
      .ch(side)
      .u32(shares)
      .alpha("AAPL", 8)
      .u32(price)
      .data;
}

inline std::vector<std::byte> add_order_mpid(
    const std::uint64_t ref, const std::string_view mpid, const char side = 'S',
    const std::uint32_t shares = 300, const std::uint32_t price = 1'605'800,
    const std::uint16_t locate = kDefaultLocate) {
  return header('F', locate)
      .u64(ref)
      .ch(side)
      .u32(shares)
      .alpha("AAPL", 8)
      .u32(price)
      .alpha(mpid, 4)
      .data;
}

inline std::vector<std::byte> executed(
    const std::uint64_t ref, const std::uint32_t shares,
    const std::uint16_t locate = kDefaultLocate) {
  return header('E', locate).u64(ref).u32(shares).u64(/*match*/ 555).data;
}

inline std::vector<std::byte> executed_with_price(
    const std::uint64_t ref, const std::uint32_t shares, const char printable,
    const std::uint32_t price, const std::uint16_t locate = kDefaultLocate) {
  return header('C', locate)
      .u64(ref)
      .u32(shares)
      .u64(/*match*/ 556)
      .ch(printable)
      .u32(price)
      .data;
}

inline std::vector<std::byte> cancel(
    const std::uint64_t ref, const std::uint32_t shares,
    const std::uint16_t locate = kDefaultLocate) {
  return header('X', locate).u64(ref).u32(shares).data;
}

inline std::vector<std::byte> delete_order(
    const std::uint64_t ref, const std::uint16_t locate = kDefaultLocate) {
  return header('D', locate).u64(ref).data;
}

inline std::vector<std::byte> replace(
    const std::uint64_t original, const std::uint64_t replacement,
    const std::uint32_t shares, const std::uint32_t price,
    const std::uint16_t locate = kDefaultLocate) {
  return header('U', locate)
      .u64(original)
      .u64(replacement)
      .u32(shares)
      .u32(price)
      .data;
}

// 'S' (System Event, 12 bytes): not a book message, parses as NotSupported.
inline std::vector<std::byte> system_event() {
  Bytes b;
  b.ch('S').u16(0).u16(0).u48(0).ch('O');
  return b.data;
}

// Puts the 2-byte length prefix in front of a message, as in the file.
inline std::vector<std::byte> framed(const std::vector<std::byte>& message) {
  Bytes b;
  b.u16(static_cast<std::uint16_t>(message.size())).append(message);
  return b.data;
}

// Parses exactly one framed message and checks that the input then ends.
inline itch::ITCHMessage parse_one(const std::vector<std::byte>& message) {
  const auto input = framed(message);
  itch::ITCHParser parser{input};
  auto first = parser.next();
  EXPECT_TRUE(first.has_value());
  EXPECT_FALSE(parser.next().has_value());
  return first.value();
}

}  // namespace itch_test

#endif /* D4A1F6C2_7E3B_4C59_A0D8_5B2E9F1C3A76 */
