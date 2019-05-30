file(GLOB HEADERS "include/morphene/chain/*.hpp" "include/morphene/chain/util/*.hpp")

## SORT .cpp by most likely to change / break compile
add_library( morphene_chain

             # As database takes the longest to compile, start it first
             database.cpp

             morphene_evaluator.cpp
             morphene_objects.cpp

             witness_schedule.cpp
             fork_database.cpp

             shared_authority.cpp
             block_log.cpp

             util/rd.cpp
             util/reward.cpp
             util/impacted.cpp
             util/advanced_benchmark_dumper.cpp

             ${HEADERS}
           )

target_link_libraries( morphene_chain morphene_jsonball morphene_protocol fc chainbase morphene_schema appbase
                       ${PATCH_MERGE_LIB} )
target_include_directories( morphene_chain
                            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/include" )

if( CLANG_TIDY_EXE )
   set_target_properties(
      morphene_chain PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

if(MSVC)
  set_source_files_properties( database.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

INSTALL( TARGETS
   morphene_chain

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/morphene/chain" )
