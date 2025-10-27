PROJECT(subconverter LANGUAGES CXX)
SET(BUILD_TARGET_NAME ${PROJECT_NAME})
CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/include/")

# Enable compile_commands.json generation
SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release)
ENDIF()
SET(CMAKE_CXX_STANDARD 20)

IF(NOT MSVC)
    ADD_COMPILE_OPTIONS(-Wall -Wextra -Wno-unused-parameter -Wno-unused-result)
ELSE()
    ADD_COMPILE_OPTIONS(/W4)
ENDIF()

#remove std::regex support since it is not compatible with group modifiers and slow
#OPTION(USING_STD_REGEX "Use std::regex from C++ library instead of PCRE2." OFF)
OPTION(USING_MALLOC_TRIM "Call malloc_trim after processing request to lower memory usage (Your system must support malloc_trim)." OFF)
#now using internal MD5 calculation
#OPTION(USING_MBEDTLS "Use mbedTLS instead of OpenSSL for MD5 calculation." OFF)
OPTION(BUILD_STATIC_LIBRARY "Build a static library containing only the essential part." OFF)

INCLUDE(CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES(
"
#include<string>
int main(){std::to_string(0);}
" HAVE_TO_STRING)

IF(NOT BUILD_STATIC_LIBRARY)

ADD_EXECUTABLE(${BUILD_TARGET_NAME} 
    src/generator/config/nodemanip.cpp
    src/generator/config/ruleconvert.cpp
    src/generator/config/subexport.cpp
    src/generator/template/templates.cpp
    src/handler/interfaces.cpp
    src/handler/multithread.cpp
    src/handler/upload.cpp
    src/handler/webget.cpp
    src/handler/settings.cpp
    src/main.cpp
    src/parser/infoparser.cpp
    src/parser/subparser.cpp
    src/script/cron.cpp
    src/script/script_quickjs.cpp
#    src/server/webserver_libevent.cpp
    src/server/webserver_httplib.cpp
    src/utils/base64/base64.cpp
    src/utils/codepage.cpp
    src/utils/file.cpp
    src/utils/logger.cpp
    src/utils/md5/md5.cpp
    src/utils/network.cpp
    src/utils/regexp.cpp
    src/utils/string.cpp
    src/utils/system.cpp
    src/utils/urlencode.cpp)
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE src)
TARGET_LINK_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

FIND_PACKAGE(PkgConfig REQUIRED)

SET(THREADS_PREFER_PTHREAD_FLAG ON)
FIND_PACKAGE(Threads REQUIRED)
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT})

#PKG_CHECK_MODULES(LIBEVENT libevent>=2.1.10 REQUIRED)
#FIND_PATH(LIBEVENT_INCLUDE_DIR NAMES event.h PATHS ${LIBEVENT_INCLUDE_DIRS})
#FIND_LIBRARY(LIBEVENT_LIBRARY NAMES event PATHS ${LIBEVENT_LIBRARY_DIRS})
#TARGET_LINK_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${LIBEVENT_LIBRARY_DIRS})
#TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${LIBEVENT_INCLUDE_DIR})
#TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${LIBEVENT_LIBRARY})

FIND_PACKAGE(CURL 7.54.0 REQUIRED)
TARGET_LINK_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${CURL_LIBRARY_DIRS})
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE CURL::libcurl)
TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DCURL_STATICLIB)

# rapidjson is header-only, find it directly
message(STATUS "Searching for rapidjson headers...")
message(STATUS "  MINGW_PREFIX: $ENV{MINGW_PREFIX}")
message(STATUS "  Checking paths: $ENV{MINGW_PREFIX}/include, /mingw64/include, /usr/include, etc.")

