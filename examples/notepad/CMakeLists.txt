mwfl_add_example(mwfl_notepad main.cpp)
target_link_libraries(mwfl_notepad PRIVATE oleacc)
install(TARGETS mwfl_notepad
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT notepad)
