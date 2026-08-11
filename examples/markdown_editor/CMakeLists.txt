include(FetchContent)
set(MD4C_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MD4C_BUILD_MD2HTML_EXECUTABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mwfl_markdown_md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG 729e6b8b320caa96328968ab27d7db2235e4fb47)
# md4c is an implementation detail of this example. Add it as an excluded
# source tree so its project-wide install rules do not leak into MWFL packages;
# the application component below stages only the runtime and required licenses.
cmake_policy(PUSH)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(mwfl_markdown_md4c)
if(NOT mwfl_markdown_md4c_POPULATED)
    FetchContent_Populate(mwfl_markdown_md4c)
    add_subdirectory("${mwfl_markdown_md4c_SOURCE_DIR}"
        "${mwfl_markdown_md4c_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()
cmake_policy(POP)

mwfl_add_example(mwfl_markdown_editor
    main.cpp
    markdown_syntax.cpp
    markdown_syntax.h
    markdown_renderer.cpp
    markdown_renderer.h
    markdown_editor.rc
    resource.h
    mwfl-mark.ico)
target_link_libraries(mwfl_markdown_editor PRIVATE
    mwfl::scintilla mwfl::webview2 md4c-html)
target_include_directories(mwfl_markdown_editor PRIVATE
    "${mwfl_markdown_md4c_SOURCE_DIR}/src")
mwfl_deploy_scintilla(mwfl_markdown_editor)
mwfl_deploy_lexilla(mwfl_markdown_editor)

# The flagship example can be staged as a self-contained application component:
# cmake --install <build> --config Release --component markdown_editor --prefix <dir>
install(TARGETS mwfl_markdown_editor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT markdown_editor)
install(FILES "${MWFL_SCINTILLA_RUNTIME}" "${MWFL_LEXILLA_RUNTIME}"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT markdown_editor)
install(FILES "${mwfl_scintilla_binary_SOURCE_DIR}/License.txt"
    DESTINATION "licenses/scintilla-lexilla"
    COMPONENT markdown_editor)
install(FILES "${mwfl_markdown_md4c_SOURCE_DIR}/LICENSE.md"
    DESTINATION "licenses/md4c"
    COMPONENT markdown_editor)
