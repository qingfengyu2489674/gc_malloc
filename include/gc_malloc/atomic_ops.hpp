#ifndef GC_MALLOC_ATOMIC_OPS_HPP
#define GC_MALLOC_ATOMIC_OPS_HPP

#include <cstdint>


static inline void atomic_store_release(volatile uintptr_t* atomic_ptr, uintptr_t value) {
#if defined(__GNUC__) || defined(__clang__)
    // 使用 GCC/Clang 提供的现代原子内置函数
    __atomic_store_n(atomic_ptr, value, __ATOMIC_RELEASE);
#else
    // 对于不支持的编译器（如旧版 MSVC），我们退化为简单的 volatile 写入。
    // 注意：这在这些编译器上不提供跨核心的内存序保证！
    *atomic_ptr = value;
#endif
}

static inline uintptr_t atomic_load_acquire(const volatile uintptr_t* atomic_ptr) {
#if defined(__GNUC__) || defined(__clang__)
    // 使用 GCC/Clang 提供的现代原子内置函数
    return __atomic_load_n(atomic_ptr, __ATOMIC_ACQUIRE);
#else
    // 对于不支持的编译器，退化为简单的 volatile 读取。
    return *atomic_ptr;
#endif
}


#endif // GC_MALLOC_BASE_ATOMIC_OPS_HPP