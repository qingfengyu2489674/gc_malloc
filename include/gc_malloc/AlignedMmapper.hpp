#ifndef ALIGNED_MAPPER_H
#define ALIGNED_MAPPER_H

#include <cstddef>

/**
 * @brief AlignedMmapper 是一个 mmap 系统调用的底层封装。
 *
 * 它的唯一职责是向操作系统申请指定大小的、并保证按该大小对齐的
 * 虚拟内存区域。它不关心上层如何使用这块内存。
 *
 * 这是一个线程安全的单例。
 */
class AlignedMmapper {
public:
    static void* allocate_aligned(size_t size);
    static void deallocate_aligned(void* ptr, size_t size);

private:
    AlignedMmapper() = delete;
    ~AlignedMmapper() = delete;
    AlignedMmapper(const AlignedMmapper&) = delete;
    AlignedMmapper& operator=(const AlignedMmapper&) = delete;
};

#endif // ALIGNED_MAPPER_H