mwfl_add_cli_example(mwfl_cli_worker main.cpp)
add_test(NAME mwfl.cli_worker.self_test COMMAND mwfl_cli_worker --self-test)
set_tests_properties(mwfl.cli_worker.self_test PROPERTIES TIMEOUT 10)
