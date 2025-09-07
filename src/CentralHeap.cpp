#include "gc_malloc/CentralHeap.hpp"
#include "gc_malloc/PageGroup.hpp"
#include "gc_malloc/AlignedMmapper.hpp"
#include "gc_malloc/MetadataAllocor.hpp"
#include <assert.h>


// =====================================================================
// 单例模式实现 (Singleton Implementation)
// =====================================================================

CentralHeap& CentralHeap::GetInstance() {
    static CentralHeap instance;
    return instance;
}


// =====================================================================
// 构造与析构 (Constructor & Destructor)
// =====================================================================

CentralHeap::CentralHeap() 
    :free_list_bitmap_(kMaxPages + 1)
{
    for(size_t i = 0; i <= kMaxPages; i++) {
        free_lists_by_size_[i].next_in_size_list = &free_lists_by_size_[i];
        free_lists_by_size_[i].prev_in_size_list = &free_lists_by_size_[i];
    }

    free_list_by_addr_.next_in_addr_list = &free_list_by_addr_;
    free_list_by_addr_.prev_in_addr_list = &free_list_by_addr_;
}


CentralHeap::~CentralHeap() {

}

// =====================================================================
// 公共接口实现 (Public API Implementation)
// =====================================================================

PageGroup* CentralHeap::acquire_pages(size_t num_pages) {
    if (num_pages == 0 || num_pages > kMaxPages) {
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    void* raw_mem = fetch_from_free_lists_unlocked(num_pages);
    if (raw_mem == nullptr) {
        return nullptr;
    }

    void* pg_mem = MetadataAllocator::GetInstance().allocate(sizeof(PageGroup));
    if (pg_mem == nullptr) {
        reclaim_pages_unlocked(raw_mem, num_pages);
        return nullptr;
    }

    PageGroup* group = static_cast<PageGroup*>(pg_mem);

    group->start_address = raw_mem;
    group->page_count = num_pages;
    group->block_size = 0;
    group->block_in_used_count = 0;

    return group;
}


void CentralHeap::release_pages(PageGroup* group) {
    if (group == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    void* start_address = group->start_address;
    const size_t num_pages = group->page_count;
    MetadataAllocator::GetInstance().deallocate(group, sizeof(PageGroup));

    reclaim_pages_unlocked(start_address, num_pages);
}


// =====================================================================
// 私有辅助函数实现 (Private Helper Implementation)
// =====================================================================

void CentralHeap::reclaim_pages_unlocked(void* start_address, size_t num_pages) {
    assert(start_address != nullptr && num_pages > 0);

    FreePageSpan* new_span = static_cast<FreePageSpan*>(start_address);
    new_span->page_count = num_pages;

    FreePageSpan* insertion_point = find_addr_insertion_point(start_address);
    new_span->next_in_addr_list = insertion_point;
    new_span->prev_in_addr_list = insertion_point->prev_in_addr_list;
    insertion_point->prev_in_addr_list->next_in_addr_list = new_span;
    insertion_point->prev_in_addr_list = new_span;

    FreePageSpan* final_span = try_merge_with_neighbors(new_span);
    const size_t final_page_count = final_span->page_count;

    if (final_page_count == kPagesPerMmap &&
        (reinterpret_cast<uintptr_t>(final_span) % (kPagesPerMmap * kPageSize) == 0) &&
        (free_lists_by_size_[kMaxPages].next_in_size_list != &free_lists_by_size_[kMaxPages]))
    {
        final_span->prev_in_addr_list->next_in_addr_list = final_span->next_in_addr_list;
        final_span->next_in_addr_list->prev_in_addr_list = final_span->prev_in_addr_list;

        munmap_region(final_span);
        return;
    }

    add_to_size_list(final_span);
}


void* CentralHeap::fetch_from_free_lists_unlocked(size_t num_pages) {
    assert(num_pages > 0 && num_pages <= kMaxPages);

    while (true) {
        FreePageSpan* found_span = find_best_fit_span(num_pages);
        
        if (found_span != nullptr) {
            return split_span(found_span, num_pages);
        }
        
        void* new_region = mmap_new_region();
        if (new_region == nullptr) {
            return nullptr;
        }
        reclaim_pages_unlocked(new_region, kPagesPerMmap);
    }
    return nullptr;
}


CentralHeap::FreePageSpan* CentralHeap::try_merge_with_neighbors(FreePageSpan* span) {
    assert(span != nullptr && span != &free_list_by_addr_);
    
    FreePageSpan* prev_span = span->prev_in_addr_list;
    if (prev_span != &free_list_by_addr_ && is_adjacent(prev_span, span) && is_in_same_region(prev_span, span)) {
        remove_from_size_list(prev_span);

        span->prev_in_addr_list->next_in_addr_list = span->next_in_addr_list;
        span->next_in_addr_list->prev_in_addr_list = span->prev_in_addr_list;

        prev_span->page_count += span->page_count;
        span = prev_span;
    }

    FreePageSpan* next_span = span->next_in_addr_list;
    if (next_span != &free_list_by_addr_ && is_adjacent(span, next_span) && is_in_same_region(span, next_span)) {
        remove_from_size_list(next_span);

        next_span->prev_in_addr_list->next_in_addr_list = next_span->next_in_addr_list;
        next_span->next_in_addr_list->prev_in_addr_list = next_span->prev_in_addr_list;

        span->page_count += next_span->page_count;
    }

    return span;
}


CentralHeap::FreePageSpan* CentralHeap::find_best_fit_span(size_t num_pages) {
    size_t index = free_list_bitmap_.FindFirstSet(num_pages);

    if (index > kMaxPages) {
        return nullptr;
    }

    FreePageSpan* list_head = &free_lists_by_size_[index];
    assert(list_head->next_in_size_list != list_head); // 断言链表确实非空
    FreePageSpan* found_span = list_head->next_in_size_list;

    found_span->prev_in_size_list->next_in_size_list = found_span->next_in_size_list;
    found_span->next_in_size_list->prev_in_size_list = found_span->prev_in_size_list;
    
    found_span->prev_in_addr_list->next_in_addr_list = found_span->next_in_addr_list;
    found_span->next_in_addr_list->prev_in_addr_list = found_span->prev_in_addr_list;

    if (list_head->next_in_size_list == list_head) {
        free_list_bitmap_.Clear(index);
    }

    return found_span;
}


void* CentralHeap::split_span(FreePageSpan* span, size_t num_pages_to_acquire) {
    assert(span != nullptr);
    assert(span->page_count >= num_pages_to_acquire);

    const size_t original_size = span->page_count;
    if (original_size > num_pages_to_acquire) {
        const size_t remaining_pages = original_size - num_pages_to_acquire;
        
        char* remaining_start_addr = reinterpret_cast<char*>(span) + num_pages_to_acquire * kPageSize;
        reclaim_pages_unlocked(remaining_start_addr, remaining_pages);
        span->page_count = num_pages_to_acquire;
    }

    return span;
}


void* CentralHeap::mmap_new_region() {

    void* new_region = AlignedMmapper::allocate_aligned(kRegionSizeBytes);

    if (new_region == nullptr) {
        return nullptr;
    }

    return new_region;
}


void CentralHeap::munmap_region(void* region_ptr) {
    assert(region_ptr != nullptr);
    assert(reinterpret_cast<uintptr_t>(region_ptr) % kRegionSizeBytes == 0);
    AlignedMmapper::deallocate_aligned(region_ptr, kRegionSizeBytes);
}

bool CentralHeap::is_in_same_region(const void* addr1, const void* addr2) {
    const uintptr_t region_mask = ~(kRegionSizeBytes - 1);
    return (reinterpret_cast<uintptr_t>(addr1) & region_mask) == 
           (reinterpret_cast<uintptr_t>(addr2) & region_mask);
}

bool CentralHeap::is_adjacent(const FreePageSpan* span1, const FreePageSpan* span2) {
    return (reinterpret_cast<const char*>(span1) + span1->page_count * kPageSize) == 
           reinterpret_cast<const char*>(span2);
}

void CentralHeap::remove_from_size_list(FreePageSpan* span) {
    const size_t original_size = span->page_count;
    span->prev_in_size_list->next_in_size_list = span->next_in_size_list;
    span->next_in_size_list->prev_in_size_list = span->prev_in_size_list;

    if (free_lists_by_size_[original_size].next_in_size_list == &free_lists_by_size_[original_size]) {
        free_list_bitmap_.Clear(original_size);
    }
}

void CentralHeap::add_to_size_list(FreePageSpan* span) {
    const size_t page_count = span->page_count;
    assert(page_count > 0 && page_count <= kMaxPages);

    FreePageSpan* list_head = &free_lists_by_size_[page_count];
    
    span->next_in_size_list = list_head->next_in_size_list;
    span->prev_in_size_list = list_head;
    list_head->next_in_size_list->prev_in_size_list = span;
    list_head->next_in_size_list = span;
    
    free_list_bitmap_.Set(page_count);
}

CentralHeap::FreePageSpan* CentralHeap::find_addr_insertion_point(const void* start_address) {
    FreePageSpan* current = free_list_by_addr_.next_in_addr_list;

    while (current != &free_list_by_addr_ && current < start_address) {
        current = current->next_in_addr_list;
    }
    return current;
}