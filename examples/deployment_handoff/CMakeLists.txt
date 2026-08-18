mwfl_add_deployment_example(mwfl_deployment_handoff main.cpp)
add_test(NAME mwfl.deployment_handoff.self_test COMMAND mwfl_deployment_handoff --self-test)
set_tests_properties(mwfl.deployment_handoff.self_test PROPERTIES TIMEOUT 10)
