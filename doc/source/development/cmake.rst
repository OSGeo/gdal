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

Before GDAL 3.5, you can use the following to create the imported library target ``GDAL::GDAL``:

.. code::

    find_package(GDAL CONFIG QUIET)
    if(NOT TARGET GDAL::GDAL)
        find_package(GDAL REQUIRED)
        if(NOT TARGET GDAL::GDAL)
            add_library(GDAL IMPORTED)
            if(DEFINED GDAL_LIBRARIES)
                target_link_libraries(GDAL INTERFACE "${GDAL_LIBRARIES}")
                add_library(GDAL::GDAL ALIAS GDAL)
            else()
                message(FATAL_ERROR "Missing GDAL_LIBRARIES")
            endif()
        endif()
    endif()

    target_link_libraries(MyApp PRIVATE GDAL::GDAL)