find_path(RAPIDJSON_INCLUDE_DIRS
  NAMES rapidjson/rapidjson.h rapidjson.h
  HINTS
    $ENV{MINGW_PREFIX}/include
    $ENV{MINGW_PREFIX}
    # Standard MSYS2 paths
    /mingw64/include
    /mingw64
    /usr/include
    /usr/local/include
    # GitHub Actions MSYS2 paths (Unix format)
    /c/msys64/mingw64/include
    /c/msys64/usr/include
  PATH_SUFFIXES
    ""
    rapidjson
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
  NO_CMAKE_PATH
  NO_CMAKE_ENVIRONMENT_PATH
  NO_SYSTEM_ENVIRONMENT_PATH
  NO_CMAKE_SYSTEM_PATH
  NO_CMAKE_INSTALL_PREFIX
  ONLY_CMAKE_FIND_ROOT_PATH
)

if(RAPIDJSON_INCLUDE_DIRS)
  message(STATUS "Found rapidjson header files in ${RAPIDJSON_INCLUDE_DIRS}")
  TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
else()
  message(STATUS "rapidjson not found in standard paths. Current search paths:")
  message(STATUS "  MINGW_PREFIX: $ENV{MINGW_PREFIX}")
  message(STATUS "  PATH: $ENV{PATH}")
  message(STATUS "  Checked locations:")
  message(STATUS "    - $ENV{MINGW_PREFIX}/include")
  message(STATUS "    - $ENV{MINGW_PREFIX}/include/rapidjson")
  message(STATUS "    - /mingw64/include")
  message(STATUS "    - /mingw64/include/rapidjson")
  message(STATUS "    - /usr/include")
  message(STATUS "    - /usr/include/rapidjson")
  message(FATAL_ERROR "Could not find rapidjson headers. Please install rapidjson and ensure rapidjson/rapidjson.h is available.")
endif()

# toml11 is header-only, find it directly
message(STATUS "Searching for toml11 headers...")
message(STATUS "  MINGW_PREFIX: $ENV{MINGW_PREFIX}")
message(STATUS "  Checking paths: $ENV{MINGW_PREFIX}/include, /mingw64/include, /usr/include, etc.")

find_path(TOML11_INCLUDE_DIRS
  NAMES toml.hpp
  HINTS
    $ENV{MINGW_PREFIX}/include
    $ENV{MINGW_PREFIX}
    # Standard MSYS2 paths
    /mingw64/include
    /mingw64
    /usr/include
    /usr/local/include
    /usr/include/toml11
    /usr/local/include/toml11
    # GitHub Actions MSYS2 paths (Unix format)
    /c/msys64/mingw64/include
    /c/msys64/usr/include
  PATH_SUFFIXES
    ""
    toml11
)

if(TOML11_INCLUDE_DIRS)
  message(STATUS "Found toml11 header files in ${TOML11_INCLUDE_DIRS}")
  TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${TOML11_INCLUDE_DIRS})
else()
  message(STATUS "toml11 not found in any standard path. Current search paths:")
  message(STATUS "  MINGW_PREFIX: $ENV{MINGW_PREFIX}")
  message(STATUS "  PATH: $ENV{PATH}")
  message(FATAL_ERROR "Could not find toml11 headers. Please install toml11 and ensure toml.hpp is available.")
endif()

PKG_CHECK_MODULES(YAML_CPP yaml-cpp>=0.6.3 REQUIRED)
FIND_LIBRARY(YAML_CPP_LIBRARY NAMES yaml-cpp yaml-cppd PATHS ${YAML_CPP_LIBRARY_DIRS})
TARGET_LINK_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${YAML_CPP_LIBRARY_DIRS})
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${YAML_CPP_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${YAML_CPP_LIBRARY})
TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DYAML_CPP_STATIC_DEFINE)

FIND_PACKAGE(PCRE2 REQUIRED)
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${PCRE2_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${PCRE2_LIBRARY})
TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DPCRE2_STATIC)

FIND_PACKAGE(QuickJS REQUIRED)
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${QUICKJS_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${QUICKJS_LIBRARIES})

FIND_PACKAGE(LibCron REQUIRED)
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${LIBCRON_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${LIBCRON_LIBRARIES})

FIND_PACKAGE(LibXml2 REQUIRED)
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${LIBXML2_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${LIBXML2_LIBRARIES})
TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DLIBXML_STATIC)
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${LIBXML2_LIBRARIES} z)

IF(WIN32)
    TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE wsock32 ws2_32)
    # ODBC libraries for CWE 798 examples
    TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE odbc32 odbccp32)
