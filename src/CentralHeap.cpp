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
    // ... (加锁和调用 fetch_from_free_lists_unlocked 的逻辑不变) ...
    void* raw_mem = fetch_from_free_lists_unlocked(num_pages);
    if (raw_mem == nullptr) {
        return nullptr;
    }

    // --- 将裸内存包装成 PageGroup (不使用 new) ---

    // a. 从 MetadataAllocator 获取一块用于存放 PageGroup 对象的内存。
    void* pg_mem = MetadataAllocator::GetInstance().allocate(sizeof(PageGroup));
    if (pg_mem == nullptr) {
        reclaim_pages_unlocked(raw_mem, num_pages);
        return nullptr;
    }
    
    // b. 直接将获取到的内存指针转换为 PageGroup* 类型。
    //    我们现在有了一个指向一块未初始化内存的 PageGroup 指针。
    PageGroup* group = static_cast<PageGroup*>(pg_mem);

    // c. 手动为 PageGroup 的所有成员变量赋值。
    group->start_address = raw_mem;
    group->page_count = num_pages;
    group->block_size = 0;
    group->block_in_used_count = 0;
    // 如果您在 PageGroup 中还有 total_block_count，也在这里初始化
    // group->total_block_count = 0; 
    
    // d. 返回构造完成的 PageGroup 句柄。
    return group;
}

/**
 * @brief 将一个 PageGroup 归还给 CentralHeap。
 * 这个接口由 ThreadCache 调用，当一个 PageGroup 完全变为空闲时。
 * 它是线程安全的。
 */
void CentralHeap::release_pages(PageGroup* group) {
    // 防御性检查：不处理空指针。
    if (group == nullptr) {
        return;
    }

    // 1. 加锁，保证整个回收过程的原子性和线程安全。
    std::lock_guard<std::mutex> lock(mutex_);

    // 2. 从 PageGroup 句柄中提取出裸内存的信息。
    //    这是我们最后一次使用 group 的机会，在它被销毁之前。
    void* start_address = group->start_address;
    const size_t num_pages = group->page_count;

    // 3. (如果未来需要PageMap) 在这里注销 PageGroup。
    //    PageMap::GetInstance().unregister_span(start_address, num_pages);
    
    // 如果您不使用 new 来创建 PageGroup，那么也就不需要显式调用析构函数。
    // 如果您使用了 placement new，则需要在这里加上：group->~PageGroup();

    // 4. 将 PageGroup 对象本身占用的内存，归还给元数据分配器。
    MetadataAllocator::GetInstance().deallocate(group, sizeof(PageGroup));

    // 5. 调用无锁的内部函数，来处理裸内存的回收、合并和入池。
    reclaim_pages_unlocked(start_address, num_pages);
}

// =====================================================================
// 私有辅助函数实现 (Private Helper Implementation)
// =====================================================================



