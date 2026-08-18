mwfl_add_service_example(mwfl_service_management main.cpp)
add_test(NAME mwfl.service_management.self_test COMMAND mwfl_service_management --self-test)
set_tests_properties(mwfl.service_management.self_test PROPERTIES TIMEOUT 10)
