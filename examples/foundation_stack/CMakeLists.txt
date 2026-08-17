mwfl_add_foundation_stack_example(mwfl_foundation_stack main.cpp)
add_test(NAME mwfl.foundation_stack.self_test COMMAND mwfl_foundation_stack --self-test)
set_tests_properties(mwfl.foundation_stack.self_test PROPERTIES TIMEOUT 15)
