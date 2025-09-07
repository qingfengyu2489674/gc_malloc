
#ifndef GC_MALLOC_SYS_MMAN_HPP
#define GC_MALLOC_SYS_MMAN_HPP

#if defined(__SANITIZE_THREAD__)
    // --- TSan 开启时的路径 ---
    // 直接包含系统头文件，让 TSan 能够拦截 mmap/munmap。
    #include <sys/mman.h>

#else
    // --- 正常编译时的路径 ---
    // 在这里，我们只提供宏定义和函数声明。

#ifdef __cplusplus
extern "C" {
#endif

    #include <cstddef>
    #include <cerrno>
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/syscall.h> 

    // 宏定义保持不变
    #define PROT_READ       0x1
    #define PROT_WRITE      0x2
    #define PROT_EXEC       0x4
    #define PROT_NONE       0x0
    #define MAP_SHARED      0x01
    #define MAP_PRIVATE     0x02
    #define MAP_ANONYMOUS   0x20
    #define MAP_ANON        MAP_ANONYMOUS
    #define MAP_FAILED      (reinterpret_cast<void*>(-1))

    // 关键修改：只留下函数声明，去掉 static inline 和函数体
    void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
    int munmap(void* addr, size_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __SANITIZE_THREAD__

#endif // GC_MALLOC_SYS_MMAN_HPP