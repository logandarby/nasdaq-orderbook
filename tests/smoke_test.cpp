// Placeholder so the test harness and CI run from the first commit.
// Real tests go next to it: parser tests against tests/data/fixture.itch,
// book invariants, and the differential test against the std::map book.

#include <gtest/gtest.h>

#include <filesystem>

TEST(Smoke, TestDataDirExists) {
  EXPECT_TRUE(std::filesystem::is_directory(ORDERBOOK_TEST_DATA_DIR));
}
