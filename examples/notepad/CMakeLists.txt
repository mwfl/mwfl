mwtl_add_example(mwtl_notepad main.cpp)
target_link_libraries(mwtl_notepad PRIVATE oleacc)
install(TARGETS mwtl_notepad
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
