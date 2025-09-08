#ifndef GC_MALLOC_BLOCK_HEADER_HPP
#define GC_MALLOC_BLOCK_HEADER_HPP

#include <cstdint> // for uintptr_t


class PageGroup;

struct BlockHeader {
    volatile uintptr_t state;

    PageGroup* owner_group;

    BlockHeader* next;
};

// 确保在64位系统上，头部大小正好是24字节
static_assert(sizeof(BlockHeader) == 24, "BlockHeader size must be 24 bytes on a 64-bit system.");

// 定义状态常量
enum BlockState : uintptr_t {
    STATE_FREED = 0,
    STATE_IN_USE = 1
};


#endif // GC_MALLOC_BLOCK_HEADER_HPP