void CentralHeap::reclaim_pages_unlocked(void* start_address, size_t num_pages) {
    assert(start_address != nullptr && num_pages > 0);
    const size_t region_size_bytes = kPagesPerMmap * kPageSize;
    FreePageSpan* current_span = nullptr;

    // --- 步骤 1: 查找插入点并尝试向前合并 ---
    FreePageSpan* insertion_point = free_list_by_addr_.next_in_addr_list;
    while(insertion_point != &free_list_by_addr_ && insertion_point < start_address) {
        insertion_point = insertion_point->next_in_addr_list;
    }

    FreePageSpan* prev_span = insertion_point->prev_in_addr_list;
    if (prev_span != &free_list_by_addr_ &&
        (reinterpret_cast<char*>(prev_span) + prev_span->page_count * kPageSize) == start_address &&
        ((reinterpret_cast<uintptr_t>(prev_span) & ~(region_size_bytes - 1)) == 
         (reinterpret_cast<uintptr_t>(start_address) & ~(region_size_bytes - 1)))) 
    {
        const size_t prev_span_original_size = prev_span->page_count;
        
        // 从 size 链表中移除 prev_span
        prev_span->prev_in_size_list->next_in_size_list = prev_span->next_in_size_list;
        prev_span->next_in_size_list->prev_in_size_list = prev_span->prev_in_size_list;
        if (free_lists_by_size_[prev_span_original_size].next_in_size_list == &free_lists_by_size_[prev_span_original_size]) {
            free_list_bitmap_.Clear(prev_span_original_size);
        }
        
        prev_span->page_count += num_pages;
        current_span = prev_span;
    } else {
        // 无法向前合并，则在裸内存上创建新 Span
        current_span = static_cast<FreePageSpan*>(start_address);
        current_span->page_count = num_pages;
        
        // 并将其插入到 addr 链表
        current_span->next_in_addr_list = insertion_point;
        current_span->prev_in_addr_list = prev_span;
        prev_span->next_in_addr_list = current_span;
        insertion_point->prev_in_addr_list = current_span;
    }

    // --- 步骤 2: 尝试向后合并 ---
    FreePageSpan* next_span = current_span->next_in_addr_list;
    if (next_span != &free_list_by_addr_ &&
        (reinterpret_cast<char*>(current_span) + current_span->page_count * kPageSize) == reinterpret_cast<char*>(next_span) &&
        ((reinterpret_cast<uintptr_t>(current_span) & ~(region_size_bytes - 1)) == 
         (reinterpret_cast<uintptr_t>(next_span) & ~(region_size_bytes - 1))))
    {
        const size_t next_span_original_size = next_span->page_count;

        // 从 size 链表中移除 next_span
        next_span->prev_in_size_list->next_in_size_list = next_span->next_in_size_list;
        next_span->next_in_size_list->prev_in_size_list = next_span->prev_in_size_list;
        if (free_lists_by_size_[next_span_original_size].next_in_size_list == &free_lists_by_size_[next_span_original_size]) {
            free_list_bitmap_.Clear(next_span_original_size);
        }

        // 从 addr 链表中移除 next_span
        next_span->prev_in_addr_list->next_in_addr_list = next_span->next_in_addr_list;
        next_span->next_in_addr_list->prev_in_addr_list = next_span->prev_in_addr_list;
        
        // 扩大 current_span
        current_span->page_count += next_span_original_size;
    }

    FreePageSpan* final_span = current_span;
    const size_t final_page_count = final_span->page_count;

    // --- 新增的步骤: 检查是否可以归还给OS ---
    
    // 条件：合并后的块大小正好等于一个完整的 Region，
    // 并且它的起始地址也是按 Region 大小对齐的。
    if (final_page_count == kPagesPerMmap &&
        (reinterpret_cast<uintptr_t>(final_span) % (kPagesPerMmap * kPageSize) == 0) &&
        (free_lists_by_size_[kMaxPages].next_in_size_list != &free_lists_by_size_[kMaxPages])) 
    {
        // --- 可以归还给操作系统 ---

        // 1. 将这个完整的 Region 从按地址排序的链表中移除。
        final_span->prev_in_addr_list->next_in_addr_list = final_span->next_in_addr_list;
        final_span->next_in_addr_list->prev_in_addr_list = final_span->prev_in_addr_list;

        // 2. 调用 AlignedMmapper 将其 munmap。
        AlignedMmapper::deallocate_aligned(final_span, final_page_count * kPageSize);
        
        // 3. 函数到此结束。我们不需要再将它加入任何空闲链表。
        return;
    }

    assert(final_page_count > 0 && final_page_count <= kMaxPages);

    FreePageSpan* list_head = &free_lists_by_size_[final_page_count];
    
    final_span->next_in_size_list = list_head->next_in_size_list;
    final_span->prev_in_size_list = list_head;
    list_head->next_in_size_list->prev_in_size_list = final_span;
    list_head->next_in_size_list = final_span;
    
    free_list_bitmap_.Set(final_page_count);
}


/**
 * @brief [无锁] 从空闲链表中获取一块内存，如果找不到则尝试拆分或向底层申请。
 */
