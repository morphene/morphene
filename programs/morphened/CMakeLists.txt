add_executable( morphened main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling morphened with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

if( MORPHENE_STATIC_BUILD )
   target_link_libraries( morphened PRIVATE
      "-static-libstdc++ -static-libgcc"
      appbase
      morphene_utilities
      morphene_plugins
      ${CMAKE_DL_LIBS}
      ${PLATFORM_SPECIFIC_LIBS}
   )
else( MORPHENE_STATIC_BUILD )
   target_link_libraries( morphened PRIVATE
      appbase
      morphene_utilities
      morphene_plugins
      ${CMAKE_DL_LIBS}
      ${PLATFORM_SPECIFIC_LIBS}
   )
endif( MORPHENE_STATIC_BUILD )

if( CLANG_TIDY_EXE )
   set_target_properties(
      morphened PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

install( TARGETS
   morphened

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
