# Top-level
cmake_minimum_required(VERSION 3.30)
project(prunefl)

# Submodules and Third Party
add_subdirectory(third_party/slang)

# Version
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/version.cc"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMAND cat "${PROJECT_SOURCE_DIR}/VERSION" | python3 "${PROJECT_SOURCE_DIR}/misc/version_to_cpp.py" > "${CMAKE_CURRENT_BINARY_DIR}/version.cc"
    MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/VERSION"
)

# Products
add_executable(prunefl src/main.cc src/driver.cc src/file.cc "${CMAKE_CURRENT_BINARY_DIR}/version.cc")
set_property(TARGET prunefl PROPERTY CXX_STANDARD 20)
target_include_directories(prunefl PUBLIC include)
target_include_directories(prunefl PRIVATE third_party/access_private_20/include)
target_link_libraries(prunefl PUBLIC slang::slang)
