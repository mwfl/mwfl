mwtl_add_example(mwtl_graphics_interop main.cpp)
target_link_libraries(mwtl_graphics_interop PRIVATE mwtl::graphics)
install(TARGETS mwtl_graphics_interop RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
