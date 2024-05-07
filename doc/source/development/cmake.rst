.. _using_gdal_in_cmake:

********************************************************************************
Using GDAL in CMake projects
********************************************************************************

.. versionadded:: 3.5

The recommended way to use the GDAL library 3.5 or higher in a CMake project is to
link to the imported library target ``GDAL::GDAL`` provided by
the CMake configuration which comes with the library. Typical usage is:

.. code::

    find_package(GDAL CONFIG REQUIRED)

    target_link_libraries(MyApp PRIVATE GDAL::GDAL)

By adding the imported library target ``GDAL::GDAL`` to the
target link libraries, CMake will also pass the include directories to
the compiler.

The CMake command ``find_package`` will look for the configuration in a
number of places. The lookup can be adjusted for all packages by setting
the cache variable or environment variable ``CMAKE_PREFIX_PATH``. In
particular, CMake will consult (and set) the cache variable
``GDAL_DIR``.

Before GDAL 3.5, it is recommended to use `find module supplied with CMake <https://cmake.org/cmake/help/latest/module/FindGDAL.html>`__.
This also creates the ``GDAL::GDAL`` target. It requires CMake version 3.14.

.. code::

    cmake_minimum_required(VERSION 3.14)

    find_package(GDAL CONFIG)
    if(NOT GDAL_FOUND)
        find_package(GDAL REQUIRED)
    endif()

    target_link_libraries(MyApp PRIVATE GDAL::GDAL)
