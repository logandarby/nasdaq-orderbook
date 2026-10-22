// Replays one ITCH 5.0 trading day into the per-stock books and reports
// messages per second. The parser and books are not written yet (stage 1).

#include <cstdio>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <file.NASDAQ_ITCH50>\n", argv[0]);
    return 2;
  }
  std::fprintf(stderr, "replay: not implemented yet\n");
  return 1;
}
