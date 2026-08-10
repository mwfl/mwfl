mwfl_add_example(mwfl_graphics_interop main.cpp)
target_link_libraries(mwfl_graphics_interop PRIVATE mwfl::graphics)
install(TARGETS mwfl_graphics_interop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
