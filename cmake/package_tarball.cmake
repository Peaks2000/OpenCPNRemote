if (NOT PLUGIN_BINARY)
  message(FATAL_ERROR "PLUGIN_BINARY is required")
endif ()
if (NOT METADATA_FILE)
  message(FATAL_ERROR "METADATA_FILE is required")
endif ()
if (NOT PACKAGE_ROOT)
  message(FATAL_ERROR "PACKAGE_ROOT is required")
endif ()
if (NOT TARBALL_FILE)
  message(FATAL_ERROR "TARBALL_FILE is required")
endif ()

get_filename_component(PACKAGE_PARENT "${PACKAGE_ROOT}" DIRECTORY)
get_filename_component(PACKAGE_NAME "${PACKAGE_ROOT}" NAME)
get_filename_component(PLUGIN_NAME "${PLUGIN_BINARY}" NAME)

file(REMOVE_RECURSE "${PACKAGE_ROOT}" "${TARBALL_FILE}")
file(MAKE_DIRECTORY "${PACKAGE_ROOT}/usr/local/lib/opencpn")
file(COPY_FILE "${PLUGIN_BINARY}" "${PACKAGE_ROOT}/usr/local/lib/opencpn/${PLUGIN_NAME}")
file(COPY_FILE "${METADATA_FILE}" "${PACKAGE_PARENT}/metadata.xml")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar czf "${TARBALL_FILE}" metadata.xml "${PACKAGE_NAME}"
  WORKING_DIRECTORY "${PACKAGE_PARENT}"
  RESULT_VARIABLE result
)
file(REMOVE "${PACKAGE_PARENT}/metadata.xml")

if (NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to create ${TARBALL_FILE}")
endif ()

message(STATUS "Created ${TARBALL_FILE}")
