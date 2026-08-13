mwfl_add_cli_example(mwfl_cli_basic main.cpp)
add_test(NAME mwfl.cli_basic.help COMMAND mwfl_cli_basic --help)
add_test(NAME mwfl.cli_basic_echo COMMAND mwfl_cli_basic echo foundation)
