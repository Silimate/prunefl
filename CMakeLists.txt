# Top-level
project(nodo)
cmake_minimum_required(VERSION 4.0)

# Submodules
# option(FMT_INSTALL OFF)
# FetchContent_Declare(
#     fmt
#     EXCLUDE_FROM_ALL
#     SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/fmt
# )

add_subdirectory(third_party/slang)

# Products
include_directories(third_party)
add_executable(nodo src/main.cc)
set_property(TARGET nodo PROPERTY CXX_STANDARD 20)
target_link_libraries(nodo PUBLIC slang_slang fmt::fmt)
