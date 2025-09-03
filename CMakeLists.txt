# CMakeLists.txt (根目录)

# 1. 项目基本设置
cmake_minimum_required(VERSION 3.14)
project(CppProjectTemplate LANGUAGES CXX)

# 推荐设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 启用测试
# 这一行告诉 CMake，这个项目包含测试，之后可以用 ctest 命令运行
enable_testing()

# 3. 引入并配置 GoogleTest
# 引入 FetchContent 模块，这是 CMake 自动下载依赖的核心
include(FetchContent)

# 声明 GoogleTest 作为一个依赖项
FetchContent_Declare(
  googletest
  # 指定 Git 仓库地址
  GIT_REPOSITORY https://github.com/google/googletest.git
  # 指定一个稳定版本，避免未来更新破坏你的构建
  GIT_TAG        release-1.12.1
)

# 下载并配置 GoogleTest。CMake 会在需要时自动执行此操作。
# 这会创建 gtest, gtest_main 等可链接的目标
FetchContent_MakeAvailable(googletest)

# 4. 包含子目录
# 让 CMake 去处理 src 和 tests 目录下的 CMakeLists.txt 文件
add_subdirectory(src)
add_subdirectory(tests)