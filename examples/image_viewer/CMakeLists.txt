mwfl_add_example(mwfl_image_viewer_demo main.cpp application.cpp application.h)
target_link_libraries(mwfl_image_viewer_demo PRIVATE mwfl::d2d mwfl::imaging)
