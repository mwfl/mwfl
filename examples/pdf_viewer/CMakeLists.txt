mwfl_add_example(mwfl_pdf_viewer main.cpp)
target_link_libraries(mwfl_pdf_viewer PRIVATE mwfl::webview2 shlwapi)

install(TARGETS mwfl_pdf_viewer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT pdf_viewer)
