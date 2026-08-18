mwfl_add_security_example(mwfl_security_credentials main.cpp)
add_test(NAME mwfl.security_credentials.self_test COMMAND mwfl_security_credentials --self-test)
set_tests_properties(mwfl.security_credentials.self_test PROPERTIES TIMEOUT 10)
