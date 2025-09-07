#ifndef METADATA_ALLOC_H
#define METADATA_ALLOC_H

#include <cstddef> 
#include <mutex> 

struct PageGroup;

class MetadataAllocator {

public:

    static MetadataAllocator& GetInstance();

    // 内部设计是高度特化的，size参数并无作用，这里保留只是为了接口统一性
    void* allocate(size_t size);

    void deallocate(void* ptr, size_t size);
    
private:

    MetadataAllocator();
    ~MetadataAllocator();
    MetadataAllocator(const MetadataAllocator&) = delete;
    MetadataAllocator& operator=(const MetadataAllocator&) = delete;

    bool refill_free_list();

private:

    struct Chunk {
        Chunk* next;
        // 可以补充Chunk其他的元数据
    };

    std::mutex mutex_;

    void* free_list_ = nullptr;
    Chunk* chunk_list_ = nullptr;

    static constexpr size_t kChunkSize = 1 * 1024 * 1024;

    size_t allocated_objects_count_ = 0;
    size_t chunks_acquired_ = 0;
};


#endif // METADATA_ALLOC_H