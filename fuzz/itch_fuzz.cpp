// libFuzzer target for the ITCH parser: arbitrary bytes in, and every message
// that comes out has all of its fields decoded.
//
// Allowed outcomes: messages, or an ITCHError (bad input, expected).
// Findings: a crash, an ASan/UBSan report, an EXPECT abort (EXPECT is forced
// on for this target), a hang, or any other exception escaping.
//
// Build and run (from the repo root):
//   cmake --preset clang-fuzz && cmake --build --preset clang-fuzz
//   build/clang-fuzz/itch_fuzz -max_total_time=60 -dict=fuzz/itch.dict \
//       fuzz/corpus
// Crashing inputs are saved as crash-<hash>; rerun one with
//   build/clang-fuzz/itch_fuzz crash-<hash>

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

#include "itch/itch_parser.h"

namespace {

// Calls every decode_* accessor the message type has, so each field's
// validation (side, printable, price limit) is exercised.
void decode_all_fields(const itch::ITCHMessage& message) {
  std::visit(
      [](const auto& m) {
        if constexpr (requires { m.decode_stock_locate(); })
          (void)m.decode_stock_locate();
        if constexpr (requires { m.decode_order_reference_number(); })
          (void)m.decode_order_reference_number();
        if constexpr (requires { m.decode_original_order_reference_number(); })
          (void)m.decode_original_order_reference_number();
        if constexpr (requires { m.decode_new_order_reference_number(); })
          (void)m.decode_new_order_reference_number();
        if constexpr (requires { m.decode_shares(); }) (void)m.decode_shares();
        if constexpr (requires { m.decode_executed_shares(); })
          (void)m.decode_executed_shares();
        if constexpr (requires { m.decode_cancelled_shares(); })
          (void)m.decode_cancelled_shares();
        if constexpr (requires { m.decode_match_number(); })
          (void)m.decode_match_number();
        if constexpr (requires { m.decode_attribution(); })
          (void)m.decode_attribution();
        // These can throw DecodeError; the rest of the message is still
        // worth decoding, so each gets its own try.
        if constexpr (requires { m.decode_side(); }) {
          try {
            (void)m.decode_side();
          } catch (const itch::DecodeError&) {
          }
        }
        if constexpr (requires { m.decode_price(); }) {
          try {
            (void)m.decode_price();
          } catch (const itch::DecodeError&) {
          }
        }
        if constexpr (requires { m.decode_execution_price(); }) {
          try {
            (void)m.decode_execution_price();
          } catch (const itch::DecodeError&) {
          }
        }
        if constexpr (requires { m.decode_printable(); }) {
          try {
            (void)m.decode_printable();
          } catch (const itch::DecodeError&) {
          }
        }
      },
      message);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
  const std::span<const std::byte> input{
      reinterpret_cast<const std::byte*>(data), size};
  itch::ITCHParser parser{input};
  try {
    while (const auto message = parser.next()) decode_all_fields(*message);
  } catch (const itch::ITCHError&) {
    // Bad input, correctly rejected.
  }
  return 0;
}
