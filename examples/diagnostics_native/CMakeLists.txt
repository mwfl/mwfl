mwfl_add_diagnostics_example(mwfl_diagnostics_native main.cpp)
add_test(NAME mwfl.diagnostics_native.self_test COMMAND mwfl_diagnostics_native --self-test)
set_tests_properties(mwfl.diagnostics_native.self_test PROPERTIES TIMEOUT 10)
