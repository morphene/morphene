add_executable( cli_wallet main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling cli_wallet with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

if( MORPHENE_STATIC_BUILD )
   target_link_libraries( cli_wallet PRIVATE
                          "-static-libstdc++ -static-libgcc -lreadline"
                          graphene_net morphene_protocol morphene_utilities morphene_wallet fc database_api_plugin account_history_api_plugin ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
else( MORPHENE_STATIC_BUILD )
   target_link_libraries( cli_wallet PRIVATE
                          graphene_net morphene_protocol morphene_utilities morphene_wallet fc database_api_plugin account_history_api_plugin ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
endif( MORPHENE_STATIC_BUILD )

if( CLANG_TIDY_EXE )
   set_target_properties(
      cli_wallet PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

if(MSVC)
  set_source_files_properties( main.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   cli_wallet

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
