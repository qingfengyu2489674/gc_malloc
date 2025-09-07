#ifndef GC_MALLOC_CENTRAL_HEAP_HPP
#define GC_MALLOC_CENTRAL_HEAP_HPP

#include <mutex>
#include <cstddef>
#include "gc_malloc/PageGroup.hpp"
#include "gc_malloc/Bitmap.hpp"

class CentralHeap {
public:
    // ================== 公共接口 ==================
    static CentralHeap& GetInstance();
    PageGroup* acquire_pages(size_t num_pages);
    void release_pages(PageGroup* group);

private:
    // ================== 核心常量 ==================
    static constexpr size_t kPageSize = 4 * 1024;
    static constexpr size_t kPagesPerMmap = 256;
    static constexpr size_t kRegionSizeBytes = kPagesPerMmap * kPageSize;
    static constexpr size_t kMaxPages = kPagesPerMmap;

private:
    // ================== 核心数据结构 ==================
    struct FreePageSpan {
        FreePageSpan* next_in_size_list;
        FreePageSpan* prev_in_size_list;
        FreePageSpan* next_in_addr_list;
        FreePageSpan* prev_in_addr_list;
        size_t page_count;
    };

    Bitmap free_list_bitmap_;
    FreePageSpan free_lists_by_size_[kMaxPages + 1];
    FreePageSpan free_list_by_addr_;
    std::mutex mutex_;

private:
    // ================== 单例模式实现 ==================
    CentralHeap();
    ~CentralHeap();
    CentralHeap(const CentralHeap&) = delete;
    CentralHeap& operator=(const CentralHeap&) = delete;

private:
    // ================== 私有辅助函数 ==================
    
    // --- 主要的无锁工作流 ---
    void* fetch_from_free_lists_unlocked(size_t num_pages);
    void reclaim_pages_unlocked(void* start_address, size_t num_pages);
    
    // --- 获取路径的子程序 ---
    FreePageSpan* find_best_fit_span(size_t num_pages);
    void* split_span(FreePageSpan* span, size_t num_pages_to_acquire);
    void* mmap_new_region();

    // --- 回收路径的子程序 ---
    FreePageSpan* find_addr_insertion_point(const void* start_address);
    FreePageSpan* try_merge_with_neighbors(FreePageSpan* span);
    void munmap_region(void* region_ptr);
    
    // --- 底层链表与位图操作 ---
    void remove_from_size_list(FreePageSpan* span);
    void add_to_size_list(FreePageSpan* span);

    // --- 静态检查工具函数 ---
    static bool is_in_same_region(const void* addr1, const void* addr2);
    static bool is_adjacent(const FreePageSpan* span1, const FreePageSpan* span2);
};

#endif // GC_MALLOC_CENTRAL_HEAP_HPP