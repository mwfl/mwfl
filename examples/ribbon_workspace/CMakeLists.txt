find_program(MWFL_RIBBON_UICC NAMES uicc.exe HINTS
    "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x86")
if(NOT MWFL_RIBBON_UICC)
    message(WARNING "uicc.exe not found; Ribbon reference application is unavailable")
    return()
endif()
set(ribbon_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(ribbon_binary "${ribbon_generated}/ribbon.bml")
set(ribbon_resource "${ribbon_generated}/ribbon.rc")
set(ribbon_header "${ribbon_generated}/ribbon.h")
add_custom_command(
    OUTPUT "${ribbon_binary}" "${ribbon_resource}" "${ribbon_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ribbon_generated}"
    COMMAND "${MWFL_RIBBON_UICC}" "${CMAKE_CURRENT_SOURCE_DIR}/ribbon.ribbon"
        "${ribbon_binary}" "/header:${ribbon_header}" "/res:${ribbon_resource}"
        /name:MWFL_RIBBON_REFERENCE /W2
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ribbon.ribbon"
    VERBATIM)
mwfl_add_example(mwfl_ribbon_workspace main.cpp "${ribbon_resource}")
target_link_libraries(mwfl_ribbon_workspace PRIVATE mwfl::ribbon mwfl::shell)
install(TARGETS mwfl_ribbon_workspace RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
