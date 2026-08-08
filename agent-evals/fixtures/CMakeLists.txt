function(mwtl_add_agent_eval name)
    add_library(mwtl_agent_eval_${name} OBJECT "${name}.cpp")
    target_compile_features(mwtl_agent_eval_${name} PRIVATE cxx_std_20)
    target_compile_options(mwtl_agent_eval_${name} PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_libraries(mwtl_agent_eval_${name} PRIVATE mwtl::mwtl)
    set_property(GLOBAL APPEND PROPERTY MWTL_AGENT_EVAL_TARGETS mwtl_agent_eval_${name})
endfunction()

foreach(name IN ITEMS window form layout commands background dialog placement dpi appearance native_message)
    mwtl_add_agent_eval(${name})
endforeach()

get_property(mwtl_agent_eval_targets GLOBAL PROPERTY MWTL_AGENT_EVAL_TARGETS)
add_custom_target(mwtl_agent_eval_fixtures DEPENDS ${mwtl_agent_eval_targets})

