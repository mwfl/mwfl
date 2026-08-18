mwfl_add_ipc_example(mwfl_ipc_secure main.cpp)
add_test(NAME mwfl.ipc_secure.self_test COMMAND mwfl_ipc_secure --self-test)
set_tests_properties(mwfl.ipc_secure.self_test PROPERTIES TIMEOUT 15)
