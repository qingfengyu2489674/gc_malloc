# C++ Project Template

这是一个现代化的、开箱即用的 C++ 项目模板，旨在帮助您快速启动新的 C++ 项目。它集成了 CMake 用于构建系统，以及 GoogleTest 用于单元测试。

## ✨ 特性

- **现代化的 CMake**：使用现代 CMake 实践 (`target_*` 命令)，确保项目结构清晰、可维护。
- **开箱即用的单元测试**：集成了 GoogleTest 框架，通过 CMake 的 `FetchContent` 自动下载和配置，无需手动安装。
- **清晰的目录结构**：将接口 (`include`)、实现 (`src`) 和测试 (`tests`) 完全分离。
- **跨平台**：可在 Linux, macOS 和 Windows (配合 VS Code, MinGW/MSVC, WSL) 等多种环境下使用。
- **易于扩展**：添加新的源文件、库或可执行文件都非常简单。

## 📂 目录结构

```
.
├── CMakeLists.txt      # 根 CMakeLists，项目总控
├── include/            # 存放头文件 (.hpp)
│   └── mylib/
│       └── calculator.hpp
├── src/                # 存放源文件 (.cpp)
│   ├── CMakeLists.txt
│   └── calculator.cpp
└── tests/              # 存放测试文件 (test_*.cpp)
    ├── CMakeLists.txt
    └── test_calculator.cpp
```

## 🚀 快速开始

### 环境要求

- C++ 编译器 (GCC, Clang, MSVC 等)
- CMake (版本 >= 3.14)
- Git

### 构建步骤

标准的 CMake 构建流程如下。所有命令都在项目根目录下执行。

**1. 创建并进入构建目录**

我们采用外部构建 (out-of-source build)，这是一种最佳实践，可以保持源代码目录的干净。

```bash
mkdir build
cd build
```

**2. 配置项目 (Configure)**

运行 `cmake` 来配置项目。在这一步，CMake 会自动下载 GoogleTest。首次运行时需要网络连接。

```bash
cmake ..
```

**3. 编译项目 (Build)**

使用 CMake 的构建命令来编译您的库和测试程序。

```bash
cmake --build .
```
或者，在 Linux/macOS 上，你也可以直接使用 `make`：
```bash
make
```

**4. 运行测试 (Test)**

我们使用 `ctest`（CMake 自带的测试运行器）来执行所有测试用例。

```bash
ctest --verbose
```
如果所有测试都通过，您会看到 `100% tests passed` 的消息。

## 📝 如何扩展项目

扩展此模板非常简单。以下是添加新源文件和测试文件的步骤。

### 场景：添加一个新的 `AdvancedMath` 类

假设我们要添加一个新功能，包含 `advanced_math.hpp` 和 `advanced_math.cpp` 两个文件。

#### 1. 添加头文件

在 `include/mylib/` 目录下创建新文件 `advanced_math.hpp`。

```cpp
// include/mylib/advanced_math.hpp
#pragma once

class AdvancedMath {
public:
    long long multiply(int a, int b);
};
```

#### 2. 添加源文件

在 `src/` 目录下创建新文件 `advanced_math.cpp`。

```cpp
// src/advanced_math.cpp
#include "mylib/advanced_math.hpp"

long long AdvancedMath::multiply(int a, int b) {
    return static_cast<long long>(a) * b;
}
```

#### 3. 修改 `src/CMakeLists.txt`

**这是关键一步！** 打开 `src/CMakeLists.txt` 文件，将新的 `.cpp` 文件添加到 `add_library` 命令中。

```cmake
# src/CMakeLists.txt

add_library(mylib STATIC
    calculator.cpp
    advanced_math.cpp  # <-- 在这里添加新的一行
)

target_include_directories(mylib PUBLIC
    ../include
)
```

### 场景：为 `AdvancedMath` 类添加测试

现在，我们为刚刚添加的新功能编写单元测试。

#### 1. 创建测试文件

在 `tests/` 目录下创建一个新的测试文件，例如 `test_advanced_math.cpp`。

```cpp
// tests/test_advanced_math.cpp
#include <gtest/gtest.h>
#include "mylib/advanced_math.hpp" // 包含你要测试的头文件

TEST(AdvancedMathTest, ShouldMultiplyNumbers) {
    AdvancedMath math;
    ASSERT_EQ(math.multiply(5, 10), 50);
    ASSERT_EQ(math.multiply(-5, 10), -50);
}
```

#### 2. 修改 `tests/CMakeLists.txt`

**同样是关键一步！** 打开 `tests/CMakeLists.txt` 文件，将新的测试文件添加到 `add_executable` 命令中。

```cmake
# tests/CMakeLists.txt

add_executable(run_tests
    test_calculator.cpp
    test_advanced_math.cpp  # <-- 在这里添加新的一行
)

target_link_libraries(run_tests PRIVATE
    mylib
    gtest_main
)

include(GoogleTest)
gtest_discover_tests(run_tests)
```

### 3. 重新构建并测试

在你添加完所有文件并修改完 `CMakeLists.txt` 之后，回到 `build` 目录，重新运行构建和测试命令即可。

```bash
# 确保在 build 目录中
cd build

# 重新编译
make 
# 或者 cmake --build .

# 运行所有测试 (包括你新添加的)
ctest --verbose
```
`make` 会自动检测到 `CMakeLists.txt` 的改动并重新运行必要的配置步骤，然后编译你的新文件。`ctest` 会自动发现并运行你新添加的测试用例。
