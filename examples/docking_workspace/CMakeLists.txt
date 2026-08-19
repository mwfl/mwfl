mwfl_add_example(mwfl_docking_workspace_demo main.cpp application.cpp application.h)
install(TARGETS mwfl_docking_workspace_demo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
