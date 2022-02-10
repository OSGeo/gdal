cmake_minimum_required(VERSION 3.0)

project(test_cpp CXX)

find_package(GDAL CONFIG REQUIRED)

add_executable(test_cpp test_cpp.cpp)
set_property(TARGET test_cpp PROPERTY CXX_STANDARD 11)
target_link_libraries(test_cpp PRIVATE GDAL::GDAL)