ELSE()
    INCLUDE(GNUInstallDirs)
    INSTALL(TARGETS ${BUILD_TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR}/${BUILD_TARGET_NAME})
    INSTALL(DIRECTORY base/ DESTINATION ${CMAKE_INSTALL_BINDIR}/${BUILD_TARGET_NAME} FILES_MATCHING PATTERN "*")
ENDIF()

ELSE() #BUILD_STATIC_LIBRARY

ADD_LIBRARY(${BUILD_TARGET_NAME} STATIC
    src/generator/config/ruleconvert.cpp
    src/generator/config/subexport.cpp
    src/generator/template/templates.cpp
    src/lib/wrapper.cpp
    src/parser/subparser.cpp
    src/utils/base64/base64.cpp
    src/utils/codepage.cpp
    src/utils/logger.cpp
    src/utils/md5/md5.cpp
    src/utils/network.cpp
    src/utils/regexp.cpp
    src/utils/string.cpp
    src/utils/urlencode.cpp)
TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DNO_JS_RUNTIME -DNO_WEBGET)

TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PUBLIC src)

# rapidjson is header-only, find it directly (same as main target)
find_path(RAPIDJSON_INCLUDE_DIRS
  NAMES rapidjson/rapidjson.h rapidjson.h
  HINTS
    $ENV{MINGW_PREFIX}/include
    $ENV{MINGW_PREFIX}
    # Standard MSYS2 paths
    /mingw64/include
    /mingw64
    /usr/include
    /usr/local/include
    # GitHub Actions MSYS2 paths (Unix format)
    /c/msys64/mingw64/include
    /c/msys64/usr/include
  PATH_SUFFIXES
    ""
    rapidjson
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
  NO_CMAKE_PATH
  NO_CMAKE_ENVIRONMENT_PATH
  NO_SYSTEM_ENVIRONMENT_PATH
  NO_CMAKE_SYSTEM_PATH
  NO_CMAKE_INSTALL_PREFIX
  ONLY_CMAKE_FIND_ROOT_PATH
)

if(RAPIDJSON_INCLUDE_DIRS)
  message(STATUS "Found rapidjson header files in ${RAPIDJSON_INCLUDE_DIRS}")
  TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
else()
  message(STATUS "rapidjson not found in standard paths. Current search paths:")
  message(STATUS "  MINGW_PREFIX: $ENV{MINGW_PREFIX}")
  message(STATUS "  PATH: $ENV{PATH}")
  message(FATAL_ERROR "Could not find rapidjson headers. Please install rapidjson and ensure rapidjson/rapidjson.h is available.")
endif()

FIND_PACKAGE(PkgConfig REQUIRED)

PKG_CHECK_MODULES(YAML_CPP yaml-cpp>=0.6.3 REQUIRED)
FIND_LIBRARY(YAML_CPP_LIBRARY NAMES yaml-cpp yaml-cppd PATHS ${YAML_CPP_LIBRARY_DIRS})
TARGET_LINK_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${YAML_CPP_LIBRARY_DIRS})
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${YAML_CPP_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${YAML_CPP_LIBRARY})
TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DYAML_CPP_STATIC_DEFINE)

FIND_PACKAGE(PCRE2 REQUIRED)
TARGET_INCLUDE_DIRECTORIES(${BUILD_TARGET_NAME} PRIVATE ${PCRE2_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ${PCRE2_LIBRARY})
TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DPCRE2_STATIC)

IF(WIN32)
    TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE ws2_32)
    # ODBC libraries for CWE 798 examples
    TARGET_LINK_LIBRARIES(${BUILD_TARGET_NAME} PRIVATE odbc32 odbccp32)
ENDIF()

ENDIF() #BUILD_STATIC_LIBRARY

IF(HAVE_TO_STRING)
    TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DHAVE_TO_STRING)
ENDIF()

IF(USING_MALLOC_TRIM)
    TARGET_COMPILE_DEFINITIONS(${BUILD_TARGET_NAME} PRIVATE -DMALLOC_TRIM)
ENDIF()
