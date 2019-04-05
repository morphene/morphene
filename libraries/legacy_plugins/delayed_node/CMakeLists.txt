file(GLOB HEADERS "include/morphene/delayed_node/*.hpp")

add_library( morphene_delayed_node
             delayed_node_plugin.cpp
           )

target_link_libraries( morphene_delayed_node morphene_chain morphene_protocol morphene_app )
target_include_directories( morphene_delayed_node
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( delayed_node_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   morphene_delayed_node

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
