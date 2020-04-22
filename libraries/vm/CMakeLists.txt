file(GLOB evm_file "${CMAKE_CURRENT_SOURCE_DIR}/evm/*.hpp")
file(GLOB common_file "${CMAKE_CURRENT_SOURCE_DIR}/common/*.hpp")
add_library( libvm ${evm_file} ${common_file} instruction.hpp  chainFace.hpp instruction.cpp chainFace.cpp)
target_link_libraries( libvm PRIVATE  fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
target_include_directories( libvm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                                          "${CMAKE_CURRENT_SOURCE_DIR}/../chain/include"
                                          "${CMAKE_CURRENT_SOURCE_DIR}/../db/include"
                                          )
if(MSVC)
  set_source_files_properties( instruction.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   libvm

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
