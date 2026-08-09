set(required_headers
    application appearance binding command command_controls concepts control_batch
    control_host control_resources controls desktop document dpi error events
    input_controls layout message_pump must navigation_controls recent_files text_file text_history text_search timer wakeup
    window window_options)

file(READ "${PROJECT_ROOT}/docs/reference.md" reference)

foreach(header IN LISTS required_headers)
    if(NOT EXISTS "${PROJECT_ROOT}/include/mwtl/${header}.h")
        message(FATAL_ERROR "Missing public API header: ${header}.h")
    endif()
    if(NOT reference MATCHES "mwtl/${header}\\.h")
        message(FATAL_ERROR "Public header is missing from reference: ${header}.h")
    endif()
endforeach()

set(required_api
    "class Application" "class Window" "class WindowBase" "class NativeControl"
    "class LayoutNode" "class MessagePump" "class WindowWakeup"
    "class Error" "class Command" "class CommandSet" "class ChangeGate")
file(GLOB public_headers "${PROJECT_ROOT}/include/mwtl/*.h")
set(surface "")
foreach(header IN LISTS public_headers)
    file(READ "${header}" contents)
    string(APPEND surface "\n${contents}")
endforeach()
foreach(symbol IN LISTS required_api)
    string(FIND "${surface}" "${symbol}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Required 0.1 API is missing: ${symbol}")
    endif()
endforeach()
