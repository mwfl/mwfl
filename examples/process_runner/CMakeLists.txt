mwfl_add_process_example(mwfl_process_runner main.cpp)
add_test(NAME mwfl.process_runner.self_test COMMAND mwfl_process_runner --self-test)
set_tests_properties(mwfl.process_runner.self_test PROPERTIES TIMEOUT 10)
