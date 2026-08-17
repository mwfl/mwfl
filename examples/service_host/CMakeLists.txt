mwfl_add_service_example(mwfl_service_host main.cpp)
add_test(NAME mwfl.service_host.self_test COMMAND mwfl_service_host --self-test)
set_tests_properties(mwfl.service_host.self_test PROPERTIES TIMEOUT 10)
