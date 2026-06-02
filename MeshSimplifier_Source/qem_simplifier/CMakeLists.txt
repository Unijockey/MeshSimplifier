cmake_minimum_required(VERSION 3.15)
project(qem_simplifier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── 自动获取 pybind11 (无需手动安装) ───────────────────

include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.12.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pybind11)

# ─── Python 模块 (.pyd) ─────────────────────────────────

pybind11_add_module(qem_simplifier
    src/pybind_module.cpp
)

target_include_directories(qem_simplifier PRIVATE src)

# ─── 可选: 独立 CLI 工具 ────────────────────────────────

add_executable(qem_cli
    src/cli_main.cpp
)
target_include_directories(qem_cli PRIVATE src)
set_target_properties(qem_cli PROPERTIES
    OUTPUT_NAME "qem_simplifier"
    SUFFIX ".exe"
)

# ─── 安装 ────────────────────────────────────────────────

install(TARGETS qem_simplifier
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)
