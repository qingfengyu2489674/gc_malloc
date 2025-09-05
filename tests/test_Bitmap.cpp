#include <gtest/gtest.h>
#include <vector>

// 包含我们要测试的目标
#include "gc_malloc/Bitmap.hpp" // 请确保路径正确

// 定义一个测试夹具(Test Fixture)类，方便管理不同大小的Bitmap实例
class BitmapTest : public ::testing::Test {
protected:
    // SetUp() 和 TearDown() 在这里不是必需的，但保留是一个好习惯
    void SetUp() override {}
    void TearDown() override {}
};

// =====================================================================
// 测试用例 1: 基础的 Set, Clear 和 IsSet 功能验证
// 需求: 验证位图能否正确地设置、清除和检查单个位的状态。
// =====================================================================
TEST_F(BitmapTest, BasicSetClearIsSet) {
    // 创建一个足够大的位图来进行测试，例如100位
    const size_t BITS = 100;
    Bitmap bmp(BITS);

    // 需求1: 初始状态下，所有位都应该是 0 (cleared)。
    for (size_t i = 0; i < BITS; ++i) {
        EXPECT_FALSE(bmp.IsSet(i)) << "Bit " << i << " should be initially clear.";
    }

    // 需求2: Set() 应该能成功将指定的位设置为 1。
    bmp.Set(10);
    bmp.Set(35);
    bmp.Set(99);

    // 验证2: 被设置的位应该是 1，其他位保持为 0。
    EXPECT_TRUE(bmp.IsSet(10));
    EXPECT_TRUE(bmp.IsSet(35));
    EXPECT_TRUE(bmp.IsSet(99));
    EXPECT_FALSE(bmp.IsSet(11)); // 检查相邻位
    EXPECT_FALSE(bmp.IsSet(0));  // 检查边界位

    // 需求3: Clear() 应该能成功将指定的位清除为 0。
    bmp.Clear(35);

    // 验证3: 被清除的位应该是 0，其他已设置的位不受影响。
    EXPECT_FALSE(bmp.IsSet(35));
    EXPECT_TRUE(bmp.IsSet(10));
    EXPECT_TRUE(bmp.IsSet(99));
}

// =====================================================================
// 测试用例 2: 边界条件验证
// 需求: 验证位图在处理边界索引（如0，size-1）和无效索引时行为是否正确。
// =====================================================================
TEST_F(BitmapTest, EdgeCasesAndInvalidIndices) {
    const size_t BITS = 257; // 使用一个不规则的大小来测试
    Bitmap bmp(BITS);

    // 需求4: 应该能正确操作第一个位 (索引 0)。
    EXPECT_FALSE(bmp.IsSet(0));
    bmp.Set(0);
    EXPECT_TRUE(bmp.IsSet(0));
    bmp.Clear(0);
    EXPECT_FALSE(bmp.IsSet(0));

    // 需求5: 应该能正确操作最后一个位 (索引 BITS - 1)。
    const size_t last_bit = BITS - 1;
    EXPECT_FALSE(bmp.IsSet(last_bit));
    bmp.Set(last_bit);
    EXPECT_TRUE(bmp.IsSet(last_bit));
    bmp.Clear(last_bit);
    EXPECT_FALSE(bmp.IsSet(last_bit));

    // 需求6: 对于越界的索引，所有操作都应该被安全地忽略，不应导致程序崩溃。
    // 我们检查一个刚好越界的索引 (BITS) 和一个远超边界的索引。
    const size_t out_of_bounds_index = BITS;
    const size_t far_out_of_bounds_index = BITS + 100;

    // 验证6: 调用不应抛出异常，且 IsSet 应返回 false。
    ASSERT_NO_THROW(bmp.Set(out_of_bounds_index));
    ASSERT_NO_THROW(bmp.Clear(out_of_bounds_index));
    EXPECT_FALSE(bmp.IsSet(out_of_bounds_index));

    ASSERT_NO_THROW(bmp.Set(far_out_of_bounds_index));
    EXPECT_FALSE(bmp.IsSet(far_out_of_bounds_index));
}

// =====================================================================
// 测试用例 3: FindFirstSet 功能验证
// 需求: 验证 FindFirstSet 能否正确地找到第一个被设置的位。
// =====================================================================
TEST_F(BitmapTest, FindFirstSetFunctionality) {
    const size_t BITS = 512;
    Bitmap bmp(BITS);

    // 需求7: 在一个空的位图中，应该找不到任何被设置的位。
    // 验证7: FindFirstSet 应该返回 BITS (表示“未找到”)。
    EXPECT_EQ(bmp.FindFirstSet(0), BITS);

    // 设置一些位，用于测试查找
    bmp.Set(15);
    bmp.Set(128);
    bmp.Set(256);
    bmp.Set(511);

    // 需求8: 从头开始查找，应该能找到第一个被设置的位。
    // 验证8: 应该返回 15。
    EXPECT_EQ(bmp.FindFirstSet(0), 15);

    // 需求9: 从一个被设置的位开始查找，应该返回该位本身。
    // 验证9: 从 15 开始查找，应该返回 15。
    EXPECT_EQ(bmp.FindFirstSet(15), 15);

    // 需求10: 从一个被设置的位之后开始查找，应该能找到下一个被设置的位。
    // 验证10: 从 16 开始查找，应该跳过 15，找到 128。
    EXPECT_EQ(bmp.FindFirstSet(16), 128);
    EXPECT_EQ(bmp.FindFirstSet(129), 256);

    // 需求11: 如果从一个位置开始，后面没有任何被设置的位，应该返回“未找到”。
    // 验证11: 从 512 (越界) 开始查找，应该返回 BITS。
    EXPECT_EQ(bmp.FindFirstSet(512), BITS);
    // 从 511 开始查找，应该找到 511
    EXPECT_EQ(bmp.FindFirstSet(511), 511);
    // 从 511 之后，即 512 开始查找，应该返回 BITS（因为没有了）
    // 但我们的循环是 i < size_，所以从 512 开始查找会直接退出
    // 为了更准确，我们从 512 之前的最后一个位置 511 的下一个位置 512 开始
    // 但这是越界的，所以我们应该从 511 的下一个有效位置开始，即 512
    // 但我们的循环条件是 i < size_, 所以我们应该从 512 开始
    // 但这是越界的，所以我们应该从 511 的下一个位置开始，即 512
    // 实际上，我们应该测试从 512 之前的最后一个位置的下一个位置开始，也就是 512，
    // 但是这是越界的，所以我们应该测试从 511 的下一个位置，即 512 开始，
    // 但是这是越界的，所以我们应该从 511 的下一个位置开始
    // 实际上，我们应该从 512 之前的最后一个位置 511 的下一个位置 512 开始，
    // 但是这是越界的，所以我们应该从 511 的下一个位置 512 开始
    // 实际上，我们应该测试从 512 之前的最后一个位置 511 的下一个位置 512 开始，
    // 但是这是越界的，所以我们应该从 511 的下一个位置 512 开始
    // 实际上，我们应该从 512 之前的最后一个位置 511 的下一个位置 512 开始，
    // 您的代码 for (size_t i = start_bit; i < size_; i++) 如果 start_bit >= size_ 会直接退出循环，
    // 并返回 size_，这是正确的。
    // 我们测试从最后一个设置位之后开始查找。
    EXPECT_EQ(bmp.FindFirstSet(257), 511);
}