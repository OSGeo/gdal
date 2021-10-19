.. _build_hints:

================================================================================
Build hints (cmake)
================================================================================

Build requirements
--------------------------------------------------------------------------------

The minimum requirements are:

- CMake >= 3.10, and an associated build system (make, ninja, Visual Studio, etc.)
- C99 compiler
- C++11 compiler
- PROJ >= 6.0

But a number of optional libraries are also strongly recommended for most builds:
SQLite3, expat, libcurl, zlib, libtiff, libgeotiff, libpng, libjpeg, etc.

CMake
--------------------------------------------------------------------------------

With the CMake build system you can compile and install GDAL on more or less any
platform. After unpacking the source distribution archive step into the source-
tree::

    cd gdal-{VERSION}

Create a build directory and step into it::

    mkdir build
    cd build

From the build directory you can now configure CMake, build and install the binaries::

    cmake ..
    cmake --build .
    cmake --build . --target install

On Windows, one may need to specify generator::

    cmake -G "Visual Studio 15 2017" ..

If a dependency is installed in a custom location, specify the
paths to the include directory and the library::

    cmake -DSQLITE3_INCLUDE_DIR=/opt/SQLite/include -DSQLITE3_LIBRARY=/opt/SQLite/lib/libsqlite3.so ..

Alternatively, a custom prefix can be specified::

    cmake -DCMAKE_PREFIX_PATH=/opt/SQLite ..

You can unset existing cached variables, by using the -U switch of cmake, for example with wildcards::

    cmake .. -UGDAL_USE_*

CMake general configure options
+++++++++++++++++++++++++++++++

Options to configure a CMake are provided using ``-D<var>=<value>``.
All cached entries can be viewed using ``cmake -LAH`` from a build directory.

.. option:: BUILD_APPS=ON

    Build applications. Default is ON.

.. option:: BUILD_SHARED_LIBS

    Build GDAL library shared. Default is ON. See also the CMake
    documentation for `BUILD_SHARED_LIBS
    <https://cmake.org/cmake/help/v3.10/variable/BUILD_SHARED_LIBS.html>`_.

.. option:: CMAKE_BUILD_TYPE

    Choose the type of build, options are: None (default), Debug, Release,
    RelWithDebInfo, or MinSizeRel. See also the CMake documentation for
    `CMAKE_BUILD_TYPE
    <https://cmake.org/cmake/help/v3.10/variable/CMAKE_BUILD_TYPE.html>`_.

    .. note::
        A default build is not optimized without specifying
        ``-DCMAKE_BUILD_TYPE=Release`` (or similar) during configuration,
        or by specifying ``--config Release`` with CMake
        multi-configuration build tools (see example below).

.. option:: CMAKE_C_COMPILER

    C compiler. Ignored for some generators, such as Visual Studio.

.. option:: CMAKE_C_FLAGS

    Flags used by the C compiler during all build types. This is
    initialized by the :envvar:`CFLAGS` environment variable.

.. option:: CMAKE_CXX_COMPILER

    C++ compiler. Ignored for some generators, such as Visual Studio.

.. option:: CMAKE_CXX_FLAGS

    Flags used by the C++ compiler during all build types. This is
    initialized by the :envvar:`CXXFLAGS` environment variable.

.. option:: CMAKE_INSTALL_PREFIX

    Default for Windows is based on the environment variable
    :envvar:`OSGEO4W_ROOT` (if set), otherwise is ``c:/OSGeo4W``.
    Default for Unix-like is ``/usr/local/``.

.. option:: ENABLE_IPO=OFF

    Build library using the compiler's `interprocedural optimization
    <https://en.wikipedia.org/wiki/Interprocedural_optimization>`_
    (IPO), if available, default OFF.

CMake package dependent options
+++++++++++++++++++++++++++++++

.. Put packages in alphabetic order.

curl
****

.. option:: CURL_INCLUDE_DIR

    Path to an include directory with the ``curl`` directory.

