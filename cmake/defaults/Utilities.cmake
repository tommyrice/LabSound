
function(source_file fname)
    if(IS_ABSOLUTE ${fname})
        target_sources(${PROJECT_NAME} PRIVATE ${fname})
    else()
        target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${fname})
    endif()
endfunction()

function(include_dir fpath)
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${fpath})
endfunction()

function(_set_cxx_14 proj)
	target_compile_features(${proj} INTERFACE cxx_std_14)
endfunction()

