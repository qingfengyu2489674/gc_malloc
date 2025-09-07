#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <unordered_set>
#include <chrono>
#include <random>

#include "gc_malloc/ThreadHeap.hpp"
#include "gc_malloc/BlockHeader.hpp"


class ThreadHeapTest : public ::testing::Test {
protected:
    // SetUp 现在可以简化，因为 GetInstance 会自动创建
    void SetUp() override {
        // 调用 GetInstance() 确保当前线程的 heap 实例存在
        th_ = ThreadHeap::GetInstance();
        ASSERT_NE(th_, nullptr);
    }

    // 在测试夹具中保存当前线程的 heap 指针，方便使用
    ThreadHeap* th_;
};

// =====================================================================
// 测试 1: 小对象分配与回收
// =====================================================================
TEST_F(ThreadHeapTest, SmallObjectAllocationAndGC) {
    const size_t alloc_size = 64;

    void* p1 = th_->allocate(alloc_size);
    ASSERT_NE(p1, nullptr);
    
    // deallocate 是静态的，调用方式不变
    ThreadHeap::deallocate(p1);

    th_->garbage_collect();

    void* p2 = th_->allocate(alloc_size);
    ASSERT_NE(p2, nullptr);

    EXPECT_EQ(p1, p2) << "The block was not correctly recycled by garbage_collect.";
}

// =====================================================================
// 测试 2: 大对象分配与回收
// =====================================================================
TEST_F(ThreadHeapTest, LargeObjectAllocationAndGC) {
    const size_t large_alloc_size = 32 * 1024; // 32KB

    void* p1 = th_->allocate(large_alloc_size);
    ASSERT_NE(p1, nullptr);
    
    ThreadHeap::deallocate(p1);
    th_->garbage_collect();

    void* p2 = th_->allocate(large_alloc_size);
    ASSERT_NE(p2, nullptr) << "Failed to allocate another large object after GC.";
}

// =====================================================================
// 测试 3: Refill 机制
// =====================================================================
TEST_F(ThreadHeapTest, RefillMechanism) {
    const size_t alloc_size = 48;
    std::vector<void*> pointers;
    for (int i = 0; i < 500; ++i) {
        void* p = th_->allocate(alloc_size);
        ASSERT_NE(p, nullptr) << "Allocation failed during refill stress test at iteration " << i;
        pointers.push_back(p);
    }
    SUCCEED();
    for (void* p : pointers) {
        ThreadHeap::deallocate(p);
    }
    th_->garbage_collect();
}


// =====================================================================
// 测试 4: 跨线程释放 (Cross-Thread Free)
// =====================================================================
TEST_F(ThreadHeapTest, CrossThreadFreeAndGC) {
    std::atomic<void*> shared_ptr = nullptr; // 使用 atomic 保证多线程可见性

    // 线程1 (分配者)
    std::thread allocator_thread([&]() {
        // 在新线程中，通过 GetInstance() 获取该线程自己的 heap
        ThreadHeap* local_th = ThreadHeap::GetInstance();
        
        void* p = local_th->allocate(128);
        shared_ptr.store(p); // 原子地写入共享指针
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        local_th->garbage_collect();
        
        void* p2 = local_th->allocate(128);
        EXPECT_EQ(p, p2) << "Allocator thread failed to GC a block freed by another thread.";
    });

    // 线程2 (释放者)
    std::thread deallocator_thread([&]() {
        void* p_to_free = nullptr;
        // 确保能读到非空的指针
        while ((p_to_free = shared_ptr.load()) == nullptr) { 
            std::this_thread::yield(); 
        }
        
        ThreadHeap::deallocate(p_to_free);
    });

    allocator_thread.join();
    deallocator_thread.join();
}


// =====================================================================
// 测试 5: 混合尺寸并发分配测试 (带中间状态验证)
// =====================================================================
TEST_F(ThreadHeapTest, MixedSizeConcurrentAllocation) {
    const int kNumThreads = std::thread::hardware_concurrency();
    const int kAllocationsPerThread = 20000; // 恢复到较高的压力值
    std::vector<std::thread> threads;
    
    std::unordered_set<void*> all_pointers;
    std::mutex set_mutex;

    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&]() {
            ThreadHeap* th = ThreadHeap::GetInstance();
            std::vector<void*> local_pointers;
            local_pointers.reserve(kAllocationsPerThread);
            
            std::vector<size_t> sizes = {32, 64, 128, 256, 512, 1024};

            // ✅ 2. 为每个线程创建独立的随机数生成器
            // 使用 thread_local 确保每个线程只初始化一次，效率更高
            thread_local std::mt19937 generator(
                // 使用线程ID和当前时间组合作为种子，确保不同线程和不同运行次有不同的序列
                std::hash<std::thread::id>{}(std::this_thread::get_id()) + time(nullptr)
            );
            std::uniform_int_distribution<size_t> distribution(0, sizes.size() - 1);

            for (int j = 0; j < kAllocationsPerThread; ++j) {
                // ✅ 3. 使用新的、线程安全的随机数生成方式
                size_t alloc_size = sizes[distribution(generator)];
                
                void* p = th->allocate(alloc_size);
                ASSERT_NE(p, nullptr);
                local_pointers.push_back(p);
            }

            {
                std::lock_guard<std::mutex> lock(set_mutex);
                for (void* p : local_pointers) {
                    ASSERT_TRUE(all_pointers.insert(p).second) 
                        << "Duplicate pointer detected during concurrent allocation!";
                }
            }

            for (void* p : local_pointers) {
                ThreadHeap::deallocate(p);
            }
            th->garbage_collect();
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    
    const size_t total_allocations = kNumThreads * kAllocationsPerThread;
    EXPECT_EQ(all_pointers.size(), total_allocations) << "The number of unique pointers does not match the total number of allocations.";
}

