mwfl_add_example(mwfl_document_workspace main.cpp application.cpp application.h)
install(TARGETS mwfl_document_workspace
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
