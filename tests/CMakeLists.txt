# tests/CMakeLists.txt

# 创建一个名为 "run_tests" 的可执行文件
add_executable(run_tests
    test_calculator.cpp
)

# 链接测试程序。它需要链接我们自己的库 (mylib) 和 GoogleTest (gtest_main)
# gtest_main 已经包含了 main 函数，非常方便
target_link_libraries(run_tests PRIVATE
    mylib
    gtest_main
)

# 使用 GoogleTest 的 CMake 模块来自动发现所有测试
# 并将它们添加到 CTest 中
include(GoogleTest)
gtest_discover_tests(run_tests)