// =====================================================================
// 测试 6: 内存搅动与碎片化测试 (带中间状态验证)
// =====================================================================
TEST_F(ThreadHeapTest, ChurnAndFragmentation) {
    const size_t kNumCycles = 50;
    const size_t kAllocsPerCycle = 1000;
    const size_t alloc_size = 128;

    std::vector<void*> pointers;
    pointers.reserve(kAllocsPerCycle);

    // [中间状态验证] 我们获取第一个分配的指针，用于后续的重用检查
    void* first_ptr = th_->allocate(alloc_size);
    ASSERT_NE(first_ptr, nullptr);
    ThreadHeap::deallocate(first_ptr);
    th_->garbage_collect();
    
    void* reused_ptr = th_->allocate(alloc_size);
    ASSERT_EQ(first_ptr, reused_ptr) << "Allocator failed to reuse a simple freed block.";
    pointers.push_back(reused_ptr);


    for (size_t cycle = 0; cycle < kNumCycles; ++cycle) {
        for (size_t i = 1; i < kAllocsPerCycle; ++i) { // 从1开始，因为我们已经有了一个
            void* p = th_->allocate(alloc_size);
            ASSERT_NE(p, nullptr);
            pointers.push_back(p);
        }
        
        std::random_shuffle(pointers.begin(), pointers.end());
        
        for (void* p : pointers) {
            ThreadHeap::deallocate(p);
        }
        pointers.clear();
        
        th_->garbage_collect();

        // [中间状态验证] 在每一轮搅动和GC之后，
        // 我们都尝试重新分配一个块。它应该能成功，并且很可能
        // 是之前释放的某个地址，这证明了回收和合并是有效的。
        void* p_after_gc = th_->allocate(alloc_size);
        ASSERT_NE(p_after_gc, nullptr) << "Allocation failed after a churn-and-gc cycle " << cycle;
        pointers.push_back(p_after_gc); // 为下一轮循环做准备
    }
    
    // 清理最后一轮的指针
    for (void* p : pointers) {
        ThreadHeap::deallocate(p);
    }
    th_->garbage_collect();
    SUCCEED();
}


#include <condition_variable>
#include <queue>

// =====================================================================
// 测试 7: 跨线程生产者消费者压力测试 (带中间状态验证)
// =====================================================================
TEST_F(ThreadHeapTest, ProducerConsumerStressTest) {
    const int kNumProducers = 4;
    const int kNumConsumers = 4;
    const int kItemsPerProducer = 5000;
    const int total_items = kNumProducers * kItemsPerProducer;

    std::queue<void*> shared_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<int> items_produced_count = 0;
    std::atomic<int> items_consumed_count = 0;

    std::vector<std::thread> producers;
    for (int i = 0; i < kNumProducers; ++i) {
        producers.emplace_back([&]() {
            ThreadHeap* th = ThreadHeap::GetInstance();
            for (int j = 0; j < kItemsPerProducer; ++j) {
                void* p = th->allocate(256);
                ASSERT_NE(p, nullptr);
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    shared_queue.push(p);
                }
                items_produced_count++;
                cv.notify_one();
            }
            th->garbage_collect();
        });
    }

    std::vector<std::thread> consumers;
    for (int i = 0; i < kNumConsumers; ++i) {
        consumers.emplace_back([&]() {
            ThreadHeap* th = ThreadHeap::GetInstance();
            while (items_consumed_count.load() < total_items) {
                void* p = nullptr;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    // 等待队列非空，或者所有产品都已生产完毕
                    if (!cv.wait_for(lock, std::chrono::seconds(1), [&] { return !shared_queue.empty() || items_produced_count.load() >= total_items; })) {
                        // 超时，可能是死锁或逻辑错误
                        FAIL() << "Consumer timed out waiting for item.";
                        break;
                    }

                    if (!shared_queue.empty()) {
                        p = shared_queue.front();
                        shared_queue.pop();
                        items_consumed_count++;
                    } else if (items_produced_count.load() >= total_items) {
                        break; // 退出循环
                    }
                }
                if (p) {
                    ThreadHeap::deallocate(p);
                }
            }
            th->garbage_collect();
        });
    }

    for (auto& t : producers) t.join();
    cv.notify_all();
    for (auto& t : consumers) t.join();

    // [最终状态验证]
    EXPECT_EQ(items_produced_count.load(), total_items);
    EXPECT_EQ(items_consumed_count.load(), total_items);
    EXPECT_TRUE(shared_queue.empty());
    SUCCEED();
}