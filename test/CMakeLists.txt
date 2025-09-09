set(ALL_TESTS
  test_extern
  test_macros
  test_packages
  test_peerdeps
  test_modules
  test_incdir
)

foreach(test_name ${ALL_TESTS})
  set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/${test_name})
  add_test(
    NAME ${test_name}
    WORKING_DIRECTORY ${test_dir}
    COMMAND
        bash -c "$<TARGET_FILE:prunefl> -C list.f --top top_module --include-dirs > ${CMAKE_BINARY_DIR}/${test_name}.f && ${CMAKE_CURRENT_SOURCE_DIR}/verify_unused.py --unused ./unused.txt ${CMAKE_BINARY_DIR}/${test_name}.f && $<TARGET_FILE:slang_hier> -C ${CMAKE_BINARY_DIR}/${test_name}.f --top top_module"
  )
endforeach()
