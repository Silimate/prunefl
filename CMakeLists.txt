# Top-level
cmake_minimum_required(VERSION 3.30)
project(prunefl)

# Submodules and Third Party
add_subdirectory(third_party/PicoSHA2)
add_subdirectory(third_party/json)
add_subdirectory(third_party/ordered-map)
set(SLANG_INCLUDE_TOOLS ON)
add_subdirectory(third_party/slang)
set_target_properties(
  slang_slang
  # otherwise there's a huge linker warning dump on macOS that obscures any
  # actual issues with the codebase
  PROPERTIES 
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
)
# Version
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/version.cc"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMAND cat "${PROJECT_SOURCE_DIR}/VERSION" | python3 "${PROJECT_SOURCE_DIR}/misc/file2utf8z.py" VERSION > "${CMAKE_CURRENT_BINARY_DIR}/version.cc"
    MAIN_DEPENDENCY "${PROJECT_SOURCE_DIR}/VERSION"
)

# Products
add_executable(prunefl src/main.cc src/driver.cc "${CMAKE_CURRENT_BINARY_DIR}/version.cc")
set_property(TARGET prunefl PROPERTY CXX_STANDARD 20)
target_include_directories(prunefl PUBLIC include)
target_include_directories(prunefl PRIVATE third_party/access_private_20/include)
target_link_libraries(prunefl PUBLIC slang::slang tsl::ordered_map nlohmann_json::nlohmann_json picosha2)
# target_compile_options(prunefl PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
# target_link_options(prunefl PRIVATE -fsanitize=address)
enable_testing()
add_subdirectory(test)
