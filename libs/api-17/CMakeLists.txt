cmake_minimum_required(VERSION 3.5)

if (TARGET ocpn::api)
  return ()
endif ()

set(API_VERSION "1.17" PARENT_SCOPE)
set(API_OCPN_MIN_VERSION "5.2.0" PARENT_SCOPE)

add_library(_API INTERFACE)
add_library(ocpn::api ALIAS _API)
target_include_directories(_API INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
if (WIN32 AND MSVC)
  target_link_libraries(_API INTERFACE
                        "${CMAKE_CURRENT_SOURCE_DIR}/msvc/opencpn.lib")
elseif(WIN32 AND MINGW) 
  target_link_libraries(_API INTERFACE
                        "${CMAKE_CURRENT_SOURCE_DIR}/mingw/libopencpn.dll.a")
endif ()
