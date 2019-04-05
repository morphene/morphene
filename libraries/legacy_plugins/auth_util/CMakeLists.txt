file(GLOB HEADERS "include/morphene/plugins/auth_util/*.hpp")

add_library( morphene_auth_util
             ${HEADERS}
             auth_util_plugin.cpp
             auth_util_api.cpp
           )

target_link_libraries( morphene_auth_util morphene_app morphene_chain morphene_protocol fc )
target_include_directories( morphene_auth_util
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
