cmake_minimum_required(VERSION 3.0.0)
project(mongocat)

# The version number.
set (mongocat_VERSION_MAJOR 0)
set (mongocat_VERSION_MINOR 1)
set (mongocat_VERSION_PATCH 0)

set (PROJECT_SOURCE_DIR src)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/mongocatConfig.h.in"
  "${PROJECT_BINARY_DIR}/mongocatConfig.h"
  )

# Specify the minimum version you require.
find_package (libmongoc-static-1.0 1.13 REQUIRED)

message ("--   mongoc found version \"${MONGOC_STATIC_VERSION}\"")
message ("--   mongoc include path \"${MONGOC_STATIC_INCLUDE_DIRS}\"")
message ("--   mongoc libraries \"${MONGOC_STATIC_LIBRARIES}\"")

include(CTest)
include_directories("${PROJECT_BINARY_DIR}" "include")
enable_testing()

#However, the file(GLOB...) allows for wildcard additions:
file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/*.c")

add_executable(mongocat ${SOURCES})
target_include_directories (mongocat PRIVATE "${MONGOC_STATIC_INCLUDE_DIRS}")
target_link_libraries (mongocat PRIVATE "${MONGOC_STATIC_LIBRARIES}")
target_compile_definitions (mongocat PRIVATE "${MONGOC_STATIC_DEFINITIONS}")

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
