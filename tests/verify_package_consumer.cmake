if(NOT DEFINED MWFL_BUILD_DIR OR NOT DEFINED MWFL_SOURCE_DIR OR
   NOT DEFINED MWFL_INSTALL_DIR OR NOT DEFINED MWFL_CONFIGURATION OR
   NOT DEFINED MWFL_GENERATOR OR NOT DEFINED MWFL_PLATFORM OR
   NOT DEFINED MWFL_EXPECT_NOTEPAD OR NOT DEFINED MWFL_EXPECT_SCINTILLA OR
   NOT DEFINED MWFL_EXPECT_WEBVIEW2)
    message(FATAL_ERROR "Package consumer verification arguments are incomplete")
endif()

file(REMOVE_RECURSE "${MWFL_INSTALL_DIR}" "${MWFL_BUILD_DIR}/package-consumer-build")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MWFL_BUILD_DIR}"
            --config "${MWFL_CONFIGURATION}" --prefix "${MWFL_INSTALL_DIR}"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "mwfl install failed: ${install_result}")
endif()
if(MWFL_EXPECT_NOTEPAD AND NOT EXISTS "${MWFL_INSTALL_DIR}/bin/mwfl_notepad.exe")
    message(FATAL_ERROR "installed package is missing bin/mwfl_notepad.exe")
endif()
if(MWFL_EXPECT_SCINTILLA AND NOT EXISTS "${MWFL_INSTALL_DIR}/bin/Scintilla.dll")
    message(FATAL_ERROR "installed package is missing pinned bin/Scintilla.dll")
endif()

foreach(method IN ITEMS subdirectory fetchcontent)
    set(method_build "${MWFL_BUILD_DIR}/package-${method}-build")
    file(REMOVE_RECURSE "${method_build}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -S "${MWFL_SOURCE_DIR}/tests/package_${method}"
                -B "${method_build}"
                -G "${MWFL_GENERATOR}" -A "${MWFL_PLATFORM}"
                "-DMWFL_SOURCE_DIR=${MWFL_SOURCE_DIR}"
                "-DMWFL_WTL_SOURCE_DIR=${MWFL_WTL_SOURCE_DIR}"
                "-DMWFL_WIL_SOURCE_DIR=${MWFL_WIL_SOURCE_DIR}"
                "-DMWFL_DEPENDENCY_MODE=SYSTEM"
        RESULT_VARIABLE method_configure_result)
    if(NOT method_configure_result EQUAL 0)
        message(FATAL_ERROR "${method} consumer configure failed: ${method_configure_result}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${method_build}"
                --config "${MWFL_CONFIGURATION}"
        RESULT_VARIABLE method_build_result)
    if(NOT method_build_result EQUAL 0)
        message(FATAL_ERROR "${method} consumer build failed: ${method_build_result}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${MWFL_SOURCE_DIR}/tests/package_consumer"
            -B "${MWFL_BUILD_DIR}/package-consumer-build"
            -G "${MWFL_GENERATOR}" -A "${MWFL_PLATFORM}"
            "-DCMAKE_PREFIX_PATH=${MWFL_INSTALL_DIR}"
            "-DMWFL_ENABLE_ASAN=${MWFL_ENABLE_ASAN}"
            "-DMWFL_TEST_SCINTILLA=${MWFL_EXPECT_SCINTILLA}"
            "-DMWFL_TEST_WEBVIEW2=${MWFL_EXPECT_WEBVIEW2}"
            "-DMWFL_WTL_SOURCE_DIR=${MWFL_WTL_SOURCE_DIR}"
            "-DMWFL_WIL_SOURCE_DIR=${MWFL_WIL_SOURCE_DIR}"
            "-DMWFL_DEPENDENCY_MODE=SYSTEM"
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer configure failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${MWFL_BUILD_DIR}/package-consumer-build"
            --config "${MWFL_CONFIGURATION}"
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer build failed: ${build_result}")
endif()

if(MWFL_EXPECT_SCINTILLA)
    execute_process(
        COMMAND "${MWFL_BUILD_DIR}/package-consumer-build/${MWFL_CONFIGURATION}/mwfl_scintilla_package_consumer.exe"
        RESULT_VARIABLE scintilla_run_result)
    if(NOT scintilla_run_result EQUAL 0)
        message(FATAL_ERROR "Installed Scintilla consumer failed: ${scintilla_run_result}")
    endif()
endif()

if(MWFL_EXPECT_WEBVIEW2)
    if(NOT EXISTS "${MWFL_INSTALL_DIR}/lib/WebView2LoaderStatic.lib")
        message(FATAL_ERROR "installed package is missing pinned WebView2 static loader")
    endif()
    execute_process(
        COMMAND "${MWFL_BUILD_DIR}/package-consumer-build/${MWFL_CONFIGURATION}/mwfl_webview2_package_consumer.exe"
        RESULT_VARIABLE webview2_run_result)
    if(NOT webview2_run_result EQUAL 0)
        message(FATAL_ERROR "Installed WebView2 consumer failed: ${webview2_run_result}")
    endif()
endif()

execute_process(
    COMMAND "${MWFL_BUILD_DIR}/package-consumer-build/${MWFL_CONFIGURATION}/mwfl_package_consumer.exe"
    RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer failed: ${run_result}")
endif()
