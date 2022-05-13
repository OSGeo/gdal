cmake_minimum_required(VERSION 3.0)

project(test_c C CXX) # CXX for properly linking GDAL

find_package(GDAL CONFIG REQUIRED)

add_executable(test_c test_c.c)
target_link_libraries(test_c PRIVATE GDAL::GDAL)
