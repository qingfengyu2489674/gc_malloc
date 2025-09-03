# src/CMakeLists.txt

# 将 calculator.cpp 编译成一个名为 "mylib" 的静态库
add_library(mylib STATIC
    calculator.cpp
)

# 告诉 CMake 这个库的头文件在哪里。
# 使用 PUBLIC 意味着，任何链接了 mylib 的目标（比如我们的测试程序）
# 也会自动获得这个头文件搜索路径。
target_include_directories(mylib PUBLIC
    ../include
)