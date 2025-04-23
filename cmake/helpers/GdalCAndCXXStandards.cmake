
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if (NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

# explicitly tell CMake not to do module
# dependency scanning
# https://discourse.cmake.org/t/cmake-3-28-cmake-cxx-compiler-clang-scan-deps-notfound-not-found/9244/3
set(CMAKE_CXX_SCAN_FOR_MODULES 0)
