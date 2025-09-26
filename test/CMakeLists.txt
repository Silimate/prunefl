set(ALL_TESTS
  test_extern
  test_macros
  test_packages
  test_peerdeps
  test_modules
  test_incdir
  test_cli_define
  test_libmap
  test_timescales
)

foreach(test_name ${ALL_TESTS})
  set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/${test_name})
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${test_dir}
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/run_test
          ${test_name}
          ${CMAKE_BINARY_DIR}
          $<TARGET_FILE:prunefl>
          $<TARGET_FILE:slang_hier>
  )
endforeach()