.. option:: CURL_LIBRARY_RELEASE

    Path to a shared or static library file, such as ``libcurl.dll``,
    ``libcurl.so``, ``libcurl.lib``, or other name.

.. option:: GDAL_USE_CURL=ON/OFF

    Control whether to use Curl. Defaults to ON when Curl is found.


libgeotiff
**********

.. option:: GEOTIFF_INCLUDE_DIR

    Path to an include directory with the libgeotiff header files.

.. option:: GEOTIFF_LIBRARY

    Path to a shared or static library file such as libgeotiff.so

.. option:: GDAL_USE_GEOTIFF=ON/OFF

    Control whether to use external libgeotiff. Defaults to ON when external libgeotiff is found.

.. option:: GDAL_USE_LIBGEOTIFF_INTERNAL=ON/OFF

    Control whether to use internal libgeotiff copy. Defaults to ON when external
    libgeotiff is not found.


libtiff
*******

.. option:: TIFF_INCLUDE_DIR

    Path to an include directory with the ``tiff.h`` header file.

.. option:: TIFF_LIBRARY_RELEASE

    Path to a shared or static library file, such as ``tiff.dll``,
    ``libtiff.so``, ``tiff.lib``, or other name. A similar variable
    ``TIFF_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_TIFF=ON/OFF

    Control whether to use external libtiff. Defaults to ON when external libtiff is found.

.. option:: GDAL_USE_LIBTIFF_INTERNAL=ON/OFF

    Control whether to use internal libtiff copy. Defaults to ON when external
    libtiff is not found.


PROJ
****

.. option:: PROJ_INCLUDE_DIR

    Path to an include directory with the ``proj.h`` header file.

.. option:: PROJ_LIBRARY

    Path to a shared or static library file.


SQLite3
*******

.. option:: SQLITE3_INCLUDE_DIR

    Path to an include directory with the ``sqlite3.h`` header file.

.. option:: SQLITE3_LIBRARY

    Path to a shared or static library file, such as ``sqlite3.dll``,
    ``libsqlite3.so``, ``sqlite3.lib`` or other name.

.. option:: GDAL_USE_SQLITE3=ON/OFF

    Control whether to use SQLite3. Defaults to ON when SQLite3 is found.


Selection of drivers
++++++++++++++++++++

TODO

Building on Windows with Conda dependencies and Visual Studio 2017
--------------------------------------------------------------------------------

It is less appropriate for Debug builds of GDAL, than other methods, such as using vcpkg.

Install git
+++++++++++

Install `git <https://git-scm.com/download/win>`_

Install miniconda
+++++++++++++++++

Install `miniconda <https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe>`_

Install GDAL dependencies
+++++++++++++++++++++++++

Start a Conda enabled console and assuming there is a c:\\dev directory

::

    cd c:\dev
    conda create --name gdal
    conda activate gdal
    conda install --yes --quiet curl libiconv icu git python=3.7 swig numpy pytest zlib clcache
    conda install --yes --quiet -c conda-forge \
        proj geos hdf4 hdf5 \
        libnetcdf openjpeg poppler libtiff libpng xerces-c expat libxml2 kealib json-c \
        cfitsio freexl geotiff jpeg libpq libspatialite libwebp-base pcre postgresql \
        sqlite tiledb zstd charls cryptopp cgal jasper librttopo libkml openssl xz

Checkout GDAL sources
+++++++++++++++++++++

::

    cd c:\dev
    git clone https://github.com/OSGeo/gdal.git

Build GDAL
++++++++++

From a Conda enabled console

::

    conda activate gdal
    cd c:\dev\gdal
    call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
    cmake -S . -B build -DCMAKE_PREFIX_PATH:FILEPATH="%CONDA_PREFIX%" \
                        -DCMAKE_C_COMPILER_LAUNCHER=clcache
                        -DCMAKE_CXX_COMPILER_LAUNCHER=clcache
    cmake --build build --config Release -j 8

.. only:: FIXME

    Run GDAL tests
    ++++++++++++++

    ::

        cd c:\dev\GDAL
        cd _build.vs2019
        ctest -V --build-config Release
