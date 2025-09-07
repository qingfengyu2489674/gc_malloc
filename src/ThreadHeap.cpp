#include "gc_malloc/ThreadHeap.hpp"
#include "gc_malloc/CentralHeap.hpp"
#include "gc_malloc/PageGroup.hpp" // 需要 PageGroup 的完整定义
#include <cassert>


// =====================================================================
//                 线程局部存储 (Thread-Local Storage)
// =====================================================================

// 每个线程都拥有一个指向自己 ThreadHeap 实例的指针。
// 初始为 nullptr，在第一次分配时创建。
thread_local ThreadHeap* ThreadHeap::tls_instance_ = nullptr;

// =====================================================================
// 构造与析构 (Constructor & Destructor)
// =====================================================================


ThreadHeap::~ThreadHeap() {
    // 析构函数为空
}


// =====================================================================
// 公共接口实现 (Public API Implementation)
// =====================================================================

ThreadHeap* ThreadHeap::GetInstance() {
    // 延迟初始化：只在线程第一次请求时才创建实例
    if (tls_instance_ == nullptr) {
        // 使用 new 创建，因为它的生命周期需要由我们手动管理
        // (虽然我们在这个设计中没有手动 delete，依赖进程退出)
        tls_instance_ = new ThreadHeap();
    }
    return tls_instance_;
}


void* ThreadHeap::allocate(size_t size) {
    const size_t index = SizeClassInfo::map_size_to_index(size);
    BlockHeader* block_to_alloc = nullptr;

    if (index < kNumSizeClasses) {
        // 小对象分配路径
        if (free_lists_[index].head == nullptr) {
            if (!refill(index)) {
                return nullptr;
            }
        }
        assert(free_lists_[index].head != nullptr);
        
        block_to_alloc = free_lists_[index].head;
        free_lists_[index].head = block_to_alloc->next;
        free_lists_[index].count--;
        block_to_alloc->owner_group->block_in_used_count++;
    } else {
        // 大对象分配路径
        const size_t total_size_needed = size + sizeof(BlockHeader);
        const size_t num_pages = (total_size_needed + CentralHeap::kPageSize - 1) / CentralHeap::kPageSize;

        PageGroup* group = request_pages_from_central_heap(num_pages);
        if (group == nullptr) {
            return nullptr;
        }

        block_to_alloc = static_cast<BlockHeader*>(group->start_address);
        block_to_alloc->owner_group = group;
        group->page_count = num_pages;
        group->block_size = total_size_needed;
        group->total_block_count = 1;
        group->block_in_used_count = 1;
    }

    // 统一处理头部并链接到托管链表
    block_to_alloc->state = STATE_IN_USE;
    block_to_alloc->next = managed_list_head_;
    managed_list_head_ = block_to_alloc;

    return static_cast<void*>(block_to_alloc + 1);
}

void ThreadHeap::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    BlockHeader* header = static_cast<BlockHeader*>(ptr) - 1;
    atomic_store_release(&header->state, STATE_FREED);
}


void ThreadHeap::garbage_collect() {
    BlockHeader* current = managed_list_head_;
    BlockHeader* prev = nullptr;

    while (current != nullptr) {
        BlockHeader* next = current->next;

        if (atomic_load_acquire(&current->state) == STATE_FREED) {
            // 从托管链表中移除
            if (prev == nullptr) {
                managed_list_head_ = next;
            } else {
                prev->next = next;
            }

            PageGroup* owner_group = current->owner_group;
            assert(owner_group != nullptr);

            if (owner_group->block_size > 0) {
                // 回收小对象
                const size_t index = SizeClassInfo::map_size_to_index(owner_group->block_size);
                current->next = free_lists_[index].head;
                free_lists_[index].head = current;
                free_lists_[index].count++;

                owner_group->block_in_used_count--;
                               if (owner_group->block_in_used_count == 0 && 
                    free_lists_[index].count > owner_group->total_block_count) {

                    FreeList& list = free_lists_[index];
                    BlockHeader** indirect_head = &list.head;
                    size_t removed_count = 0;

                    while (*indirect_head != nullptr) {
                        if ((*indirect_head)->owner_group == owner_group) {
                            *indirect_head = (*indirect_head)->next;
                            removed_count++;
                        } else {
                            indirect_head = &((*indirect_head)->next);
                        }
                    }
                    list.count -= removed_count;
                    
                    release_pages_to_central_heap(owner_group);
                }
            } else {
                // 回收大对象
                release_pages_to_central_heap(owner_group);
            }
            current = next;
        } else {
            prev = current;
            current = next;
        }
    }
}

// =====================================================================
// 私有辅助函数实现 (Private Helper Implementation)
// =====================================================================

bool ThreadHeap::refill(size_t index) {
    assert(index < kNumSizeClasses);
    assert(free_lists_[index].head == nullptr);

    const size_t num_pages_to_acquire = SizeClassInfo::get_pages_to_acquire_for_index(index);
    const size_t block_size = SizeClassInfo::get_block_size_for_index(index);
    assert(block_size > sizeof(BlockHeader));

    PageGroup* group = request_pages_from_central_heap(num_pages_to_acquire);
    if (group == nullptr) {
        return false;
    }

    group->block_size = block_size;
    group->page_count = num_pages_to_acquire;
    
    char* start = static_cast<char*>(group->start_address);
    const size_t total_bytes = group->page_count * CentralHeap::kPageSize;
    const size_t num_blocks = total_bytes / block_size;
    
    group->total_block_count = num_blocks;
    group->block_in_used_count = 0;

    BlockHeader* current_list_head = nullptr;
    for (size_t i = 0; i < num_blocks; ++i) {
        char* block_start = start + i * block_size;
        BlockHeader* header = reinterpret_cast<BlockHeader*>(block_start);
        
        header->state = STATE_FREED; // 新块初始为空闲
        header->owner_group = group;

        // 利用 next 临时串联空闲块
        header->next = current_list_head;
        current_list_head = header;
    }

    free_lists_[index].head = current_list_head;
    free_lists_[index].count = num_blocks;

    return true;
}



PageGroup* ThreadHeap::request_pages_from_central_heap(size_t num_pages) {
    return CentralHeap::GetInstance().acquire_pages(num_pages);
}

void ThreadHeap::release_pages_to_central_heap(PageGroup* group) {
    CentralHeap::GetInstance().release_pages(group);
}
