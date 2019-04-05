file(GLOB HEADERS "include/morphene/account_statistics/*.hpp")

add_library( morphene_account_statistics
             account_statistics_plugin.cpp
             account_statistics_api.cpp
           )

target_link_libraries( morphene_account_statistics morphene_chain morphene_protocol morphene_app )
target_include_directories( morphene_account_statistics
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   morphene_account_statistics

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)