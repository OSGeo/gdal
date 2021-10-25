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

    Where to install the software.
    Default for Unix-like is ``/usr/local/``.

.. option:: CMAKE_PREFIX_PATH

    List of directories specifying installation prefixes to be searched when
    external dependencies are looked for.

    Starting with CMake 3.12, it is also possible to use a
    ``<Packagename>_ROOT`` variable to define the prefix for a particular
    package. See https://cmake.org/cmake/help/latest/release/3.12.html?highlight=root#commands

.. option:: ENABLE_IPO=OFF

    Build library using the compiler's `interprocedural optimization
    <https://en.wikipedia.org/wiki/Interprocedural_optimization>`_
    (IPO), if available, default OFF.

CMake package dependent options
+++++++++++++++++++++++++++++++

.. Put packages in alphabetic order.

Generally speaking, packages (external dependencies) will be automatically found if
they are in default locations used by CMake. This can be also tuned for example
with the ``CMAKE_PREFIX_PATH`` variable.

Starting with CMake 3.12, it is also possible to use a
``<Packagename>_ROOT`` variable to define the prefix for a particular
package. See https://cmake.org/cmake/help/latest/release/3.12.html?highlight=root#commands

Most dependencies that would be found can also be disabled by setting the
following option:

.. option:: GDAL_USE_<Packagename>:BOOL=ON/OFF

    Control whether a found dependency can be used for the GDAL build.

curl
****

.. option:: CURL_INCLUDE_DIR

    Path to an include directory with the ``curl`` directory.

.. option:: CURL_LIBRARY_RELEASE

    Path to a shared or static library file, such as ``libcurl.dll``,
    ``libcurl.so``, ``libcurl.lib``, or other name.

.. option:: GDAL_USE_CURL=ON/OFF

    Control whether to use Curl. Defaults to ON when Curl is found.


geotiff
*******

.. option:: GEOTIFF_INCLUDE_DIR

    Path to an include directory with the libgeotiff header files.

.. option:: GEOTIFF_LIBRARY

    Path to a shared or static library file such as libgeotiff.so

.. option:: GDAL_USE_GEOTIFF=ON/OFF

    Control whether to use external libgeotiff. Defaults to ON when external libgeotiff is found.

.. option:: GDAL_USE_LIBGEOTIFF_INTERNAL=ON/OFF

    Control whether to use internal libgeotiff copy. Defaults to ON when external
    libgeotiff is not found.


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


TIFF
****

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

TileDB
******

Specify install prefix in the ``CMAKE_PREFIX_PATH`` variable.


Selection of drivers
++++++++++++++++++++

By default, all drivers that have their build requirements satisfied will be
built-in in the GDAL core library.

The following options are available to select a subset of drivers:

.. option:: GDAL_ENABLE_FRMT_<driver_name>:BOOL=ON/OFF

.. option:: OGR_ENABLE_<driver_name>:BOOL=ON/OFF

    Independently of options that control global behavior, drivers can be individually
    enabled or disabled with those options.

.. option:: GDAL_BUILD_OPTIONAL_DRIVERS:BOOL=ON/OFF

.. option:: OGR_BUILD_OPTIONAL_DRIVERS:BOOL=ON/OFF

    Globally enable/disable all GDAL/raster or OGR/vector drivers.
    More exactly, setting those variables to ON affect the default value of the
    ``GDAL_ENABLE_FRMT_<driver_name>`` or ``OGR_ENABLE_<driver_name>`` variables
    (when they are not yet set).

    This can be combined with individual activation of a subset of drivers by using
    the ``GDAL_ENABLE_FRMT_<driver_name>:BOOL=ON`` or ``OGR_ENABLE_<driver_name>:BOOL=ON``
    variables. Note that changing the value of GDAL_BUILD_OPTIONAL_DRIVERS/
    OGR_BUILD_OPTIONAL_DRIVERS after a first run of CMake does not change the
    activation of individual drivers. It might be needed to pass
    ``-UGDAL_ENABLE_FRMT_* -UOGR_ENABLE_*`` to reset their state.


