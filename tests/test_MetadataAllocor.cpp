#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <unordered_set>
#include <mutex>
#include <algorithm>

// 包含我们要测试的目标
#include "gc_malloc/MetadataAllocor.hpp"
// 包含 PageGroup 以便获取其大小
#include "gc_malloc/PageGroup.hpp"


// 定义一个测试夹具(Test Fixture)类，以便在测试用例之间共享设置
class MetadataAllocTest : public ::testing::Test {
protected:
    // SetUp() 会在每个 TEST_F 测试用例运行前被调用
    void SetUp() override {
        // 在这里可以进行一些每个测试都需要的前置准备工作
        // 对于 MetadataAllocator 这个单例，通常不需要特别的 SetUp
    }

    // TearDown() 会在每个 TEST_F 测试用例运行后被调用
    void TearDown() override {
        // 在这里可以进行一些清理工作
    }

    // 获取对单例的引用，方便书写
    MetadataAllocator& alloc_ = MetadataAllocator::GetInstance();
    const size_t kBlockSize = sizeof(PageGroup);
};


// =====================================================================
// 测试用例 1: 单线程下的基本功能验证
// 需求: 验证分配器在单线程环境下能否正确地分配、回收和重用内存。
// =====================================================================
TEST_F(MetadataAllocTest, SingleThreadedFunctionality) {
    // 需求1: 分配器能够成功分配出一个内存块。
    void* block1 = alloc_.allocate(kBlockSize);
    // 验证1: 返回的指针不应为空。
    ASSERT_NE(block1, nullptr) << "First allocation failed.";

    // 需求2: 分配器能够成功分配出第二个内存块。
    void* block2 = alloc_.allocate(kBlockSize);
    // 验证2: 第二个指针也不为空，且地址与第一个不同。
    ASSERT_NE(block2, nullptr) << "Second allocation failed.";
    ASSERT_NE(block1, block2) << "Consecutive allocations returned the same address.";

    // 需求3: 释放一个内存块后，该内存块应该能被重用。
    alloc_.deallocate(block1, kBlockSize);
    // 此时，block1 应该在 free_list 的头部。

    // 需求4: 再次分配时，应该优先返回刚刚被释放的那个块。
    void* block3 = alloc_.allocate(kBlockSize);
    // 验证4: block3 的地址应该与被释放的 block1 完全相同。
    // 这是对空闲链表 (free list) 机制最直接的验证。
    ASSERT_EQ(block3, block1) << "Allocator did not reuse the deallocated block.";

    // 清理剩下的内存块，保持环境干净
    alloc_.deallocate(block2, kBlockSize);
    alloc_.deallocate(block3, kBlockSize);
}


// =====================================================================
// 测试用例 2: 多线程下的并发安全验证
// 需求: 验证分配器在多个线程同时调用 allocate 和 deallocate 时，
//      能否保证线程安全，不会出现数据竞争、死锁或内存损坏。
// =====================================================================
TEST_F(MetadataAllocTest, MultiThreadedSafety) {
    const int kNumThreads = 8;
    const int kAllocationsPerThread = 10000;
    const size_t total_allocations = kNumThreads * kAllocationsPerThread;

    // 创建一个二维 vector，每个线程有一个自己的 vector 来存储分配到的指针
    std::vector<std::vector<void*>> pointers_per_thread(kNumThreads);
    for(auto& vec : pointers_per_thread) {
        vec.reserve(kAllocationsPerThread);
    }

    std::vector<std::thread> threads;

    // ================== 分配阶段 ==================
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([this, i, kAllocationsPerThread, &pointers_per_thread]() {
            for (int j = 0; j < kAllocationsPerThread; ++j) {
                void* p = alloc_.allocate(kBlockSize);
                ASSERT_NE(p, nullptr);
                pointers_per_thread[i].push_back(p);
            }
        });
    }
    // 等待所有分配线程结束
    for (auto& t : threads) {
        t.join();
    }

    // ================== 验证阶段 ==================
    // 现在，所有分配都已完成，我们可以安全地检查是否有重复
    std::unordered_set<void*> unique_pointers;
    for (const auto& vec : pointers_per_thread) {
        for (void* ptr : vec) {
            // set.insert() 返回一个 pair，其 .second 成员在插入成功时为 true
            // 如果插入失败（因为元素已存在），则为 false。
            ASSERT_TRUE(unique_pointers.insert(ptr).second) 
                << "Duplicate pointer " << ptr << " allocated. Race condition detected.";
        }
    }
    // 同时，检查总数是否正确
    ASSERT_EQ(unique_pointers.size(), total_allocations);


    // ================== 释放阶段 ==================
    threads.clear();
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([this, i, &pointers_per_thread]() {
            for (void* p : pointers_per_thread[i]) {
                alloc_.deallocate(p, kBlockSize);
            }
        });
    }
    // 等待所有释放线程结束
    for (auto& t : threads) {
        t.join();
    }
}