#include "gc_malloc/SizeClassInfo.hpp"
#include <cassert>


struct SizeClassData {
    size_t block_size;
    size_t pages_to_acquire;
};

static const SizeClassData g_size_class_table[kNumSizeClasses] = {
    {    32,      1 },
    {    48,      1 },
    {    64,      1 },
    {    80,      1 },
    {    96,      1 },
    {   112,      1 },
    {   128,      1 },
    {   192,      2 },
    {   256,      2 },
    {   384,      3 },
    {   512,      4 },
    {   768,      6 },
    {  1024,      8 },
    {  2048,     16 },
    {  4096,     32 },
    {  8192,     32 },
    { 16384,     32 }
};

size_t SizeClassInfo::map_size_to_index(size_t size) {
    assert(size > 0);
    
    for (size_t i = 0; i < kNumSizeClasses; ++i) {
        if (g_size_class_table[i].block_size >= size) {
            return i;
        }
    }

    return kNumSizeClasses;
}

size_t SizeClassInfo::get_block_size_for_index(size_t index) {
    assert(index < kNumSizeClasses);
    return g_size_class_table[index].block_size;
}

size_t SizeClassInfo::get_pages_to_acquire_for_index(size_t index) {
    assert(index < kNumSizeClasses);
    return g_size_class_table[index].pages_to_acquire;
}
