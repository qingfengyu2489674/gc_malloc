# --- src/CMakeLists.txt ---

# 定义源文件列表
set(GC_MALLOC_SOURCES
    MetadataAllocor.cpp
    Bitmap.cpp
    AlignedMmapper.cpp
    CentralHeap.cpp
    SizeClassInfo.cpp
    ThreadHeap.cpp
)

# 1. 将源文件编译成一个名为 "gc_malloc" 的静态库
add_library(gc_malloc STATIC ${GC_MALLOC_SOURCES})


# 2. 为 gc_malloc 目标指定头文件搜索路径

# 使用 PUBLIC 关键字意味着，任何链接了 gc_malloc 的目标（比如我们的测试程序）
# 也会自动获得这些头文件搜索路径。

target_include_directories(gc_malloc PUBLIC
    # 修正点：使用 ${PROJECT_SOURCE_DIR} 来确保路径是绝对且正确的。
    # 这会指向你项目的根目录下的 include 文件夹。
    "${PROJECT_SOURCE_DIR}/include"

    # 【解决 TSAN 问题的关键】
    # 添加包含 sanitizer 头文件的系统目录。
    # 这条路径已经是绝对路径，所以保持原样即可。
    "/usr/lib/gcc/x86_64-linux-gnu/11/include"
)