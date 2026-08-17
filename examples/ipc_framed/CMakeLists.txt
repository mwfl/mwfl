mwfl_add_ipc_example(mwfl_ipc_framed main.cpp)
add_test(NAME mwfl.ipc_framed.self_test COMMAND mwfl_ipc_framed --self-test)
set_tests_properties(mwfl.ipc_framed.self_test PROPERTIES TIMEOUT 10)
