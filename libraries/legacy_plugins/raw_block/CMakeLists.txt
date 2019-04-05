file(GLOB HEADERS "include/morphene/plugins/raw_block/*.hpp")

add_library( morphene_raw_block
             ${HEADERS}
             raw_block_plugin.cpp
             raw_block_api.cpp
           )

target_link_libraries( morphene_raw_block morphene_app morphene_chain morphene_protocol fc )
target_include_directories( morphene_raw_block
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
