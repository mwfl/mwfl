mwfl_add_scheduler_example(mwfl_scheduled_task main.cpp)
add_test(NAME mwfl.scheduled_task.self_test COMMAND mwfl_scheduled_task --self-test)
set_tests_properties(mwfl.scheduled_task.self_test PROPERTIES TIMEOUT 10)
