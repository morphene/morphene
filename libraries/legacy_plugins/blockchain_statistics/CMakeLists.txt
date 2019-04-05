file(GLOB HEADERS "include/morphene/blockchain_statistics/*.hpp")

add_library( morphene_blockchain_statistics
             blockchain_statistics_plugin.cpp
             blockchain_statistics_api.cpp
           )

target_link_libraries( morphene_blockchain_statistics morphene_chain morphene_protocol morphene_app )
target_include_directories( morphene_blockchain_statistics
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   morphene_blockchain_statistics

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)