void* CentralHeap::fetch_from_free_lists_unlocked(size_t num_pages) {
    // 防御性检查：确保请求的页数有效且在我们管理的范围内。
    assert(num_pages > 0);
    if (num_pages > kMaxPages) {
        // 对于 CentralHeap 无法精细化管理的超大块，直接返回失败。
        // 上层可以考虑直接向 AlignedMmapper 请求。
        return nullptr;
    }

    // --- 步骤 1: 使用位图快速查找一个足够大的空闲块 ---

    // 从 num_pages 开始，查找第一个被设置的位，代表第一个大小 >= num_pages 的非空链表。
    size_t index = free_list_bitmap_.FindFirstSet(num_pages);

    // --- 步骤 2: 处理位图查找成功的情况 ---

    if (index <= kMaxPages) { // 使用 <= kMaxPages 检查是否找到了有效的索引
        // a. 从 free_lists_by_size_[index] 中取出一个 FreePageSpan。
        FreePageSpan* list_head = &free_lists_by_size_[index];
        assert(list_head->next_in_size_list != list_head); // 断言链表确实非空
        FreePageSpan* found_span = list_head->next_in_size_list;

        // b. 将这个 FreePageSpan 从两个链表中同时移除。
        // (从 size 链表中移除)
        found_span->prev_in_size_list->next_in_size_list = found_span->next_in_size_list;
        found_span->next_in_size_list->prev_in_size_list = found_span->prev_in_size_list;
        
        // (从 addr 链表中移除)
        found_span->prev_in_addr_list->next_in_addr_list = found_span->next_in_addr_list;
        found_span->next_in_addr_list->prev_in_addr_list = found_span->prev_in_addr_list;

        // c. 如果移除后 size 链表变空，更新位图。
        if (list_head->next_in_size_list == list_head) {
            free_list_bitmap_.Clear(index);
        }

        // d. 如果取出的块 (大小为 index) 比请求的块 (大小为 num_pages) 大，进行拆分。
        if (index > num_pages) {
            // -- 创建一个新的 FreePageSpan 来描述剩余的部分 --
            // 计算剩余部分的起始地址
            char* remaining_start_addr = reinterpret_cast<char*>(found_span) + num_pages * kPageSize;
            // 计算剩余部分的页数
            const size_t remaining_pages = index - num_pages;

            // -- 将这个剩余的块“归还”给 CentralHeap --
            // 我们直接调用 reclaim_pages_unlocked，因为它包含了所有正确的入链和合并逻辑。
            reclaim_pages_unlocked(remaining_start_addr, remaining_pages);
            
            // -- 更新我们即将返回的块的大小 --
            // 注意：这一步不是必需的，因为上层会根据 num_pages 创建 PageGroup。
            // 但为了保持 FreePageSpan 内部数据的一致性，最好更新它。
            found_span->page_count = num_pages;
        }
        
        // e. 返回大小正好为 num_pages 的内存块的地址。
        // 返回的是 FreePageSpan 对象本身的地址，上层将在此之上创建 PageGroup。
        return found_span;
    }

    // --- 步骤 3: 处理位图查找失败的情况 (没有足够大的空闲块) ---

    // a. 向 AlignedMmapper 请求一块新的、对齐的大块内存 (Region)。
    void* new_region = AlignedMmapper::allocate_aligned(kPagesPerMmap * kPageSize);
    if (new_region == nullptr) {
        // 如果底层OS也耗尽内存，则分配失败。
        return nullptr;
    }

    // b. 将这块新内存作为一个大的 FreePageSpan 归还到池中。
    // 这会填充我们的空闲链表，并可能与现有的小块合并。
    reclaim_pages_unlocked(new_region, kPagesPerMmap);

    // c. 递归地再次调用本函数来完成分配。
    // 这次调用几乎肯定会成功，因为我们刚刚补充了一大块内存。
    // 这是一个非常简洁和强大的模式。
    return fetch_from_free_lists_unlocked(num_pages);
}