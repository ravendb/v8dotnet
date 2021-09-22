# Toolchain_aarch64_l4t.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TARGET_ARCH x86_64-linux-gnu)
set(CMAKE_LIBRARY_ARCHITECTURE ${TARGET_ARCH} CACHE STRING "" FORCE)

# Which compilers to use for C and C++
set(CMAKE_C_COMPILER gcc -m32 -ggdb -O0) # -lpthread -ldl -lstdc++fs)
set(CMAKE_CXX_COMPILER g++ -m32 -ggdb -O0) # -lpthread -ldl -lstdc++fs)
set(CMAKE_CROSS_COMPILING TRUE)
