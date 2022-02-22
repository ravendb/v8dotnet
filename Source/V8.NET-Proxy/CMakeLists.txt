# File generated at : 14:25:02, Fri 22 Nov 
# Converted Project : V8.Net-Proxy-x64.vcxproj
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(V8_Net_Proxy) # <-- This defines many internal CMake variables, required for other commands.

set (MSCPlatformToolset 200)

# TODO: Make a variable for holding the path to the V8 source. 
# installed clang and set it as the default compiler
# installed sudo apt-get install libc++-dev 

set(BOOST_BEAST_INCLUDE "usr/include" PATH "Path to the Boost.Beast include dir." )

include_directories(
    V8/v8/
    V8/v8/include
    #debugging/
    #V8/v8/outecho .gn/x64.release/obj/third_party/icu
    #V8/v8/out.gn/x64.release/obj/src/inspector
    #V8/v8/out.gn/x64.release/obj
    #V8/v8/out.gn/x64.release/gen
)

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(GLIB glib-2.0)
include_directories(
    ${GLIB_INCLUDE_DIRS} 
    ${BOOST_BEAST_INCLUDE}
)
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})


add_compile_options("-fms-extensions")


##################### Variables ############################
# Change if you want modify path or other values           #
############################################################

# Project
get_filename_component(PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(DEPENDENCIES_DIR ${PROJECT_DIR}/dependencies)
set(PROJECT_NAME V8_Net_Proxy_x86_64) #"${CMAKE_SYSTEM_PROCESSOR})


# Outputs
set(OUTPUT_DEBUG ../bin/netstandard/Debug/)
set(OUTPUT_RELEASE ../bin/netstandard/Release/)


#g++ -I. -Iinclude samples/hello-world.cc -o hello_world -lv8_monolith -Lout.gn/x64.release.sample/obj/ -pthread -lrt -ldl -std=c++14 -DV8_COMPRESS_POINTERS
SET(GCC_COVERAGE_COMPILE_FLAGS "-shared -fPIC -pthread -lrt -ldl -std=c++14 -DV8_COMPRESS_POINTERS")


################# CMake Project ############################
# The main options of project                              #
############################################################

project(${PROJECT_NAME} CXX)

# Define Release by default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default.")
endif(NOT CMAKE_BUILD_TYPE)

############## Artefacts Output ############################
# Defines outputs , depending BUILD TYPE                   #
############################################################

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_DIR}/${OUTPUT_DEBUG}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_DIR}/${OUTPUT_DEBUG}")
  set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${PROJECT_DIR}/${OUTPUT_DEBUG}")
else()
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_DIR}/${OUTPUT_RELEASE}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_DIR}/${OUTPUT_RELEASE}")
  set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${PROJECT_DIR}/${OUTPUT_RELEASE}")
endif()

# Messages
message("${PROJECT_NAME}: MAIN PROJECT: ${CMAKE_PROJECT_NAME}")
message("${PROJECT_NAME}: CURR PROJECT: ${CMAKE_CURRENT_SOURCE_DIR}")
message("${PROJECT_NAME}: CURR BIN DIR: ${CMAKE_CURRENT_BINARY_DIR}")

############### Files & Targets ############################
# Files of project and target to build                     #
############################################################

set(LIBV8
    v8_monolith
)
link_directories(
    V8/v8/out.gn/x64.release.sample/obj/
)

FILE(GLOB DEBUGGING_H "./debugging/*.h")
FILE(GLOB DEBUGGING_CPP "./debugging/*.cpp")

# Source Files
set(SRC_FILES
    ./ContextProxy.cpp
    ./Exports.cpp
    ./FunctionTemplateProxy.cpp
    ./HandleProxy.cpp
    ./ObjectTemplateProxy.cpp
    ./Utilities.cpp
    ./V8EngineProxy.cpp
    ./ValueProxy.cpp
    //${DEBUGGING_CPP}
)
source_group("Sources" FILES ${SRC_FILES})

# Header Files
set(HEADERS_FILES
    Platform.h
    ProxyTypes.h
    resource.h
    //${DEBUGGING_H}
)
source_group("Headers" FILES ${HEADERS_FILES})

# Add library to build.
add_library(${PROJECT_NAME} SHARED
   ${SRC_FILES} ${HEADERS_FILES} 
)

######################### Flags ############################
# Defines Flags for Windows and Linux                      #
############################################################

#add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    SET_TARGET_PROPERTIES (${PROJECT_NAME} PROPERTIES DEFINE_SYMBOL  "_MSC_VER__V8_NET" )
    #SET_TARGET_PROPERTIES (${PROJECT_NAME} PROPERTIES DEFINE_SYMBOL  "V8_OS_WIN" )
endif()

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
   if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
   endif()
endif(NOT MSVC)


# Preprocessor definitions
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
   -D_MSC_PLATFORM_TOOLSET=${MSCPlatformToolset}
   -D_ITERATOR_DEBUG_LEVEL=0 
   -DDEBUG 
    )
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE  /W3 /MD /Od /Zi)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
   -D_MSC_PLATFORM_TOOLSET=${MSCPlatformToolset}
    )
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE  /W3 /GL /Oi /Gy /Zi)
    endif()
endif()

########### Link & Dependencies ############################
# Add project dependencies and Link to project             #
############################################################

target_link_libraries(${PROJECT_NAME} -Wl,--start-group ${LIBV8} -Wl,--end-group)
#target_link_libraries(${PROJECT_NAME} pthread)
#target_link_libraries(${PROJECT_NAME} stdc++fs)
#target_link_libraries(${PROJECT_NAME} dl)