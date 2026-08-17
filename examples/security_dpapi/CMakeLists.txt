mwfl_add_security_example(mwfl_security_dpapi main.cpp)
add_test(NAME mwfl.security_dpapi.self_test COMMAND mwfl_security_dpapi --self-test)
set_tests_properties(mwfl.security_dpapi.self_test PROPERTIES TIMEOUT 10)
