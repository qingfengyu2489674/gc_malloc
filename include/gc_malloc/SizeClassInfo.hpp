#ifndef GC_MALLOC_SIZE_CLASS_INFO_HPP
#define GC_MALLOC_SIZE_CLASS_INFO_HPP

#include <cstddef>


static constexpr size_t kNumSizeClasses = 17;

class SizeClassInfo {
public:
    static size_t map_size_to_index(size_t size);
    static size_t get_block_size_for_index(size_t index);
    static size_t get_pages_to_acquire_for_index(size_t index);
};


#endif // GC_MALLOC_SIZE_CLASS_INFO_HPP