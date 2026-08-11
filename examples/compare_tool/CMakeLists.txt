mwfl_add_example(mwfl_compare_tool
    main.cpp
    compare_model.cpp)
target_link_libraries(mwfl_compare_tool PRIVATE shlwapi)

install(TARGETS mwfl_compare_tool
    RUNTIME DESTINATION examples/compare_tool
    COMPONENT compare_tool)
