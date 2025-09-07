#ifndef CENTRAL_POOL_H
#define CENTRAL_POOL_H

#include <mutex>
#include "gc_malloc/PageGroup.hpp"
#include "gc_malloc/Bitmap.hpp"


class CentralHeap {
public:
    static CentralHeap& GetInstance();
    PageGroup* acquire_pages(size_t num_pages);
    void release_pages(PageGroup* group);

private:
    CentralHeap();
    ~CentralHeap();
    CentralHeap(const CentralHeap&) = delete;
    CentralHeap& operator=(const CentralHeap&) = delete;

    struct FreePageSpan {
        FreePageSpan* next_in_size_list;
        FreePageSpan* prev_in_size_list;

        FreePageSpan* next_in_addr_list;
        FreePageSpan* prev_in_addr_list;
        
        size_t page_count;
    };
    
    void reclaim_pages_unlocked(void* start_address, size_t num_pages);
    void* fetch_from_free_lists_unlocked(size_t num_pages);

private:
    static constexpr size_t kPagesPerMmap = 256;
    static constexpr size_t kMaxPages = kPagesPerMmap;
    static constexpr size_t kPageSize = 4 * 1024;

    Bitmap free_list_bitmap_;
    FreePageSpan free_lists_by_size_[kMaxPages + 1];
    FreePageSpan free_list_by_addr_;
    
    std::mutex mutex_;
};

#endif // CENTRAL_POOL_H