Example of minimal build with the JP2OpenJPEG and SVG drivers enabled::

    cmake .. -UGDAL_ENABLE_FRMT_* -UOGR_ENABLE_* \
             -DGDAL_BUILD_OPTIONAL_DRIVERS:BOOL=OFF -DOGR_BUILD_OPTIONAL_DRIVERS:BOOL=OFF \
             -DGDAL_ENABLE_FRMT_JP2OPENPEG:BOOL=ON \
             -DOGR_ENABLE_SVG:BOOL=ON

Build drivers as plugins
++++++++++++++++++++++++

An important subset, but not all, drivers can be also built as plugin, that is
to say as standalone .dll/.so shared libraries, to be installed in the ``gdalplugins``
subdirectory of the GDAL installation. This can be useful in particular for
drivers that depend on libraries that have a license different (proprietary, copyleft, ...)
from the core GDAL library.

The list of drivers that can be built as plugins can be obtained with::

    cmake .. -L | grep -e "_ENABLE.*PLUGIN"

The following options are available to select the plugin/builtin status of
a driver:

.. option:: GDAL_ENABLE_FRMT_<driver_name>_PLUGIN:BOOL=ON/OFF

.. option:: OGR_ENABLE_<driver_name>_PLUGIN:BOOL=ON/OFF

    Independently of options that control global behavior, drivers can be individually
    enabled or disabled with those options.

    Note that for the driver to be built, the corresponding base
    ``GDAL_ENABLE_FRMT_{driver_name}:BOOL=ON`` or ``OGR_ENABLE_{driver_name}:BOOL=ON`` option must
    be set.

.. option:: GDAL_ENABLE_PLUGINS:BOOL=ON/OFF

    Globally enable/disable building all (plugin capable), GDAL and OGR, drivers as plugins.
    More exactly, setting that variable to ON affects the default value of the
    ``GDAL_ENABLE_FRMT_<driver_name>_PLUGIN`` or ``OGR_ENABLE_<driver_name>_PLUGIN``
    variables (when they are not yet set).

    This can be combined with individual activation/deactivation of the plugin status with the
    ``GDAL_ENABLE_FRMT_{driver_name}_PLUGIN:BOOL`` or ``OGR_ENABLE_{driver_name}_PLUGIN:BOOL`` variables.
    Note that changing the value of GDAL_ENABLE_PLUGINS after a first
    run of CMake does not change the activation of the plugin status of individual drivers.
    It might be needed to pass ``-UGDAL_ENABLE_FRMT_* -UOGR_ENABLE_*`` to reset their state.


Example of build with all potential drivers as plugins, except the JP2OpenJPEG one::

    cmake .. -UGDAL_ENABLE_FRMT_* -UOGR_ENABLE_* \
             -DGDAL_ENABLE_PLUGINS:BOOL=ON \
             -DGDAL_ENABLE_FRMT_JP2OPENPEG_PLUGIN:BOOL=OFF

There is a subtelty regarding ``GDAL_ENABLE_PLUGINS:BOOL=ON``. It only controls
the plugin status of plugin-capable drivers that have external dependencies,
that are not part of GDAL core dependencies (e.g. are netCDF, HDF4, Oracle, PDF, etc.).

.. option:: GDAL_ENABLE_PLUGINS_NO_DEPS:BOOL=ON/OFF

    Globally enable/disable building all (plugin capable), GDAL and OGR, drivers as plugins,
    for drivers that have no external dependencies (e.g. BMP, FlatGeobuf), or that have
    dependencies that are part of GDAL core dependencies (e.g GPX).
    Building such drivers as plugins is generally not necessary, hence
    the use of a different option from GDAL_ENABLE_PLUGINS.


Building on Windows with Conda dependencies and Visual Studio
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
    conda install --yes --quiet -c conda-forge compilers
    conda install --yes --quiet -c conda-forge \
        cmake proj geos hdf4 hdf5 \
        libnetcdf openjpeg poppler libtiff libpng xerces-c expat libxml2 kealib json-c \
        cfitsio freexl geotiff jpeg libpq libspatialite libwebp-base pcre postgresql \
        sqlite tiledb zstd charls cryptopp cgal jasper librttopo libkml openssl xz

.. note::

    The ``compilers`` package will install ``vs2017_win-64`` (at time of writing)
    to set the appropriate environment for cmake to pick up. It is also possible
    to use the ``vs2019_win-64`` package if Visual Studio 2019 is to be used.

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
