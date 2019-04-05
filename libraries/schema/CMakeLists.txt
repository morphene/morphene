file(GLOB HEADERS "include/morphene/schema/*.hpp")
add_library( morphene_schema schema.cpp ${HEADERS} )
target_link_libraries( morphene_schema fc )
target_include_directories( morphene_schema PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   morphene_schema

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/morphene/schema" )
