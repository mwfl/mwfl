mwtl_add_example(mwtl_mdi_workspace main.cpp)
target_link_libraries(mwtl_mdi_workspace PRIVATE mwtl::mdi)
install(TARGETS mwtl_mdi_workspace RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
