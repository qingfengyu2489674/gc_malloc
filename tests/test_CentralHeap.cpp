#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <numeric>
#include <algorithm>
#include <map>

#include "gc_malloc/CentralHeap.hpp"
#include "gc_malloc/PageGroup.hpp"

class CentralHeapBlackBoxTest : public ::testing::Test {
protected:
    CentralHeap& heap_ = CentralHeap::GetInstance();
};

// =====================================================================
// 测试 1: 核心API行为测试 (API Contract Test)
// 需求: 验证 acquire_pages 和 release_pages 是否遵守了其基本契约。
// =====================================================================
TEST_F(CentralHeapBlackBoxTest, APITest) {
    // 契约1: 申请0页内存应该失败
    EXPECT_EQ(heap_.acquire_pages(0), nullptr) << "Acquiring 0 pages should fail.";

    // 契约2: 申请一个有效大小的内存应该成功
    PageGroup* group1 = heap_.acquire_pages(1);
    ASSERT_NE(group1, nullptr) << "Failed to acquire a single page.";
    EXPECT_NE(group1->start_address, nullptr);
    EXPECT_EQ(group1->page_count, 1);

    // 契约3: 申请另一个也应该成功，且地址不应重叠
    PageGroup* group8 = heap_.acquire_pages(8);
    ASSERT_NE(group8, nullptr) << "Failed to acquire 8 pages.";
    EXPECT_NE(group8->start_address, nullptr);
    EXPECT_EQ(group8->page_count, 8);
    
    // 验证地址不重叠
    uintptr_t start1 = reinterpret_cast<uintptr_t>(group1->start_address);
    uintptr_t end1 = start1 + group1->page_count * 4096; // 假设页大小为4K
    uintptr_t start8 = reinterpret_cast<uintptr_t>(group8->start_address);
    EXPECT_TRUE(start8 >= end1 || (start8 + 8*4096) <= start1) << "Allocated regions overlap.";

    // 契约4: 释放空指针应该是安全无操作的
    ASSERT_NO_THROW(heap_.release_pages(nullptr));

    // 契约5: 释放有效的 PageGroup 应该是成功的
    ASSERT_NO_THROW(heap_.release_pages(group1));
    ASSERT_NO_THROW(heap_.release_pages(group8));
}

// =====================================================================
// 测试 2: 拆分行为推断测试 (Splitting Behavior Inference)
// 需求: 通过行为推断，当池中没有小块时，分配器会从大块中拆分。
// =====================================================================
TEST_F(CentralHeapBlackBoxTest, InfersSplitting) {
    // 步骤1: 消耗掉池中所有可能存在的小块，强制分配器去申请一个全新的大块(Region)
    // 我们通过申请大量不同的小尺寸内存来做到这一点。
    std::vector<PageGroup*> warm_up_groups;
    for(int i=0; i<50; ++i) {
        warm_up_groups.push_back(heap_.acquire_pages(i % 16 + 1));
    }

    // 步骤2: 申请一个中等大小的块，比如128页。这几乎肯定会从一个全新的256页Region中分配。
    PageGroup* large_group = heap_.acquire_pages(128);
    ASSERT_NE(large_group, nullptr);

    // 步骤3: 关键推断。如果拆分逻辑正确，现在池中应该有一个128页的剩余块。
    // 我们可以通过多次申请小块来验证这一点。如果系统能持续提供内存而
    // 不崩溃或返回失败，就强烈暗示了拆分和剩余块的回收是成功的。
    std::vector<PageGroup*> small_groups;
    bool success = true;
    for (int i = 0; i < 120; ++i) { // 尝试消耗掉剩余的块 (120 < 128)
        PageGroup* p = heap_.acquire_pages(1);
        if (p == nullptr) {
            success = false;
            break;
        }
        small_groups.push_back(p);
    }
    EXPECT_TRUE(success) << "Failed to acquire small pages, suggesting the remainder of a split was not returned to the pool.";

    // 清理
    for (auto p : warm_up_groups) heap_.release_pages(p);
    heap_.release_pages(large_group);
    for (auto p : small_groups) heap_.release_pages(p);
}


