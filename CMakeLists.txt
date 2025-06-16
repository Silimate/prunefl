# Top-level
cmake_minimum_required(VERSION 4.0)
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
include_directories(third_party third_party/access_private_20/include)
add_executable(nodo src/main.cc)
set_property(TARGET nodo PROPERTY CXX_STANDARD 20)
target_link_libraries(nodo PUBLIC slang_slang fmt::fmt argparse)
