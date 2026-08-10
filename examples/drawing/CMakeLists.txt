mwfl_add_example(mwfl_drawing_demo main.cpp drawing_model.cpp drawing_model.h)
target_link_libraries(mwfl_drawing_demo PRIVATE mwfl::d2d)
