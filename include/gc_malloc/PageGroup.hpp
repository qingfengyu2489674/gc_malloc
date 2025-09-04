#ifndef PAGE_GROUP_H
#define PAGE_GROUP_H

#include <cstddef>

struct PageGroup
{
    void* start_address;        // 该结构所描述的内存起始地址
    size_t page_count;          // 内存所占的页数
    size_t block_size;          // 内存要切分出的块大小
    int total_block_count;      // 切分出的总体的块数量
    int block_in_used_count;    // 分配出去的块数量
};


#endif // PAGE_GROUP_H