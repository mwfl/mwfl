mwfl_add_core_example(mwfl_core_foundation main.cpp)
add_test(NAME mwfl.core_foundation.self_test COMMAND mwfl_core_foundation --self-test)
set_tests_properties(mwfl.core_foundation.self_test PROPERTIES TIMEOUT 10)
