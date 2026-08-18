mwfl_add_process_example(mwfl_process_supervisor main.cpp)
add_test(NAME mwfl.process_supervisor.self_test COMMAND mwfl_process_supervisor --self-test)
set_tests_properties(mwfl.process_supervisor.self_test PROPERTIES TIMEOUT 15)