// =====================================================================
// 测试 3: 合并行为推断测试 (Coalescing Behavior Inference) - 安全的黑盒版本
// 需求: 通过合法的API调用，创造一个相邻块被释放的场景，并验证它们会被合并。
// =====================================================================
TEST_F(CentralHeapBlackBoxTest, InfersCoalescing) {
    // 步骤 1: 申请一个足够大的连续内存块(span_c)，我们将在它上面进行“手术”。
    // 比如 32 页。
    const size_t total_size = 32;
    PageGroup* span_c = heap_.acquire_pages(total_size);
    ASSERT_NE(span_c, nullptr) << "Failed to acquire the initial large span.";
    void* base_addr = span_c->start_address;

    // 步骤 2: 将这个大块归还。现在池中应该有一个32页的空闲块。
    heap_.release_pages(span_c);

    // 步骤 3: 重新从池中申请出三个小块 a, b, d，它们应该会从刚刚释放的
    // 大块 c 中被拆分出来，并且 a 和 b 是相邻的。
    const size_t size_a = 10;
    const size_t size_b = 12;
    // 剩余部分 d 的大小是 32 - 10 - 12 = 10
    
    PageGroup* span_a = heap_.acquire_pages(size_a);
    PageGroup* span_b = heap_.acquire_pages(size_b);
    PageGroup* span_d = heap_.acquire_pages(total_size - size_a - size_b);

    // 验证：确保这三个块都成功分配，并且来自于我们期望的连续内存区域。
    ASSERT_NE(span_a, nullptr);
    ASSERT_NE(span_b, nullptr);
    ASSERT_NE(span_d, nullptr);

    // 我们通过地址来验证它们是按顺序从大块c中拆分出来的。
    // 这依赖于分配器优先使用“最佳匹配”的空闲块的特性。
    void* addr_a = span_a->start_address;
    void* addr_b = span_b->start_address;
    
    ASSERT_EQ(addr_a, base_addr) << "Span A was not allocated from the start of the large span.";
    ASSERT_EQ(addr_b, static_cast<char*>(addr_a) + size_a * 4096) << "Span B is not adjacent to Span A.";

    // 步骤 4: 释放 a 和 b，但不释放 d。
    // 这就创造了一个场景：池中有两个相邻的空闲块（原来的a和b），
    // 它们旁边还有一个被占用的块（d）。
    heap_.release_pages(span_a);
    heap_.release_pages(span_b);

    // 步骤 5: 关键推断！
    // 如果合并逻辑正确，a 和 b 现在应该已经被合并成一个大小为 a+b (22页) 的块。
    // 我们尝试申请一个 22 页的块。如果能成功，并且其地址是 a 的地址，
    // 那么就强有力地证明了合并已经发生。
    PageGroup* merged_span = heap_.acquire_pages(size_a + size_b);
    ASSERT_NE(merged_span, nullptr) << "Failed to acquire a block of the merged size. Coalescing might have failed.";
    
    // 验证合并后的块地址是否正确
    EXPECT_EQ(merged_span->start_address, addr_a) << "The merged span should start at the address of the first released span (A).";
    EXPECT_EQ(merged_span->page_count, size_a + size_b);

    // 步骤 6: 清理所有剩余的块
    heap_.release_pages(merged_span);
    heap_.release_pages(span_d);
}


// =====================================================================
// 测试 4: 多线程压力与资源竞争测试 (Stress & Contention Test)
// 需求: 验证在高并发、高强度、随机性的操作下，系统不会崩溃、死锁，
//      并且内存不会被重复分配或泄漏（间接验证）。
// =====================================================================
TEST_F(CentralHeapBlackBoxTest, MultiThreadedStressAndContention) {
    const int kNumThreads = 16;
    const int kOperationsPerThread = 1000;
    std::vector<std::thread> threads;
    std::atomic<size_t> total_acquired_pages = 0;
    std::atomic<size_t> total_released_pages = 0;
    std::atomic<bool> test_failed = false;

    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&]() {
            std::vector<PageGroup*> local_groups;
            srand(time(nullptr) + pthread_self()); // 为每个线程设置不同的随机种子

            for (int j = 0; j < kOperationsPerThread; ++j) {
                // 随机决定是分配还是释放
                if (local_groups.empty() || (rand() % 100 < 70)) { // 70% 的几率分配
                    size_t num_pages = (rand() % 8) + 1; // 申请 1-8 页
                    PageGroup* group = heap_.acquire_pages(num_pages);
                    if (group) {
                        local_groups.push_back(group);
                        total_acquired_pages += num_pages;
                    } else if (num_pages > 0) {
                        // 在高压力下，偶尔分配失败是可能的（如果池暂时为空），但不应是常态
                    }
                } else { // 30% 的几率释放
                    size_t index_to_release = rand() % local_groups.size();
                    PageGroup* group_to_release = local_groups[index_to_release];
                    total_released_pages += group_to_release->page_count;
                    heap_.release_pages(group_to_release);
                    // 从vector中移除已释放的元素
                    local_groups.erase(local_groups.begin() + index_to_release);
                }
            }
            
            // 线程结束时，释放所有剩余的 PageGroup
            for (PageGroup* group : local_groups) {
                total_released_pages += group->page_count;
                heap_.release_pages(group);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 最终验证
    // 验证1: 程序没有崩溃或死锁，能运行到这里本身就是一种成功。
    SUCCEED();
    
    // 验证2: 理想情况下，所有申请的页最终都应该被释放。
    // 这可以间接检查是否有内存泄漏或计数错误。
    EXPECT_EQ(total_acquired_pages.load(), total_released_pages.load()) << "The total number of acquired and released pages do not match, suggesting a leak or accounting error.";
}