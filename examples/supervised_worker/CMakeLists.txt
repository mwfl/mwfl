mwfl_add_supervised_worker_example(mwfl_supervised_worker main.cpp protocol.cpp)
add_test(NAME mwfl.supervised_worker.self_test COMMAND mwfl_supervised_worker --self-test)
set_tests_properties(mwfl.supervised_worker.self_test PROPERTIES TIMEOUT 20)
