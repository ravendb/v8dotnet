#
# CMake Toolchain file for crosscompiling on ARM.
#
# Target operating system name.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch32)
set(TARGET_ARCH aarch32-linux-gnu)

#set(CMAKE_LIBRARY_ARCHITECTURE ${TARGET_ARCH} CACHE STRING "" FORCE)
# Name of C compiler.
#set(CMAKE_C_COMPILER "/usr/bin/${TARGET_ARCH}-gcc-8")
#set(CMAKE_CXX_COMPILER "/usr/bin/${TARGET_ARCH}-g++-8")

# Where to look for the target environment. (More paths can be added here)
set(CMAKE_FIND_ROOT_PATH /usr/${TARGET_ARCH})
#set(CMAKE_SYSROOT /usr/${TARGET_ARCH})

find_program(CMAKE_C_COMPILER ${TARGET_ARCH}-gcc)
find_program(CMAKE_CXX_COMPILER ${TARGET_ARCH}-g++)
if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR "Can't find suitable C/C++ cross compiler for ${TARGET_ARCH}")
endif()

# Adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search headers and libraries in the target environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)


#set(CMAKE_AR ${TARGET_ARCH}-ar CACHE FILEPATH "" FORCE)
#set(CMAKE_RANLIB ${TARGET_ARCH}-ranlib)
#set(CMAKE_LINKER ${TARGET_ARCH}-ld)

# Not all shared libraries dependencies are installed in host machine.
# Make sure linker doesn't complain.
#set(CMAKE_EXE_LINKER_FLAGS_INIT -Wl,--allow-shlib-undefined)

# instruct nvcc to use our cross-compiler
#set(CMAKE_CUDA_FLAGS "-ccbin ${CMAKE_CXX_COMPILER} -Xcompiler -fPIC" CACHE STRING "" FORCE)]
set(CMAKE_CROSS_COMPILING TRUE)


