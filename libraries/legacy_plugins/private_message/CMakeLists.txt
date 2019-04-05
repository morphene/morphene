file(GLOB HEADERS "include/morphene/private_message/*.hpp")

add_library( morphene_private_message
             private_message_plugin.cpp
           )

target_link_libraries( morphene_private_message morphene_chain morphene_protocol morphene_app )
target_include_directories( morphene_private_message
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   morphene_private_message

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
