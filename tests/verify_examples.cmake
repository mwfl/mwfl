set(example_names
    hello
    application
    window
    native_message
    keyboard
    mouse
    resize
    timer
    paint
    minmax
    close_policy
    window_state
    dpi
    window_options
    wait_aware
    wakeup
    com_sta
    controls
    common_controls
    self_drawn_host
    system_lifecycle
    hot_corners
    form_binding
    commands
    desktop_integration
    appearance
    layout_gallery)

file(READ "${PROJECT_ROOT}/README.md" root_readme)
file(READ "${PROJECT_ROOT}/examples/README.md" examples_readme)
file(READ "${PROJECT_ROOT}/examples/CMakeLists.txt" examples_cmake)
file(READ "${PROJECT_ROOT}/include/mwtl/controls.h" controls_header)
file(READ "${PROJECT_ROOT}/include/mwtl/navigation_controls.h" navigation_controls_header)
file(READ "${PROJECT_ROOT}/include/mwtl/input_controls.h" input_controls_header)
file(READ "${PROJECT_ROOT}/include/mwtl/command_controls.h" command_controls_header)
file(READ "${PROJECT_ROOT}/include/mwtl/control_resources.h" control_resources_header)
file(READ "${PROJECT_ROOT}/examples/controls/main.cpp" controls_example)
file(READ "${PROJECT_ROOT}/examples/common_controls/main.cpp" common_controls_example)
file(READ "${PROJECT_ROOT}/site/index.html" site_home)
file(READ "${PROJECT_ROOT}/site/components/index.html" site_components)

set(public_control_types
    Label Button TextBox CheckBox RadioButton GroupBox ComboBox ListBox
    ProgressBar Slider)
foreach(control_type IN LISTS public_control_types)
    if(NOT controls_header MATCHES "class ${control_type} final")
        message(FATAL_ERROR "public control wrapper is missing: ${control_type}")
    endif()
    if(NOT controls_example MATCHES "mwtl::${control_type}")
        message(FATAL_ERROR "controls example does not instantiate: ${control_type}")
    endif()
endforeach()

set(specialized_control_types
    TreeView ListView Header TabControl ComboBoxEx DateTimePicker MonthCalendar
    HotKey IpAddress UpDown SysLink Toolbar StatusBar Rebar Pager Animation ScrollBar
    Tooltip ImageList)
set(all_specialized_headers "${controls_header}${navigation_controls_header}${input_controls_header}${command_controls_header}${control_resources_header}")
foreach(control_type IN LISTS specialized_control_types)
    if(NOT all_specialized_headers MATCHES "class ${control_type} final")
        message(FATAL_ERROR "specialized control wrapper is missing: ${control_type}")
    endif()
    if(NOT common_controls_example MATCHES "mwtl::${control_type}")
        message(FATAL_ERROR "common controls example does not instantiate: ${control_type}")
    endif()
endforeach()
foreach(function_name IN ITEMS ShowTaskDialog InitializeFlatScrollBars)
    if(NOT control_resources_header MATCHES "${function_name}")
        message(FATAL_ERROR "common controls function is missing: ${function_name}")
    endif()
    if(NOT common_controls_example MATCHES "mwtl::${function_name}")
        message(FATAL_ERROR "common controls example does not call: ${function_name}")
    endif()
endforeach()

foreach(example_name IN LISTS example_names)
    if(NOT EXISTS "${PROJECT_ROOT}/examples/${example_name}/main.cpp")
        message(FATAL_ERROR "example source is missing: ${example_name}")
    endif()
    if(NOT EXISTS "${PROJECT_ROOT}/examples/${example_name}/CMakeLists.txt")
        message(FATAL_ERROR "example CMakeLists is missing: ${example_name}")
    endif()
    if(NOT examples_cmake MATCHES "add_subdirectory\\(${example_name}\\)")
        message(FATAL_ERROR "example is not managed by examples/CMakeLists.txt: ${example_name}")
    endif()
    if(NOT root_readme MATCHES "examples/${example_name}")
        message(FATAL_ERROR "root README does not catalog example: ${example_name}")
    endif()
    if(NOT root_readme MATCHES "examples/${example_name}/main.cpp")
        message(FATAL_ERROR "root README does not link to example source: ${example_name}")
    endif()
    if(NOT examples_readme MATCHES "`${example_name}`")
        message(FATAL_ERROR "examples README does not catalog example: ${example_name}")
    endif()
    string(REPLACE "_" "-" image_slug "${example_name}")
    set(image_path "docs/images/examples/${image_slug}.png")
    if(NOT EXISTS "${PROJECT_ROOT}/${image_path}")
        message(FATAL_ERROR "README screenshot is missing: ${image_path}")
    endif()
endforeach()

set(all_control_types ${public_control_types}
    TreeView ListView Header TabControl ComboBoxEx DateTimePicker MonthCalendar
    HotKey IpAddress UpDown SysLink Toolbar StatusBar Rebar Pager Animation Splitter)
foreach(control_type IN LISTS all_control_types)
    if(NOT site_components MATCHES "<h3>${control_type}</h3>")
        message(FATAL_ERROR "Pages component card is missing: ${control_type}")
    endif()
endforeach()

string(REGEX MATCHALL "<article class=\"component-card\">" component_cards "${site_components}")
list(LENGTH component_cards component_card_count)
if(NOT component_card_count EQUAL 28)
    message(FATAL_ERROR "Pages must contain exactly 28 control cards; found ${component_card_count}")
endif()

# Keep every capture on disk for reference, but feature only visually useful
# examples in the README instead of showing nearly blank infrastructure windows.
set(featured_images controls common_controls form_binding layout_gallery)
foreach(example_name IN LISTS featured_images)
    string(REPLACE "_" "-" image_slug "${example_name}")
    set(image_path "docs/images/examples/${image_slug}.png")
    if(NOT root_readme MATCHES "${image_path}")
        message(FATAL_ERROR "README does not feature screenshot: ${image_path}")
    endif()
    if(NOT site_home MATCHES "docs/images/examples/${image_slug}.png")
        message(FATAL_ERROR "Pages does not show featured screenshot: ${example_name}")
    endif()
    if(NOT site_home MATCHES "examples/${example_name}/main.cpp")
        message(FATAL_ERROR "Pages does not link featured source: ${example_name}")
    endif()
endforeach()

if(NOT EXISTS "${PROJECT_ROOT}/docs/images/mwtl-mark.svg")
    message(FATAL_ERROR "README project mark is missing")
endif()
if(NOT root_readme MATCHES "docs/images/mwtl-mark.svg")
    message(FATAL_ERROR "README does not reference the project mark")
endif()

list(LENGTH example_names example_count)
if(NOT example_count EQUAL 27)
    message(FATAL_ERROR "example catalog must contain exactly 27 programs")
endif()
