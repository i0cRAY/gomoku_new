#include <gtest/gtest.h>

#include "core/core.h"

// T01：確認 gomoku_core 能被測試程式連結
TEST(BuildTest, T01_CoreLibraryLinks) {
    EXPECT_STREQ(kCoreLibraryName, "gomoku_core");
}
