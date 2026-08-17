mwfl_add_foundation_overview_example(mwfl_foundation_overview main.cpp)
add_test(NAME mwfl.foundation_overview.self_test COMMAND mwfl_foundation_overview --self-test)
set_tests_properties(mwfl.foundation_overview.self_test PROPERTIES TIMEOUT 10)
