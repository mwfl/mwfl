mwfl_add_diagnostics_example(mwfl_diagnostics_pipeline main.cpp)
add_test(NAME mwfl.diagnostics_pipeline.self_test COMMAND mwfl_diagnostics_pipeline --self-test)
set_tests_properties(mwfl.diagnostics_pipeline.self_test PROPERTIES TIMEOUT 10)
