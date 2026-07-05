cmake_minimum_required(VERSION 3.16)

project(opencpn_remote_pi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(wxWidgets REQUIRED COMPONENTS core base net)
include(${wxWidgets_USE_FILE})

add_subdirectory(libs/api-17)

set(PLUGIN_DISPLAY_NAME "OpenCPN Remote")
set(PLUGIN_SUMMARY "OpenCPN-only browser remote control")
set(PLUGIN_DESCRIPTION "Streams only the OpenCPN application window to a responsive browser client and forwards authenticated input back to OpenCPN.")
set(PLUGIN_AUTHOR "Local")
set(PLUGIN_SOURCE "https://opencpn.org")

execute_process(
  COMMAND uname -m
  OUTPUT_VARIABLE OCPN_REMOTE_TARGET_ARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (OCPN_REMOTE_TARGET_ARCH STREQUAL "amd64")
  set(OCPN_REMOTE_TARGET_ARCH "x86_64")
endif ()

set(OCPN_REMOTE_BUILD_TARGET "linux")
set(OCPN_REMOTE_TARGET_VERSION "local")
if (EXISTS "/etc/os-release")
  file(STRINGS "/etc/os-release" OCPN_REMOTE_OS_RELEASE)
  foreach(line IN LISTS OCPN_REMOTE_OS_RELEASE)
    if (line MATCHES "^ID=(.*)")
      set(OCPN_REMOTE_BUILD_TARGET "${CMAKE_MATCH_1}")
      string(REGEX REPLACE "^\"|\"$" "" OCPN_REMOTE_BUILD_TARGET "${OCPN_REMOTE_BUILD_TARGET}")
    elseif(line MATCHES "^VERSION_ID=(.*)")
      set(OCPN_REMOTE_TARGET_VERSION "${CMAKE_MATCH_1}")
      string(REGEX REPLACE "^\"|\"$" "" OCPN_REMOTE_TARGET_VERSION "${OCPN_REMOTE_TARGET_VERSION}")
    elseif(line MATCHES "^BUILD_ID=(.*)" AND OCPN_REMOTE_TARGET_VERSION STREQUAL "local")
      set(OCPN_REMOTE_TARGET_VERSION "${CMAKE_MATCH_1}")
      string(REGEX REPLACE "^\"|\"$" "" OCPN_REMOTE_TARGET_VERSION "${OCPN_REMOTE_TARGET_VERSION}")
    endif ()
  endforeach()
endif ()

set(OCPN_REMOTE_TARGET "${OCPN_REMOTE_BUILD_TARGET}-${OCPN_REMOTE_TARGET_ARCH}")
set(OCPN_REMOTE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.0")
set(OCPN_REMOTE_TARBALL_NAME
  "${PROJECT_NAME}-${OCPN_REMOTE_VERSION}-${OCPN_REMOTE_TARGET}-${OCPN_REMOTE_TARGET_VERSION}"
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/metadata.xml.in
  ${CMAKE_CURRENT_BINARY_DIR}/metadata.xml
  @ONLY
)

add_library(${PROJECT_NAME} SHARED
  src/http_server.cpp
  src/http_server.h
  src/opencpn_remote_pi.cpp
  src/opencpn_remote_pi.h
  src/remote_frame_provider.cpp
  src/remote_frame_provider.h
  src/web_assets.cpp
  src/web_assets.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  MAKING_PLUGIN
  PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  ocpn::api
  ${wxWidgets_LIBRARIES}
)

if (WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "opencpn_remote_pi"
)

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION lib/opencpn
  RUNTIME DESTINATION bin
)

add_custom_target(plugin_tarball
  COMMAND ${CMAKE_COMMAND}
    -DPLUGIN_BINARY=$<TARGET_FILE:${PROJECT_NAME}>
    -DMETADATA_FILE=${CMAKE_CURRENT_BINARY_DIR}/metadata.xml
    -DPACKAGE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package/${OCPN_REMOTE_TARBALL_NAME}
    -DTARBALL_FILE=${CMAKE_CURRENT_BINARY_DIR}/${OCPN_REMOTE_TARBALL_NAME}.tar.gz
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_tarball.cmake
  DEPENDS ${PROJECT_NAME} ${CMAKE_CURRENT_BINARY_DIR}/metadata.xml
  COMMENT "Creating OpenCPN importable plugin tarball"
)
