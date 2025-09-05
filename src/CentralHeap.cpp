#include "gc_malloc/CentralHeap.hpp"
#include "gc_malloc/PageGroup.hpp"
#include "gc_malloc/AlignedMmapper.hpp"
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
    // 1. [在此处添加实现] 加锁。
    
    // 2. [在此处添加实现] 对 num_pages 进行合法性检查 (e.g.,不能为0)。
    
    // 3. [在此处添加实现] 调用 fetch_from_free_lists(num_pages) 获取内存。

    // 4. [在此处添加实现] 如果 fetch 成功：
    //    a. 从 MetadataAllocator 获取一个新的 PageGroup 对象。
    //    b. 填充 PageGroup 的信息 (start_address, page_count 等)。
    //    c. （如果需要 PageMap）在这里注册 PageGroup。
    //    d. 返回这个 PageGroup。
    
    // 5. [在此处添加实现] 如果 fetch 失败，返回 nullptr。

    return nullptr;
}


void CentralHeap::release_pages(PageGroup* group) {
    // 1. [在此处添加实现] 加锁。
    
    // 2. [在此处添加实现] 从 group 中提取 start_address 和 page_count。
    
    // 3. [在此处添加实现] （如果需要 PageMap）在这里注销 PageGroup。
    
    // 4. [在此处添加实现] 将 PageGroup 对象本身归还给 MetadataAllocator。
    
    // 5. [在此处添加实现] 调用 try_merge(start_address, page_count) 处理裸内存。
}


// =====================================================================
// 私有辅助函数实现 (Private Helper Implementation)
// =====================================================================


void CentralHeap::try_merge(void* start_address, size_t num_pages) {
    // 这是最复杂的部分。
    // 1. [在此处添加实现] 在 free_list_by_addr_ 链表中查找插入点。
    //    需要遍历链表，找到第一个地址比 start_address 大的节点。
    
    // 2. [在此处添加实现] 检查前面的节点是否能与当前块合并。
    //    a. 获取插入点的前一个节点 prev_span。
    //    b. 检查 prev_span 是否与当前块物理相邻。
    //    c. 如果相邻，将当前块合并到 prev_span 中 (更新 page_count)，
    //       并从 free_lists_by_size_ 中移除 prev_span (因为它的大小要变了)。
    
    // 3. [在此处添加实现] 检查后面的节点是否能与当前（可能已合并过的）块合并。
    //    a. 获取插入点的后一个节点 next_span。
    //    b. 检查是否相邻。
    //    c. 如果相邻，将 next_span 合并进来，并从两个链表中移除 next_span。
    
    // 4. [在此处添加实现] 将最终形成的（可能经过多次合并的）大块，
    //    作为一个新的 FreePageSpan，同时插入到 free_list_by_addr_
    //    和 free_lists_by_size_ 两个链表中。
    
    // 5. [在此处添加实现] 更新位图 free_list_bitmap_。
}


void* CentralHeap::fetch_from_free_lists(size_t num_pages) {
    if (num_pages > kMaxPages) {
        return nullptr;
    }

    size_t index = free_list_bitmap_.FindFirstSet(num_pages);

    if(index <= kMaxPages) {
        FreePageSpan* list_head = &free_lists_by_size_[index];
        assert(list_head->next_in_size_list != list_head);
        FreePageSpan* found_span = list_head->next_in_size_list;

        found_span->prev_in_size_list->prev_in_size_list = found_span->next_in_size_list;
        found_span->next_in_addr_list->prev_in_size_list = found_span->prev_in_size_list;

        found_span->prev_in_addr_list->next_in_addr_list = found_span->next_in_addr_list;
        found_span->next_in_addr_list->prev_in_addr_list = found_span->prev_in_addr_list;

        if (list_head->next_in_size_list == list_head) {
            free_list_bitmap_.Clear(index);
        }

        if (index > num_pages) {
            char* remaining_start = reinterpret_cast<char*>(found_span) + num_pages * kPageSize;
            size_t remaining_pages = index - num_pages;

            FreePageSpan* remaining_span = reinterpret_cast<FreePageSpan*>(remaining_start);
            remaining_span->page_count = remaining_pages;

            // -- 将这个剩余的块“归还”给 CentralHeap --
            // 我们不能直接调用 release_pages，因为它会加锁导致死锁。
            // 我们需要一个 release_pages_unlocked 的内部版本，或者直接在这里实现入链逻辑。
            // 为简单起见，我们直接调用 try_merge (假设它也是无锁的，或者我们重构它)
            // 一个更简单的做法是直接将 remaining_span 入链。
            
            // (伪代码) 将 remaining_span 插入到两个链表中...
            // add_span_to_free_lists(remaining_span);
            
            // 更新我们找到的块的大小，现在它变小了

            found_span->page_count = num_pages;

        }

    }

    
    // 2. [在此处添加实现] 使用 free_list_bitmap_ 快速查找第一个大小 >= num_pages 的非空链表。
    //    `ssize_t index = free_list_bitmap_.FindFirstSet(num_pages);`
    
    // 3. [在此处添加实现] 如果位图查找成功 (index 有效)：
    //    a. 从 free_lists_by_size_[index] 中取出一个 FreePageSpan。
    //    b. 将这个 FreePageSpan 从 free_lists_by_size_ 和 free_list_by_addr_ 中同时移除。
    //    c. 如果移除后 free_lists_by_size_[index] 变空，更新位图。
    //    d. 如果取出的块大小 (index) > 请求大小 (num_pages)，进行拆分。
    //       - 将多余的部分 (index - num_pages) 作为一个新的 FreePageSpan，
    //         递归地调用 try_merge (或一个类似的入链函数) 将其放回池中。
    //    e. 返回大小正好为 num_pages 的内存块的地址。
    
    // 4. [在此处添加实现] 如果位图查找失败：
    //    a. 向 PageAllocator 请求一块新的大内存 (kPagesPerMmap 页)。
    //    b. 将这块新内存交给 try_merge，让它加入到空闲池中。
    //    c. 递归地再次调用 fetch_from_free_lists(num_pages)。
    
    return nullptr;
}

