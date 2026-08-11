mwfl_add_example(mwfl_sqlite_viewer
    main.cpp
    sqlite_database.cpp)
target_link_libraries(mwfl_sqlite_viewer PRIVATE winsqlite3)

install(TARGETS mwfl_sqlite_viewer
    RUNTIME DESTINATION examples/sqlite_viewer
    COMPONENT sqlite_viewer)
