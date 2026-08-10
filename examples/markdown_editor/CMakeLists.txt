include(FetchContent)
set(MD4C_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MD4C_BUILD_MD2HTML_EXECUTABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mwtl_markdown_md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG 729e6b8b320caa96328968ab27d7db2235e4fb47)
FetchContent_MakeAvailable(mwtl_markdown_md4c)

mwtl_add_example(mwtl_markdown_editor
    main.cpp
    markdown_syntax.cpp
    markdown_syntax.h
    markdown_renderer.cpp
    markdown_renderer.h
    markdown_editor.rc
    resource.h
    mwtl-mark.ico)
target_link_libraries(mwtl_markdown_editor PRIVATE
    mwtl::scintilla mwtl::webview2 md4c-html)
target_include_directories(mwtl_markdown_editor PRIVATE
    "${mwtl_markdown_md4c_SOURCE_DIR}/src")
mwtl_deploy_scintilla(mwtl_markdown_editor)
mwtl_deploy_lexilla(mwtl_markdown_editor)

# The flagship example can be staged as a self-contained application component:
# cmake --install <build> --config Release --component markdown_editor --prefix <dir>
install(TARGETS mwtl_markdown_editor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT markdown_editor)
install(FILES "${MWTL_SCINTILLA_RUNTIME}" "${MWTL_LEXILLA_RUNTIME}"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT markdown_editor)
install(FILES "${mwtl_scintilla_binary_SOURCE_DIR}/License.txt"
    DESTINATION "licenses/scintilla-lexilla"
    COMPONENT markdown_editor)
install(FILES "${mwtl_markdown_md4c_SOURCE_DIR}/LICENSE.md"
    DESTINATION "licenses/md4c"
    COMPONENT markdown_editor)
