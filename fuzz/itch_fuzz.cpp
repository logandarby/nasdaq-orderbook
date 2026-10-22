// libFuzzer target for the ITCH parser. Feed `data` to the parser once it
// exists (stage 1). Run: build/clang-fuzz/itch_fuzz -max_total_time=60

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
  (void)data;
  (void)size;
  return 0;
}
