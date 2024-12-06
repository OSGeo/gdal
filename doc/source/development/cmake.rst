.. _using_gdal_in_cmake:

********************************************************************************
Using GDAL in CMake projects
********************************************************************************

.. versionadded:: 3.5

The recommended way to use the GDAL library 3.5 or higher in a CMake project is to
link to the imported library target ``GDAL::GDAL`` provided by
the CMake configuration which comes with the library. Typical usage is:

.. code:: cmake

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

If a specific minor version is at least required, you can search for this via:

.. code:: cmake

    find_package(GDAL 3.11 CONFIG REQUIRED)

If only a single minor version should be accepted, you need to pass a version
range to ``find_package`` (requires CMake 3.19):

.. code:: cmake

    find_package(GDAL 3.11.0...3.11.9 CONFIG REQUIRED)

.. versionchanged:: 3.11
   Until GDAL 3.10, GDAL has specified the same minor version as compatibility.
   If you want older GDAL versions to be accepted, the version number must be
   checked manually.

   .. code:: cmake

       find_package(GDAL CONFIG REQUIRED)
       if(GDAL_VERSION VERSION_LESS "3.7" OR GDAL_VERSION VERSION_GREATER "3.11")
         message(FATAL_ERROR "Required at least GDAL version 3.7 - 3.11, but found ${GDAL_VERSION}.")
       endif()

Before GDAL 3.5, it is recommended to use `find module supplied with CMake <https://cmake.org/cmake/help/latest/module/FindGDAL.html>`__.
This also creates the ``GDAL::GDAL`` target. It requires CMake version 3.14.

.. code:: cmake

    cmake_minimum_required(VERSION 3.14)

    find_package(GDAL CONFIG)
    if(NOT GDAL_FOUND)
        find_package(GDAL REQUIRED)
    endif()

    target_link_libraries(MyApp PRIVATE GDAL::GDAL)
