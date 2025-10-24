# -----------------------------------------------------------------------------------
#
# Finds the toml11 library (header-only)
#
# -----------------------------------------------------------------------------------
#
# Variables used by this module, they can change the default behaviour.
# Those variables need to be either set before calling find_package
# or exported as environment variables before running CMake:
#
# TOML11_INCLUDEDIR - Set custom include path, useful when toml11 headers are
#                     outside system paths
#
# -----------------------------------------------------------------------------------
#
# Variables defined by this module:
#
# TOML11_FOUND        - True if toml11 was found
# TOML11_INCLUDE_DIRS - Path to toml11 include directory
#
# -----------------------------------------------------------------------------------
#
# Example usage:
#
#  set(TOML11_INCLUDEDIR "/opt/toml11/include")
#
#  find_package(toml11 REQUIRED)
#
#  include_directories("${TOML11_INCLUDE_DIRS}")
#  add_executable(foo foo.cc)
#
# -----------------------------------------------------------------------------------

foreach(opt TOML11_INCLUDEDIR)
  if(${opt} AND DEFINED ENV{${opt}} AND NOT ${opt} STREQUAL "$ENV{${opt}}")
    message(WARNING "Conflicting ${opt} values: ignoring environment variable and using CMake cache entry.")
  elseif(DEFINED ENV{${opt}} AND NOT ${opt})
    set(${opt} "$ENV{${opt}}")
  endif()
endforeach()

find_path(
  TOML11_INCLUDE_DIRS
  NAMES toml.hpp
  PATHS ${TOML11_INCLUDEDIR}
  HINTS
    $ENV{MINGW_PREFIX}/include
    $ENV{MINGW_PREFIX}
    /mingw64/include
    /mingw64
    /usr/include
    /usr/local/include
    /usr/include/toml11
    /usr/local/include/toml11
  PATH_SUFFIXES
    ""
    toml11
  DOC "Include directory for the toml11 library."
)

mark_as_advanced(TOML11_INCLUDE_DIRS)

if(TOML11_INCLUDE_DIRS)
  set(TOML11_FOUND TRUE)
endif()

mark_as_advanced(TOML11_FOUND)

if(TOML11_FOUND)
  if(NOT toml11_FIND_QUIETLY)
    message(STATUS "Found toml11 header files in ${TOML11_INCLUDE_DIRS}")
  endif()
elseif(toml11_FIND_REQUIRED)
  message(FATAL_ERROR "Could not find toml11")
else()
  message(STATUS "Optional package toml11 was not found")
endif()