file(GLOB HEADERS "include/morphene/plugins/block_info/*.hpp")

add_library( morphene_block_info
             ${HEADERS}
             block_info_plugin.cpp
             block_info_api.cpp
           )

target_link_libraries( morphene_block_info morphene_app morphene_chain morphene_protocol fc )
target_include_directories( morphene_block_info
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
