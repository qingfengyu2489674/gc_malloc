#include "gc_malloc/AlignedMmapper.hpp"
#include "gc_malloc/sys/mman.hpp"
#include <cassert>
#include <cstdint>


// =====================================================================
// 公共接口实现
// =====================================================================

void* AlignedMmapper::allocate_aligned(size_t size) {

    // (size & (size - 1)) == 0 是一个判断是否为2的幂的技巧。
    assert(size > 0 && (size & (size - 1)) == 0);

    const size_t over_alloc_size = size * 2;
    void* raw_ptr = mmap(
        nullptr,
        over_alloc_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if(raw_ptr == MAP_FAILED) {
        return nullptr;
    }

    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw_ptr);

    uintptr_t aligned_addr = (raw_addr + size -1) &  ~(size - 1);

    void* aligned_ptr = reinterpret_cast<void*>(aligned_addr);

    size_t head_trim_size = aligned_addr - raw_addr;
    if(head_trim_size > 0) {
        munmap(raw_ptr, head_trim_size);
    }

    uintptr_t raw_end_addr = raw_addr + over_alloc_size;
    uintptr_t aligned_end_addr = aligned_addr + size;

    size_t tail_trim_size = raw_end_addr - aligned_end_addr;
    if(tail_trim_size > 0) {
        char* start_of_tail = static_cast<char*>(aligned_ptr);
        munmap(start_of_tail + size, tail_trim_size);
    }

    return aligned_ptr;

}



void AlignedMmapper::deallocate_aligned(void* ptr, size_t size) {
    if (ptr == nullptr || size == 0) {
        return;
    }
    
    munmap(ptr, size);
}