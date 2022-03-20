# the name of the target operating system
set(CMAKE_CROSS_COMPILING TRUE)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR x86_32)
set(TOOLCHAIN_PREFIX i686-w64-mingw32)

set(target_arch x86_32-win-gnu)
set(CMAKE_LIBRARY_ARCHITECTURE ${target_arch} CACHE STRING "" FORCE)

# which compilers to use
set(CMAKE_C_COMPILER     "${TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER   "${TOOLCHAIN_PREFIX}-g++")
#set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
#set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)

set(CMAKE_FIND_ROOT_PATH "/usr/${TOOLCHAIN_PREFIX}")
#set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)

# adjust the default behavior of the find commands:
# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)