# Top-level
cmake_minimum_required(VERSION 3.30)
project(nodo)

# Submodules and Third Party
include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

add_subdirectory(third_party/slang)


# Products
add_executable(nodo src/main.cc src/file.cc)
set_property(TARGET nodo PROPERTY CXX_STANDARD 20)
target_include_directories(nodo PUBLIC include)
target_include_directories(nodo PRIVATE third_party/access_private_20/include)
target_link_libraries(nodo PUBLIC slang_slang fmt::fmt argparse)
