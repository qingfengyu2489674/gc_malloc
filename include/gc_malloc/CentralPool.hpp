#ifndef CENTRAL_POOL_H
#define CENTRAL_POOL_H

#include <mutex>
#include "gc_malloc/PageGroup.hpp"
#include "gc_malloc/utils/bitmap.hpp"


class CentralPool {
public:

    static CentralPool& GetInstance;

    PageGroup* acquire_pages(size_t num_pages);

    void release_pages(PageGroup* group);


private:
    CentralPool();
    ~CentralPool();
    CentralPool(const CentralPool&) = delete;
    CentralPool& operator=(const CentralPool&) = delete;

    struct FreePageSpan {
        // 用于链接到“按大小分类”的空闲链表
        FreePageSpan* next_in_size_list;
        FreePageSpan* prev_in_size_list;

        // 用于链接到“按地址排序”的空闲链表
        FreePageSpan* next_in_addr_list;
        FreePageSpan* prev_in_addr_list;
        
        size_t page_count; // 这段空闲内存有多少页
    };

    
    /**
     * @brief 尝试合并一个新归还的内存块。
     * 这是 release_pages 的核心辅助函数。
     */
    void try_merge(void* start_address, size_t num_pages);

    /**
     * @brief 内部函数，用于从空闲链表中获取内存。
     * 如果找不到，会尝试从更大的块中拆分。
     */
    void* fetch_from_free_lists(size_t num_pages);


private:
    static constexpr size_t kPagesPerMmap = 256;
    static constexpr size_t kMaxPages = kPagesPerMmap;
    static constexpr size_t kPageSize = 4 * 1024;
    

    // 按大小分类的空闲链表数组 (哨兵节点)
    FreePageSpan free_lists_by_size_[kMaxPages + 1];

    gc_malloc::utils::Bitmap free_list_bitmap_;

    // 按地址排序的空闲链表 (哨兵节点)
    FreePageSpan free_list_by_addr_;
    
    std::mutex mutex_;

};



#endif // CENTRAL_POOL_H