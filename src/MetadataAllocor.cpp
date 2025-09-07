#include "gc_malloc/MetadataAllocor.hpp"
#include "gc_malloc/PageGroup.hpp"
#include "gc_malloc/AlignedMmapper.hpp"

#include <cassert> 

// =====================================================================
// 单例模式实现 (Singleton Implementation)
// =====================================================================

MetadataAllocator& MetadataAllocator::GetInstance() {
    // C++11 保证了静态局部变量的初始化是线程安全的
    static MetadataAllocator instance;
    return instance;
}

// =====================================================================
// 构造与析构 (Constructor & Destructor)
// =====================================================================

MetadataAllocator::MetadataAllocator() {
    // 构造置空
}

MetadataAllocator::~MetadataAllocator() {
    // 析构置空
    // 如果未来需要归还mmap出的内存，逻辑将写在这里。
}


// =====================================================================
// 公共接口实现 (Public API Implementation)
// =====================================================================

void* MetadataAllocator::allocate(size_t size) {

    assert(size == sizeof (PageGroup));     // 确保调用者请求正确的大小
    (void)size;                             // 消除“未使用参数”的警告

    std::lock_guard<std::mutex> lock(mutex_);

    if(free_list_ == nullptr) {
        bool success = refill_free_list();
        if(!success) {
            // 系统资源耗尽，直接返回
            return nullptr;
        }
    }

    assert(free_list_ != nullptr);

    void* block_to_return = free_list_;
    free_list_ = *(static_cast<void**>(block_to_return));

    allocated_objects_count_++;
    return block_to_return;
}

void MetadataAllocator::deallocate(void* ptr, size_t size) {

    assert(size == sizeof(PageGroup));
    (void)size;

    if(ptr == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    *(static_cast<void**>(ptr)) = free_list_;
    free_list_ = ptr;

    allocated_objects_count_--;
}



// =====================================================================
// 私有辅助函数实现 (Private Helper Implementation)
// =====================================================================

bool MetadataAllocator::refill_free_list() {

    void* new_chunk_mem = AlignedMmapper::allocate_aligned(kChunkSize);

    chunks_acquired_++;

    Chunk* new_chunk = static_cast<Chunk*>(new_chunk_mem);
    new_chunk->next = chunk_list_;
    chunk_list_ = new_chunk;

    const uintptr_t chunk_start = reinterpret_cast<uintptr_t>(new_chunk_mem);
    const uintptr_t allocatable_start = chunk_start + sizeof(Chunk);
    const uintptr_t allocatable_end = chunk_start + kChunkSize;

    const size_t block_size = sizeof(PageGroup);

    assert(block_size >= sizeof(void*));

    for(uintptr_t current = allocatable_start; current + block_size <= allocatable_end; current += block_size)
    {
        void* block = reinterpret_cast<void*>(current);

        *(static_cast<void**>(block)) = free_list_;
        free_list_ =block;
    }

    return true; // 占位符
}