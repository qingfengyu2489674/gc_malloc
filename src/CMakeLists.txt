# src/CMakeLists.txt

# 将 calculator.cpp 编译成一个名为 "gc_malloc" 的静态库
add_library(gc_malloc STATIC
    MetadataAllocor.cpp
    Bitmap.cpp
    AlignedMmapper.cpp
    CentralHeap.cpp
    SizeClassInfo.cpp
    ThreadHeap.cpp
)

# 检查 CXX 编译标志中是否不包含 "-fsanitize=thread"
if(NOT CMAKE_CXX_FLAGS MATCHES "-fsanitize=thread")
    # 如果是正常编译（非 TSan 模式），
    # 则将我们自己实现的 mman.cpp 添加到 gc_malloc 库的源文件中。
    message(STATUS "Normal build detected. Compiling custom mman.cpp.")
    target_sources(gc_malloc PRIVATE 
        sys/mman.cpp 
    )
else()
    # 如果是 TSan 模式，则不编译 mman.cpp，
    # 这样链接器就会使用 TSan 提供的标准库版本。
    message(STATUS "ThreadSanitizer build detected. Skipping custom mman.cpp.")
endif()

# 告诉 CMake 这个库的头文件在哪里。
# 使用 PUBLIC 意味着，任何链接了 gc_malloc 的目标（比如我们的测试程序）
# 也会自动获得这个头文件搜索路径。
target_include_directories(gc_malloc PUBLIC
    ../include
)