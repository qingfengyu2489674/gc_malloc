// 文件: src/sys/mman.cpp

// 关键：整个文件的内容只在非 TSan 模式下才生效
#if !defined(__SANITIZE_THREAD__)

#include "gc_malloc/sys/mman.hpp" // 包含我们即将修改的头文件中的函数声明
#include "gc_malloc/sys/syscall.hpp" // 包含 SYSCALL 宏的实现

// 提供 mmap 函数的定义（实现）
// 注意：这里不再有 static inline
void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    long ret = SYSCALL6(__NR_mmap, addr, length, prot, flags, fd, offset);
    // 正确的错误处理
    if (ret < 0 && ret > -4096) { 
        errno = -ret;
        return MAP_FAILED;
    }
    return reinterpret_cast<void*>(ret);
}

// 提供 munmap 函数的定义（实现）
int munmap(void* addr, size_t length) {
    long ret = SYSCALL2(__NR_munmap, addr, length);
    if (ret != 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

#endif // !__SANITIZE_THREAD__