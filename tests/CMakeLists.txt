add_executable(test_smoke test_smoke.c)
target_link_libraries(test_smoke PRIVATE p8core)
add_test(NAME smoke COMMAND test_smoke)
