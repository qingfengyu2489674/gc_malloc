// 文件: include/gc_malloc/ThreadHeap.hpp
#ifndef GC_MALLOC_THREAD_CACHE_HPP
#define GC_MALLOC_THREAD_CACHE_HPP

#include "gc_malloc/SizeClassInfo.hpp"
#include "gc_malloc/BlockHeader.hpp"
#include "gc_malloc/atomic_ops.hpp"

class PageGroup;

class ThreadHeap {

public:
    static ThreadHeap* GetInstance();
    static void deallocate(void* ptr);

    void* allocate(size_t size);
    void garbage_collect();

private:
    ThreadHeap() = default;
    ~ThreadHeap();

    ThreadHeap(const ThreadHeap&) = delete;
    ThreadHeap& operator=(const ThreadHeap&) = delete;

private:
    bool refill(size_t index);
    PageGroup* request_pages_from_central_heap(size_t num_pages);
    void release_pages_to_central_heap(PageGroup* group);

private:
    struct FreeList {
        BlockHeader* head = nullptr;
        size_t count = 0;
    };

    static thread_local ThreadHeap* tls_instance_;

    FreeList free_lists_[kNumSizeClasses];
    BlockHeader* managed_list_head_ = nullptr;
};

#endif // GC_MALLOC_THREAD_CACHE_HPP