if(NOT DEFINED WORKSPACE_EXE OR NOT DEFINED RESULT_FILE)
    message(FATAL_ERROR "WORKSPACE_EXE and RESULT_FILE are required")
endif()
file(REMOVE "${RESULT_FILE}")
execute_process(
    COMMAND "${WORKSPACE_EXE}" --self-test --self-test-result "${RESULT_FILE}"
    RESULT_VARIABLE exit_code
    TIMEOUT 25)
if(EXISTS "${RESULT_FILE}")
    file(READ "${RESULT_FILE}" result)
else()
    set(result "self-test produced no diagnostic result")
endif()
if(NOT exit_code EQUAL 0 OR NOT result STREQUAL "ok")
    message(FATAL_ERROR
        "Document workspace GUI self-test failed (exit ${exit_code}): ${result}")
endif()
