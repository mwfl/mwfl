mwfl_add_example(mwfl_mdi_workspace main.cpp)
target_link_libraries(mwfl_mdi_workspace PRIVATE mwfl::mdi)
install(TARGETS mwfl_mdi_workspace RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
