if(NOT DEFINED MWTL_BUILD_DIR OR NOT DEFINED MWTL_SOURCE_DIR OR
   NOT DEFINED MWTL_INSTALL_DIR OR NOT DEFINED MWTL_CONFIGURATION OR
   NOT DEFINED MWTL_GENERATOR OR NOT DEFINED MWTL_PLATFORM)
    message(FATAL_ERROR "Package consumer verification arguments are incomplete")
endif()

file(REMOVE_RECURSE "${MWTL_INSTALL_DIR}" "${MWTL_BUILD_DIR}/package-consumer-build")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MWTL_BUILD_DIR}"
            --config "${MWTL_CONFIGURATION}" --prefix "${MWTL_INSTALL_DIR}"
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "mwtl install failed: ${install_result}")
endif()
if(NOT EXISTS "${MWTL_INSTALL_DIR}/bin/mwtl_notepad.exe")
    message(FATAL_ERROR "installed package is missing bin/mwtl_notepad.exe")
endif()

foreach(method IN ITEMS subdirectory fetchcontent)
    set(method_build "${MWTL_BUILD_DIR}/package-${method}-build")
    file(REMOVE_RECURSE "${method_build}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
                -S "${MWTL_SOURCE_DIR}/tests/package_${method}"
                -B "${method_build}"
                -G "${MWTL_GENERATOR}" -A "${MWTL_PLATFORM}"
                "-DMWTL_SOURCE_DIR=${MWTL_SOURCE_DIR}"
                "-DMWTL_WTL_SOURCE_DIR=${MWTL_WTL_SOURCE_DIR}"
                "-DMWTL_WIL_SOURCE_DIR=${MWTL_WIL_SOURCE_DIR}"
                "-DMWTL_DEPENDENCY_MODE=SYSTEM"
        RESULT_VARIABLE method_configure_result)
    if(NOT method_configure_result EQUAL 0)
        message(FATAL_ERROR "${method} consumer configure failed: ${method_configure_result}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${method_build}"
                --config "${MWTL_CONFIGURATION}"
        RESULT_VARIABLE method_build_result)
    if(NOT method_build_result EQUAL 0)
        message(FATAL_ERROR "${method} consumer build failed: ${method_build_result}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${MWTL_SOURCE_DIR}/tests/package_consumer"
            -B "${MWTL_BUILD_DIR}/package-consumer-build"
            -G "${MWTL_GENERATOR}" -A "${MWTL_PLATFORM}"
            "-DCMAKE_PREFIX_PATH=${MWTL_INSTALL_DIR}"
            "-DMWTL_ENABLE_ASAN=${MWTL_ENABLE_ASAN}"
            "-DMWTL_WTL_SOURCE_DIR=${MWTL_WTL_SOURCE_DIR}"
            "-DMWTL_WIL_SOURCE_DIR=${MWTL_WIL_SOURCE_DIR}"
            "-DMWTL_DEPENDENCY_MODE=SYSTEM"
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer configure failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${MWTL_BUILD_DIR}/package-consumer-build"
            --config "${MWTL_CONFIGURATION}"
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer build failed: ${build_result}")
endif()

execute_process(
    COMMAND "${MWTL_BUILD_DIR}/package-consumer-build/${MWTL_CONFIGURATION}/mwtl_package_consumer.exe"
    RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Installed-package consumer failed: ${run_result}")
endif()
