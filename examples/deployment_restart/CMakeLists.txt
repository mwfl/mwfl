mwfl_add_deployment_example(mwfl_deployment_restart main.cpp)
add_test(NAME mwfl.deployment_restart.self_test COMMAND mwfl_deployment_restart --self-test)
set_tests_properties(mwfl.deployment_restart.self_test PROPERTIES TIMEOUT 10)
