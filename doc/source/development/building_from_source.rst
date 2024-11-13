.. _building_from_source:

================================================================================
Building GDAL from source
================================================================================

.. _build_requirements:

Build requirements
--------------------------------------------------------------------------------

The minimum requirements to build GDAL are:

- CMake >= 3.16, and an associated build system (make, ninja, Visual Studio, etc.)
- C99 compiler
- C++17 compiler since GDAL 3.9 (C++11 in previous versions)
- PROJ >= 6.3.1

Additional requirements to run the GDAL test suite are:

- SWIG >= 4, for building bindings to other programming languages
- Python >= 3.8
- Python packages listed in `autotest/requirements.txt`

A number of optional libraries are also strongly recommended for most builds:
SQLite3, expat, libcurl, zlib, libtiff, libgeotiff, libpng, libjpeg, etc.
Consult :ref:`raster_drivers` and :ref:`vector_drivers` pages for information
on dependencies of optional drivers.

CMake (GDAL versions >= 3.5.0)
--------------------------------------------------------------------------------

Since version 3.5.0, GDAL can be built using the CMake build system.
With the CMake build system you can compile and install GDAL on more or less any
platform. After unpacking the source distribution archive (or cloning the repository)
step into the source tree:

.. code-block:: bash

    cd gdal-{VERSION}

Create a build directory and step into it:

.. code-block:: bash

    mkdir build
    cd build

From the build directory you can now configure CMake, build and install the binaries:

.. code-block:: bash

    cmake ..
    cmake --build .
    cmake --build . --target install

.. note::

    For a minimal build, add these options to the initial ``cmake`` command: ``-DGDAL_BUILD_OPTIONAL_DRIVERS=OFF -DOGR_BUILD_OPTIONAL_DRIVERS=OFF``.
    To enable specific drivers, add ``-DGDAL_ENABLE_DRIVER_<driver_name>=ON`` or ``-DOGR_ENABLE_DRIVER_<driver_name>=ON``.
    See :ref:`selection-of-drivers` for more details.

.. note::

    The ``--prefix /installation/prefix`` option of CMake (>= 3.14) is supported since GDAL 3.7.0,
    but note that contrary to setting the CMAKE_INSTALL_PREFIX at configuration time,
    it will not result in the GDAL_DATA path to be hardcoded into the libgdal binary,
    and is thus not recommended. It is also not supported on Windows multi-configuration
    generator (such as VisualStudio).


If a dependency is installed in a custom location, specify the
paths to the include directory and the library:

.. code-block:: bash

    cmake -DSQLite3_INCLUDE_DIR=/opt/SQLite/include -DSQLite3_LIBRARY=/opt/SQLite/lib/libsqlite3.so ..

Alternatively, a custom prefix can be specified:

.. code-block:: bash

    cmake -DCMAKE_PREFIX_PATH=/opt/SQLite ..

It is strongly recommended (and sometimes compulsory) to specify paths on Windows
using forward slashes as well, e.g.: ``c:/path/to/include``.

You can unset existing cached variables, by using the -U switch of cmake, for example with wildcards:

.. code-block:: bash

    cmake .. -UGDAL_USE_*

You can assemble dependency settings in a file ``ConfigUser.cmake`` and use it with the -C option.
The file contains set() commands that use the CACHE option. You can set for example a different name
for the shared lib, *e.g.* ``set (GDAL_LIB_OUTPUT_NAME gdal_x64 CACHE STRING "" FORCE)``:

.. code-block:: bash

    cmake .. -C ConfigUser.cmake

.. warning::

    When iterating to configure GDAL to add/modify/remove dependencies,
    some cache variables can remain in CMakeCache.txt from previous runs, and
    conflict with new settings. If strange errors appear during cmake run,
    you may try removing CMakeCache.txt to start from a clean state.

Refer to :ref:`using_gdal_in_cmake` for how to use GDAL in a CMake project.

Building on Windows
+++++++++++++++++++

On Windows, one may need to specify generator:

.. code-block:: bash

    cmake -G "Visual Studio 15 2017" ..


Building on MacOS
+++++++++++++++++

On MacOS, there are a couple of libraries that do not function properly when the GDAL build requirements are installed using Homebrew.

The `Apache Arrow <https://arrow.apache.org/docs/index.html>`_ library included in the current distribution of Homebrew is broken, and causes a detection issue. In order to build GDAL successfully, configure CMake to not find the Arrow package:

.. code-block:: bash

    cmake -DCMAKE_DISABLE_FIND_PACKAGE_Arrow=ON ..


Similarly, recent versions of Homebrew no longer bundle `Boost <https://www.boost.org/>`_ with libkml, causing a failure to find Boost headers. You should either install Boost manually or disable libkml when building on MacOS:

.. code-block:: bash

    cmake -DGDAL_USE_LIBKML=OFF ..


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

.. option:: CMAKE_UNITY_BUILD=OFF

    .. versionadded:: 3.9

    Default is OFF. This can be set to ON to build GDAL using the
    https://cmake.org/cmake/help/latest/variable/CMAKE_UNITY_BUILD.html feature.
    This helps speeding GDAL build times. This feature is still considered
    experimental for now, and could hide subtle bugs (we are not aware of
    any at writing time though). We don't recommend it for mission critical
    builds.

.. option:: ENABLE_IPO=OFF

    Build library using the compiler's `interprocedural optimization
    <https://en.wikipedia.org/wiki/Interprocedural_optimization>`_
    (IPO), if available, default OFF.

.. option:: GDAL_SET_INSTALL_RELATIVE_RPATH=OFF

    Set to ON so that the rpath of installed binaries is written as a relative
    path to the library. This option overrides the
    `CMAKE_INSTALL_RPATH <https://cmake.org/cmake/help/latest/variable/CMAKE_INSTALL_RPATH.html>`__
    variable, and assumes that the
    `CMAKE_SKIP_INSTALL_RPATH <https://cmake.org/cmake/help/latest/variable/CMAKE_SKIP_INSTALL_RPATH.html>`__
    variable is not set.

Resource files embedding
++++++++++++++++++++++++

Starting with GDAL 3.11, if a C23-compatible compiler is used, such as
clang >= 19 or GCC >= 15, it is possible to embed resource files inside
the GDAL library, without relying on resource files to be available on the file
system (such resource files are located through an hard-coded
path at build time in ``${CMAKE_INSTALL_PREFIX}/share/gdal``, or at run-time
through the :config:`GDAL_DATA` configuration option).

The following CMake options control that behavior:

.. option:: EMBED_RESOURCE_FILES=ON/OFF

    .. versionadded:: 3.11

    Default is OFF for shared library builds (BUILD_SHARED_LIBS=ON), and ON
    for static library builds (BUILD_SHARED_LIBS=OFF).
    When ON, resource files needed by GDAL will be embedded into the GDAL library
    and/or relevant plugins.

.. option:: USE_ONLY_EMBEDDED_RESOURCE_FILES=ON/OFF

    .. versionadded:: 3.11

    Even if EMBED_RESOURCE_FILES=ON, GDAL will still try to locate resource
    files on the file system by default , and fallback to the embedded version if
    not found. By setting USE_ONLY_EMBEDDED_RESOURCE_FILES=ON, no attempt
    at locating resource files on the file system is made. Default is OFF.

CMake package dependent options
+++++++++++++++++++++++++++++++

.. Put packages in alphabetic order.

Generally speaking, packages (external dependencies) will be automatically found if
they are in default locations used by CMake. This can be also tuned for example
with the ``CMAKE_PREFIX_PATH`` variable.

Starting with CMake 3.12, it is also possible to use a
``<Packagename>_ROOT`` variable to define the prefix for a particular
package. See https://cmake.org/cmake/help/latest/release/3.12.html?highlight=root#commands
Note that the case of the package name matters for the _ROOT, _INCLUDE_DIR
and _LIBRARY variables.

Most dependencies that would be found can also be disabled by setting the
following option:

.. option:: GDAL_USE_<Packagename_in_upper_case>:BOOL=ON/OFF

    Control whether a found dependency can be used for the GDAL build.

It is also possible to ask GDAL to disable the use of any external dependency
(besides the required one, PROJ) by default by setting the following option to
OFF. Individual libraries shall then be enabled explicitly with
GDAL_USE_<Packagename_in_upper_case>:BOOL=ON.

.. option:: GDAL_USE_EXTERNAL_LIBS:BOOL=ON/OFF

     Defaults to ON. When set to OFF, all external dependencies (but mandatory
     ones) will be disabled, unless individually enabled with
     GDAL_USE_<Packagename_in_upper_case>:BOOL=ON.
     This option should be set before CMakeCache.txt is created. If it is set
     to OFF after CMakeCache.txt is created, then cmake should be reinvoked with
     "-UGDAL_USE_*" to cancel the activation of previously detected libraries.

Some of the GDAL dependencies (GEOTIFF, GIF, JPEG, JSONC, LERC, OPENCAD, PNG, QHULL, TIFF, ZLIB)
have a copy of their source code inside the GDAL source code tree. It is possible
to enable this internal copy by setting the GDAL_USE_<Packagename_in_upper_case>_INTERNAL:BOOL=ON
variable. When set, this has precedence over the external library that may be
detected. The behavior can also be globally controlled with the following variable:

.. option:: GDAL_USE_INTERNAL_LIBS=ON/OFF/WHEN_NO_EXTERNAL

     Control how internal libraries should be used.
     If set to ON, they will always be used.
     If set to OFF, they will never be used (unless individually enabled with
     GDAL_USE_<Packagename_in_upper_case>_INTERNAL:BOOL=ON)
     If set to WHEN_NO_EXTERNAL (default value), they will be used only if no
     corresponding external library is found and enabled.
     This option should be set before CMakeCache.txt is created.


.. note::

    Using together GDAL_USE_EXTERNAL_LIBS=OFF and GDAL_USE_INTERNAL_LIBS=OFF
    will result in a CMake configuration failure, because the following libraries
    (either as external dependencies or using the internal copy) are at least
    required: ZLIB, TIFF, GEOTIFF and JSONC. Enabling them as external or internal
    libraries is thus required.


Archive
*******

`libarchive <https://www.libarchive.org/>`_ is a library that supports a variety
of archive and compression formats. It might be used since GDAL 3.7 to enable
the :ref:`/vsi7z/ <vsi7z>` virtual file system.

.. option:: ARCHIVE_INCLUDE_DIR

    Path to an include directory with the ``archive.h`` header file.

.. option:: ARCHIVE_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_ARCHIVE=ON/OFF

    Control whether to use libarchive. Defaults to ON when libarchive is found.


Armadillo
*********

The `Armadillo <http://arma.sourceforge.net/>`_ C++ library is used to speed up computations related to the
Thin Plate Spline transformer. See https://cmake.org/cmake/help/latest/module/FindArmadillo.html
for details.
On Windows builds using Conda-Forge dependencies, the following packages may also
need to be installed: ``blas blas-devel libblas libcblas liblapack liblapacke``

.. option:: GDAL_USE_ARMADILLO=ON/OFF

    Control whether to use Armadillo. Defaults to ON when Armadillo is found.


Arrow
*****

The `Apache Arrow C++ <https://github.com/apache/arrow/tree/master/cpp>` library
is required for the :ref:`vector.arrow` and :ref:`vector.parquet` drivers.
Specify install prefix in the ``CMAKE_PREFIX_PATH`` variable.

.. option:: GDAL_USE_ARROW=ON/OFF

    Control whether to use Arrow. Defaults to ON when Arrow is found.

.. option:: ARROW_USE_STATIC_LIBRARIES=ON/OFF

    Control whether to use statically built Arrow libraries. Defaults to OFF when Arrow is found.

basisu
******

The `Basis Universal <https://github.com/rouault/basis_universal/tree/cmake>` library
is required for the :ref:`raster.basisu` and :ref:`raster.ktx2` drivers.
Specify install prefix in the ``CMAKE_PREFIX_PATH`` variable or ``basisu_ROOT`` variable.

.. option:: GDAL_USE_BASISU=ON/OFF

    Control whether to use basisu. Defaults to ON when basisu is found.


Blosc
*****

`Blosc <https://github.com/Blosc/c-blosc>`_ is a library which offers
a meta-compression, with different backends (LZ4, Snappy, Zlib, Zstd, etc.).
It is used by the :ref:`raster.zarr` driver.

.. option:: BLOSC_INCLUDE_DIR

    Path to an include directory with the ``blosc.h`` header file.

.. option:: BLOSC_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_BLOSC=ON/OFF

    Control whether to use Blosc. Defaults to ON when Blosc is found.


BRUNSLI
*******

The `Brunsli <https://github.com/google/brunsli>`_ JPEG repacking library, used
by the :ref:`raster.marfa` driver.

.. option:: BRUNSLI_INCLUDE_DIR

    Path to an include directory with the ``brunsli/decode.h`` and ``brunsli\encode.h`` header files.

.. option:: BRUNSLI_ENC_LIB

    Path to the brunslienc-c library file.

.. option:: BRUNSLI_DEC_LIB

    Path to the brunslidec-c library file.

.. option:: GDAL_USE_BRUNSLI=ON/OFF

    Control whether to use BRUNSLI. Defaults to ON when Brunsli is found.


CFITSIO
*******

The `C FITS I/O <https://heasarc.gsfc.nasa.gov/fitsio/>`_ library is required for the :ref:`raster.fits` driver.
It can be detected with pkg-config.

.. option:: CFITSIO_INCLUDE_DIR

    Path to an include directory with the ``fitsio.h`` header file.

.. option:: CFITSIO_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_CFITSIO=ON/OFF

    Control whether to use CFITSIO. Defaults to ON when CFITSIO is found.


Crnlib
******

`Crnlib / crunch <https://github.com/rouault/crunch/tree/build_fixes>`_ is
required for the :ref:`raster.dds` driver.

.. option:: Crnlib_INCLUDE_DIR

  Path to Crnlib include directory with ``crunch/crnlib.h`` header.

.. option:: Crnlib_LIBRARY

  Path to Crnlib library to be linked.

.. option:: GDAL_USE_CRNLIB=ON/OFF

    Control whether to use Crnlib. Defaults to ON when Crnlib is found.


CURL
****

`libcurl <https://curl.se/>`_ is required for all network accesses (HTTP, etc.).

.. option:: CURL_INCLUDE_DIR

    Path to an include directory with the ``curl`` directory.

.. option:: CURL_LIBRARY_RELEASE

    Path to a shared or static library file, such as
    ``libcurl.so``, ``libcurl.lib``, or other name.

.. option:: CURL_USE_STATIC_LIBS=ON/OFF

    .. versionadded:: 3.7.1

    Must be set to ON when linking against a static build of Curl.

.. option:: GDAL_USE_CURL=ON/OFF

    Control whether to use Curl. Defaults to ON when Curl is found.


CryptoPP
********

The `Crypto++ <https://github.com/weidai11/cryptopp>`_ library can be used for the RSA SHA256 signing
functionality used by some authentication methods of Google Cloud. It might be
required to use the :ref:`raster.eedai` images or use the :ref:`/vsigs/ <vsigs>` virtual file
system.
It is also required for the :ref:`/vsicrypt/ <vsicrypt>` virtual file system.

.. option:: CRYPTOPP_INCLUDE_DIR

    Path to the base include directory.

.. option:: CRYPTOPP_LIBRARY_RELEASE

    Path to a shared or static library file.  A similar variable
    ``CRYPTOPP_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: CRYPTOPP_USE_ONLY_CRYPTODLL_ALG=ON/OFF

    Defaults to OFF. Might be required to set to ON when linking against
    cryptopp.dll

.. option:: GDAL_USE_CRYPTOPP=ON/OFF

    Control whether to use CryptoPP. Defaults to ON when CryptoPP is found.


Deflate
*******

`libdeflate <https://github.com/ebiggers/libdeflate>`_ is a compression library
which offers the lossless Deflate/Zip compression algorithm.
It offers faster performance than ZLib, but is not a full replacement for it,
consequently it must be used as a complement to ZLib.


.. option:: Deflate_INCLUDE_DIR

    Path to an include directory with the ``libdeflate.h`` header file.

.. option:: Deflate_LIBRARY_RELEASE

    Path to a shared or static library file. A similar variable
    ``Deflate_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_DEFLATE=ON/OFF

    Control whether to use Deflate. Defaults to ON when Deflate is found.


ECW
***

The Hexagon ECW SDK (closed source/proprietary) is required for the :ref:`raster.ecw` driver.
Currently only support for ECW SDK 3.3 and 5.5 is offered.

For ECW SDK 5.5, ECW_ROOT or CMAKE_PREFIX_PATH should point to the directory
into which there are include and lib subdirectories, typically
ending with ERDAS-ECW_JPEG_2000_SDK-5.5.0/Desktop_Read-Only.

.. option:: ECW_INCLUDE_DIR

    Path to the include directory with the ``NCSECWClient.h`` header file.

.. option:: ECW_LIBRARY

    Path to library file libNCSEcw

.. option:: ECWnet_LIBRARY

    Path to library file libNCSCnet (only needed for SDK 3.3)

.. option:: ECWC_LIBRARY

    Path to library file libNCSEcwC (only needed for SDK 3.3)

.. option:: NCSUtil_LIBRARY

    Path to library file libNCSUtil (only needed for SDK 3.3)

.. option:: GDAL_USE_ECW=ON/OFF

    Control whether to use ECW. Defaults to ON when ECW is found.


EXPAT
*****

`Expat <https://github.com/libexpat/libexpat>`_ is a stream-oriented XML parser
library which is required to enable XML parsing capabilities in an important
number of OGR drivers (GML, GeoRSS, GPX, KML, LVBAG, OSM, ODS, SVG, WFS, XSLX, etc.).
It is strongly recommended. Other driver such as ILI or GMLAS may also require
the XercesC library.

.. option:: EXPAT_INCLUDE_DIR

    Path to the include directory with the ``expat.h`` header file.

.. option:: EXPAT_LIBRARY

    Path to a shared or static library file.

.. option:: EXPAT_USE_STATIC_LIBS=ON/OFF

    .. versionadded:: 3.7.1

    Must be set to ON when linking against a static build of Expat.

.. option:: GDAL_USE_EXPAT=ON/OFF

    Control whether to use EXPAT. Defaults to ON when EXPAT is found.


FileGDB
*******

The `FileGDB SDK <https://github.com/Esri/file-geodatabase-api>`_ (closed source/proprietary)
is required for the :ref:`vector.filegdb` driver (not to be confused with
the :ref:`vector.openfilegdb` driver that has no external requirements)

FileGDB_ROOT or CMAKE_PREFIX_PATH should point to the directory of the SDK.

.. option:: FileGDB_INCLUDE_DIR

    Path to the include directory with the ``FileGDBAPI.h`` header file.

.. option:: FileGDB_LIBRARY

    Path to library file

.. option:: FileGDB_LIBRARY_RELEASE

    Path to Release library file (only used on Windows)

.. option:: FileGDB_LIBRARY_DEBUG

    Path to Debug library file (only used on Windows)

.. option:: GDAL_USE_FILEGDB=ON/OFF

    Control whether to use FileGDB. Defaults to ON when FileGDB is found.


FreeXL
******

The `FreeXL <https://www.gaia-gis.it/fossil/freexl/index>`_ library is required
for the :ref:`vector.xls` driver.

.. option:: FREEXL_INCLUDE_DIR

    Path to an include directory with the ``freexl.h`` header file.

.. option:: FREEXL_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_FREEXL=ON/OFF

    Control whether to use FreeXL. Defaults to ON when FreeXL is found.


FYBA
****

The `OpenFyba <https://github.com/kartverket/fyba>`_ libraries are needed to build the :ref:`vector.sosi` driver.

.. option:: FYBA_INCLUDE_DIR

    Path to an include directory with the ``fyba.h`` header file.

.. option:: FYBA_FYBA_LIBRARY

    Path to a library file ``fyba``

.. option:: FYBA_FYGM_LIBRARY

    Path to a library file ``fygm``

.. option:: FYBA_FYUT_LIBRARY

    Path to a library file ``fyut``

.. option:: GDAL_USE_FYBA=ON/OFF

    Control whether to use FYBA. Defaults to ON when FYBA is found.


GEOTIFF
*******

It is required for the :ref:`raster.gtiff` drivers, and a few other drivers.
If not found, an internal copy of libgeotiff can be used.

.. option:: GEOTIFF_INCLUDE_DIR

    Path to an include directory with the libgeotiff header files.

.. option:: GEOTIFF_LIBRARY_RELEASE

    Path to a shared or static library file, such as
    ``libgeotiff.so``, ``geotiff.lib``, or other name. A similar variable
    ``GEOTIFF_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_GEOTIFF=ON/OFF

    Control whether to use external libgeotiff. Defaults to ON when external libgeotiff is found.

.. option:: GDAL_USE_GEOTIFF_INTERNAL=ON/OFF

    Control whether to use internal libgeotiff copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_GEOTIFF=ON


GEOS
****

`GEOS <https://github.com/libgeos/geos>`_ is a C++ library for performing operations
on two-dimensional vector geometries. It is used as the backend for most geometry
processing operations available in OGR (intersection, buffer, etc.).
The ``geos-config`` program can be used to detect it.

.. option:: GEOS_INCLUDE_DIR

    Path to an include directory with the ``geos_c.h`` header file.

.. option:: GEOS_LIBRARY

    Path to a shared or static library file (libgeos_c).

.. option:: GDAL_USE_GEOS=ON/OFF

    Control whether to use GEOS. Defaults to ON when GEOS is found.


GIF
***

`giflib <http://giflib.sourceforge.net/>`_ is required for the :ref:`raster.gif` driver.
If not found, an internal copy can be used.

.. option:: GIF_INCLUDE_DIR

    Path to an include directory with the ``gif_lib.h`` header file.

.. option:: GIF_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_GIF=ON/OFF

    Control whether to use external giflib. Defaults to ON when external giflib is found.

.. option:: GDAL_USE_GIF_INTERNAL=ON/OFF

    Control whether to use internal giflib copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_GIF=ON


GTA
***

The `GTA <https://marlam.de/gta/>`_ library is required for the :ref:`raster.gta` driver.

.. option:: GTA_INCLUDE_DIR

    Path to an include directory with the ``gta/gta.h`` header file.

.. option:: GTA_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_GTA=ON/OFF

    Control whether to use GTA. Defaults to ON when GTA is found.


HEIF
****

The `HEIF <https://github.com/strukturag/libheif>`_ (>= 1.1) library used by the :ref:`raster.heif` driver.
It can be detected with pkg-config.

.. option:: HEIF_INCLUDE_DIR

    Path to an include directory with the ``libheif/heif.h`` header file.

.. option:: HEIF_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_HEIF=ON/OFF

    Control whether to use HEIF. Defaults to ON when HEIF is found.

HDF4
****

The `HDF4 <https://support.hdfgroup.org/products/hdf4/>`_ C library is needed
for the :ref:`raster.hdf4` driver.

.. option:: HDF4_INCLUDE_DIR

    Path to an include directory with the ``hdf.h`` header file.

.. option:: HDF4_df_LIBRARY_RELEASE

    Path to a shared or static ``dfalt`` or ``df`` library file. A similar variable
    ``HDF4_df_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: HDF4_mfhdf_LIBRARY_RELEASE

    Path to a shared or static ``mfhdfalt`` or ``mfhdf`` library file. A similar variable
    ``HDF4_mfhdf_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: HDF4_xdr_LIBRARY_RELEASE

    Path to a shared or static ``xdr`` library file. A similar variable
    ``HDF4_xdr_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.
    It is generally not needed for Linux builds

.. option:: HDF4_szip_LIBRARY_RELEASE

    Path to a shared or static ``szip`` library file. A similar variable
    ``HDF4_szip_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.
    It is generally not needed for Linux builds

.. option:: HDF4_COMPONENTS

    The value of this option is a list which defaults to ``df;mfhdf;xdr;szip``.
    It may be customized if the linking of HDF4 require different libraries,
    in which case HDF4_{comp_name}_LIBRARY_[RELEASE/DEBUG] variables will be
    available to configure the library file.

.. option:: GDAL_USE_HDF4=ON/OFF

    Control whether to use HDF4. Defaults to ON when HDF4 is found.


HDF5
****

The `HDF5 <https://github.com/HDFGroup/hdf5>`_ C library is needed for the
:ref:`raster.hdf5` and :ref:`raster.bag` drivers.
The HDF5 CXX library is needed for the :ref:`raster.kea` driver.
The https://cmake.org/cmake/help/latest/module/FindHDF5.html module is used to
detect the HDF5 library.

.. option:: GDAL_USE_HDF5=ON/OFF

    Control whether to use HDF5. Defaults to ON when HDF5 is found.

.. option:: GDAL_ENABLE_HDF5_GLOBAL_LOCK=ON/OFF

    Control whether to add a global lock around calls to HDF5 library. This is
    needed if the HDF5 library is not built with thread-safety enabled and if
    the HDF5 driver is used in a multi-threaded way. On Unix, a heuristics
    try to detect if the HDF5 library has been built with thread-safety enabled
    when linking against a HDF5 library. In other situations, the setting must
    be manually set when needed.


.. _building_from_source_hdfs:

HDFS
****

The `Hadoop File System <https://hadoop.apache.org/docs/stable/hadoop-project-dist/hadoop-hdfs/LibHdfs.html>`_ native library is needed
for the :ref:`/vsihdfs/ <vsihdfs>` virtual file system.

.. option:: HDFS_INCLUDE_DIR

    Path to an include directory with the ``hdfs.h`` header file.

.. option:: HDFS_LIBRARY

    Path to a shared or static ``hdfs`` library file.

.. option:: GDAL_USE_HDFS=ON/OFF

    Control whether to use HDFS. Defaults to ON when HDFS is found.


Iconv
*****

The `Iconv <https://www.gnu.org/software/libiconv/>`_ library is used to convert
text from one encoding to another encoding.
It is generally available as a system library for Unix-like systems. On Windows,
GDAL can leverage the API of the operating system for a few base conversions,
but using Iconv will provide additional capabilities.

.. option:: Iconv_INCLUDE_DIR

    Path to an include directory with the ``iconv.h`` header file.

.. option:: Iconv_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_ICONV=ON/OFF

    Control whether to use Iconv. Defaults to ON when Iconv is found.


IDB
***

The Informix DataBase Client SDK (closed source/proprietary)  is needed to build
the :ref:`vector.idb` driver.
IDB_ROOT or CMAKE_PREFIX_PATH should point to the directory of the SDK.


.. option:: IDB_INCLUDE_DIR

    Path to an include directory (typically ending with ``incl``) with the ``c++/it.h`` header file.

.. option:: IDB_IFCPP_LIBRARY

    Path to a library file ``ifc++`` (typically in the ``lib/c++`` sub directory)

.. option:: IDB_IFDMI_LIBRARY

    Path to a library file ``ifdmi`` (typically in the ``lib/dmi`` sub directory)

.. option:: IDB_IFSQL_LIBRARY

    Path to a library file ``ifsql`` (typically in the ``lib/esql`` sub directory)

.. option:: IDB_IFCLI_LIBRARY

    Path to a library file ``ifcli`` (typically in the ``lib/cli`` sub directory)

.. option:: GDAL_USE_IDB=ON/OFF

    Control whether to use IDB. Defaults to ON when IDB is found.


JPEG
****

libjpeg is required for the :ref:`raster.jpeg` driver, and may be used by a few
other drivers (:ref:`raster.gpkg`, :ref:`raster.marfa`, internal libtiff, etc.)
If not found, an internal copy of libjpeg (6b) can be used.
Using `libjpeg-turbo <https://github.com/libjpeg-turbo/libjpeg-turbo>`_ is highly
recommended to get best performance.
See https://cmake.org/cmake/help/latest/module/FindJPEG.html for more details
on how the library is detected.

.. note::

    When using libjpeg-turbo, JPEG_LIBRARY[_RELEASE/_DEBUG] should point to a
    library with libjpeg ABI, not TurboJPEG.
    See https://libjpeg-turbo.org/About/TurboJPEG for the difference.

.. option:: JPEG_INCLUDE_DIR

    Path to an include directory with the ``jpeglib.h`` header file.

.. option:: JPEG_LIBRARY_RELEASE

    Path to a shared or static library file. A similar variable
    ``JPEG_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_JPEG=ON/OFF

    Control whether to use external libjpeg. Defaults to ON when external libjpeg is found.

.. option:: GDAL_USE_JPEG_INTERNAL=ON/OFF

    Control whether to use internal libjpeg copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_JPEG=ON

.. option:: EXPECTED_JPEG_LIB_VERSION=number

    Used with external libjpeg. number is for example 80, for libjpeg 8 ABI.
    This can be used to check a build time that the expected JPEG library is
    the one that is included by GDAL.


JPEG12
******

libjpeg-12 bit can be used by the :ref:`raster.jpeg`, :ref:`raster.gtiff` (when using internal libtiff),
:ref:`raster.jpeg`, :ref:`raster.marfa` and :ref:`raster.nitf` drivers to handle
JPEG images with a 12 bit depth. It is only supported with the internal libjpeg (6b).
This can be used independently of if for regular 8 bit JPEG an external or internal
libjpeg is used.

.. option:: GDAL_USE_JPEG12_INTERNAL=ON/OFF

    Control whether to use internal libjpeg-12 copy. Defaults to ON.

.. note::

    Starting with GDAL 3.7, if using libjpeg-turbo >= 2.2, which adds native
    support for dual 8/12-bit, using internal libjpeg-12 is no longer needed to
    get 12-bit JPEG support in the JPEG, MRF, NITF or GeoTIFF (when built with
    internal libtiff) drivers. If using external libtiff, libtiff >= 4.5
    built against libjpeg-turbo >= 2.2 is needed to get 12-bit JPEG support in the
    GeoTIFF support.

JSON-C
******

The `json-c <https://github.com/json-c/json-c>`_ library is required to read and
write JSON content.
It can be detected with pkg-config.
If not found, an internal copy of json-c can be used.

.. option:: JSONC_INCLUDE_DIR

    Path to an include directory with the ``json.h`` header file.

.. option:: JSONC_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_JSONC=ON/OFF

    Control whether to use JSON-C. Defaults to ON when JSON-C is found.

.. option:: GDAL_USE_JSONC_INTERNAL=ON/OFF

    Control whether to use internal JSON-C copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_JSONC=ON


JXL
***

The `libjxl <https://github.com/libjxl/libjxl>` library used by the
:ref:`raster.gtiff` driver, when built against internal libtiff.
It can be detected with pkg-config.

.. option:: JXL_INCLUDE_DIR

    Path to an include directory with the ``jxl/decode.h`` header file.

.. option:: JXL_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_JXL=ON/OFF

    Control whether to use JXL. Defaults to ON when JXL is found.


KDU
***

The Kakadu library (proprietary) is required for the :ref:`raster.jp2kak` and
:ref:`raster.jpipkak` drivers.
There is no standardized installation layout, nor fixed library file names, so finding
Kakadu artifacts is a bit challenging. Currently automatic finding of it from
the KDU_ROOT variable is only implemented for Linux, Mac and Windows x86_64
builds. For other platforms, users need to manually specify the KDU_LIBRARY
and KDU_AUX_LIBRARY variable.

.. option:: KDU_INCLUDE_DIR

    Path to the root of the Kakadu build tree, from which the
    ``coresys/common/kdu_elementary.h`` header file should be found.

.. option:: KDU_LIBRARY

    Path to a shared library file whose name is like libkdu_vXYR.so on Unix
    or kdu_vXYR.lib on Windows, where X.Y is the Kakadu version.

.. option:: KDU_AUX_LIBRARY

    Path to a shared library file whose name is like libkdu_aXYR.so on Unix
    or kdu_aXYR.lib on Windows, where X.Y is the Kakadu version.

.. option:: GDAL_USE_KDU=ON/OFF

    Control whether to use KDU. Defaults to ON when KDU is found.

KEA
***

The `KEA <http://www.kealib.org/>`_ library is required for the :ref:`raster.kea`
driver. The HDF5 CXX library is also required.

.. option:: KEA_INCLUDE_DIR

    Path to an include directory with the ``libkea/KEACommon.h`` header file.

.. option:: KEA_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_KEA=ON/OFF

    Control whether to use KEA. Defaults to ON when KEA is found.


LERC
****

`LERC <https://github.com/esri/lerc>`_ is an open-source image or raster format
which supports rapid encoding and decoding for any pixel type (not just RGB or Byte).
Users set the maximum compression error per pixel while encoding, so the precision
of the original input image is preserved (within user defined error bounds).

.. option:: LERC_INCLUDE_DIR

    Path to an include directory with the ``Lerc_c_api.h`` header file.

.. option:: LERC_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_LERC=ON/OFF

    Control whether to use LERC. Defaults to ON when LERC is found.

.. option:: GDAL_USE_LERC_INTERNAL=ON/OFF

    Control whether to use the LERC internal library. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_LERC=ON

LIBAEC
******

`libaec <https://gitlab.dkrz.de/k202009/libaec>`_ is a compression library which offers
the extended Golomb-Rice coding as defined in the CCSDS recommended standard 121.0-B-3.
It is used by the :ref:`raster.grib` driver.

.. option:: LIBAEC_INCLUDE_DIR

    Path to an include directory with the ``libaec.h`` header file.

.. option:: LIBAEC_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_LIBAEC=ON/OFF

    Control whether to use LIBAEC. Defaults to ON when LIBAEC is found.

LibKML
******

`LibKML <https://github.com/libkml/libkml>`_ is required for the :ref:`vector.libkml` driver.
It can be detected with pkg-config.

.. option:: LIBKML_INCLUDE_DIR

    Path to the base include directory.

.. option:: LIBKML_BASE_LIBRARY

    Path to a shared or static library file for ``kmlbase``

.. option:: LIBKML_DOM_LIBRARY

    Path to a shared or static library file for ``kmldom``

.. option:: LIBKML_ENGINE_LIBRARY

    Path to a shared or static library file for ``kmlengine``

.. option:: LIBKML_MINIZIP_LIBRARY

    Path to a shared or static library file for ``minizip``

.. option:: LIBKML_URIPARSER_LIBRARY

    Path to a shared or static library file for ``uriparser``

.. option:: GDAL_USE_LIBKML=ON/OFF

    Control whether to use LibKML. Defaults to ON when LibKML is found.

LibLZMA
*******

`LibLZMA <https://tukaani.org/xz/>`_ is a compression library which offers
the lossless LZMA2 compression algorithm.
It is used by the internal libtiff library or the :ref:`raster.zarr` driver.

.. option:: LIBLZMA_INCLUDE_DIR

    Path to an include directory with the ``lzma.h`` header file.

.. option:: LIBLZMA_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_LIBLZMA=ON/OFF

    Control whether to use LibLZMA. Defaults to ON when LibLZMA is found.


libOpenDRIVE
************

`libOpenDRIVE <https://github.com/pageldev/libOpenDRIVE>`_ in version >= 0.6.0 is required for the :ref:`vector.xodr` driver.

.. option:: OpenDrive_DIR

    Path to libOpenDRIVE CMake configuration directory ``<installDir>/cmake/``. The :file:`cmake/` path is usually automatically created when installing libOpenDRIVE and contains the necessary configuration files for inclusion into other project builds.

.. option:: GDAL_USE_OPENDRIVE=ON/OFF

    Control whether to use libOpenDRIVE. Defaults to ON when libOpenDRIVE is found.


LibQB3
******

The `QB3 <https://github.com/lucianpls/QB3>`_ compression, used
by the :ref:`raster.marfa` driver.

.. option:: GDAL_USE_LIBQB3=ON/OFF

    Control whether to use LibQB3. Defaults to ON when LibQB3 is found.


LibXml2
*******

The `LibXml2 <http://xmlsoft.org/>`_ processing library is used to do validation of XML files against
a XML Schema (.xsd) in a few drivers (PDF, GMLAS, GML OGR VRT) and for advanced
capabilities in GMLJP2v2 generation.

.. option:: LIBXML2_INCLUDE_DIR

    Path to the base include directory.

.. option:: LIBXML2_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_LIBXML2=ON/OFF

    Control whether to use LibXml2. Defaults to ON when LibXml2 is found.


LZ4
***

`LZ4 <https://github.com/lz4/lz4>`_ is a compression library which offers
the lossless LZ4 compression algorithm.
It is used by the :ref:`raster.zarr` driver.

.. option:: LZ4_INCLUDE_DIR

    Path to an include directory with the ``lz4.h`` header file.

.. option:: LZ4_LIBRARY_RELEASE

    Path to a shared or static library file.  A similar variable
    ``LZ4_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_LZ4=ON/OFF

    Control whether to use LZ4. Defaults to ON when LZ4 is found.


MONGOCXX
********

The `MongoCXX <https://github.com/mongodb/mongo-cxx-driver>`_ and BsonCXX libraries
are needed to build the :ref:`vector.mongodbv3` driver.
They can be detected with pkg-config.

.. option:: MONGOCXX_INCLUDE_DIR

    Path to an include directory with the ``mongocxx/client.hpp`` header file.

.. option:: BSONCXX_INCLUDE_DIR

    Path to an include directory with the ``bsoncxx/config/version.hpp`` header file.

.. option:: MONGOCXX_LIBRARY

    Path to a library file ``mongocxx``

.. option:: BSONCXX_LIBRARY

    Path to a library file ``bsoncxx``

.. option:: GDAL_USE_MONGOCXX=ON/OFF

    Control whether to use MONGOCXX. Defaults to ON when MONGOCXX is found.


MRSID
*****

The MRSID Raster DSDK (closed source/proprietary) is required for the
:ref:`raster.mrsid` driver.

MRSID_ROOT or CMAKE_PREFIX_PATH should point to the directory of the SDK ending with
Raster_DSDK. Note that on Linux, its lib subdirectory should be in the
LD_LIBRARY_PATH so that the linking of applications succeeds and libtbb.so can
be found.

.. option:: MRSID_INCLUDE_DIR

    Path to the include directory with the ``lt_base.h`` header file.

.. option:: MRSID_LIBRARY

    Path to library file libltidsdk

.. option:: GDAL_ENABLE_DRIVER_JP2MRSID

    Whether to enable JPEG2000 support through the MrSID SDK. The default value
    of this option is OFF.

.. option:: GDAL_USE_MRSID=ON/OFF

    Control whether to use MRSID. Defaults to ON when MRSID is found.


MSSQL_NCLI
**********

The Microsoft SQL Native Client Library (closed source/proprietary) is required
to enable bulk copy in the :ref:`vector.mssqlspatial` driver.
If both MSSQL_NCLI and MSSQL_ODBC are found and enabled, MSSQL_ODBC will be used.
The library is normally found if installed in standard location, and at version 11.

.. option:: MSSQL_NCLI_VERSION

  Major version of the Native Client, typically 11

.. option:: MSSQL_NCLI_INCLUDE_DIR

  Path to include directory with ``sqlncli.h`` header.

.. option:: MSSQL_NCLI_LIBRARY

  Path to library to be linked.

.. option:: GDAL_USE_MSSQL_NCLI=ON/OFF

    Control whether to use MSSQL_NCLI. Defaults to ON when MSSQL_NCLI is found.


MSSQL_ODBC
**********

The Microsoft SQL Native ODBC driver Library (closed source/proprietary) is required
to enable bulk copy in the :ref:`vector.mssqlspatial` driver.
If both MSSQL_NCLI and MSSQL_ODBC are found and enabled, MSSQL_ODBC will be used.
The library is normally found if installed in standard location, and at version 17.

.. option:: MSSQL_ODBC_VERSION

  Major version of the Native Client, typically 17

.. option:: MSSQL_ODBC_INCLUDE_DIR

  Path to include directory with ``msodbcsql.h`` header.

.. option:: MSSQL_ODBC_LIBRARY

  Path to library to be linked.

.. option:: GDAL_USE_MSSQL_ODBC=ON/OFF

    Control whether to use MSSQL_ODBC. Defaults to ON when MSSQL_ODBC is found.


MYSQL
*****

The MySQL or MariaDB client library is required to enable the :ref:`vector.mysql`
driver.

.. option:: MYSQL_INCLUDE_DIR

  Path to include directory with ``mysql.h`` header file.

.. option:: MYSQL_LIBRARY

  Path to library to be linked.

.. option:: GDAL_USE_MYSQL=ON/OFF

    Control whether to use MYSQL. Defaults to ON when MYSQL is found.


NetCDF
******

The `netCDF <https://github.com/Unidata/netcdf-c>`_ is required to enable the
:ref:`raster.netcdf` driver.
The ``nc-config`` program can be used to detect it.

.. option:: NETCDF_INCLUDE_DIR

    Path to an include directory with the ``netcdf.h`` header file.

.. option:: NETCDF_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_NETCDF=ON/OFF

    Control whether to use netCDF. Defaults to ON when netCDF is found.


ODBC
****

ODBC is required for various drivers: :ref:`vector.odbc`, :ref:`vector.pgeo`,
:ref:`vector.hana` and :ref:`vector.mssqlspatial`.
It is normally automatically found in system directories on Unix and Windows.

.. option:: ODBC_INCLUDE_DIR

  Path to ODBC include directory with ``sql.h`` header.

.. option:: ODBC_LIBRARY

  Path to ODBC library to be linked.

.. option:: GDAL_USE_ODBC=ON/OFF

    Control whether to use ODBC. Defaults to ON when ODBC is found.


ODBC-CPP
********

The `odbc-cpp-wrapper library <https://github.com/SAP/odbc-cpp-wrapper>`_ is required for
the :ref:`vector.hana` driver.

.. option:: ODBCCPP_INCLUDE_DIR

    Path to an include directory with the ``odbc/Environment.h`` header file.

.. option:: ODBCCPP_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_ODBCCPP=ON/OFF

    Control whether to use ODBC-CPP. Defaults to ON when ODBC-CPP is found.


OGDI
****

The `OGDI <https://github.com/libogdi/ogdi/>`_ library is required for the :ref:`vector.ogdi`
driver. It can be detected with pkg-config.

.. option:: OGDI_INCLUDE_DIR

    Path to an include directory with the ``ecs.h`` header file.

.. option:: OGDI_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_OGDI=ON/OFF

    Control whether to use OGDI. Defaults to ON when OGDI is found.


OpenCAD
*******

`libopencad <https://github.com/nextgis-borsch/lib_opencad>`_ is required for the :ref:`vector.cad`
driver. If not found, an internal copy can be used.

.. option:: OPENCAD_INCLUDE_DIR

    Path to an include directory with the ``opencad.h`` header file.

.. option:: OPENCAD_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_OPENCAD=ON/OFF

    Control whether to use external libopencad. Defaults to ON when external libopencad is found.

.. option:: GDAL_USE_OPENCAD_INTERNAL=ON/OFF

    Control whether to use internal libopencad copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_OPENCAD=ON



OpenCL
******

The OpenCL library may be used to accelerate warping computations, typically
with a GPU.

.. note:: (GDAL 3.5 and 3.6) It is disabled by default even when detected, since the current OpenCL
          warping implementation lags behind the generic implementation.
          Starting with GDAL 3.7, build support is enabled by default when OpenCL is detected,
          but it is disabled by default at runtime. The warping option USE_OPENCL
          or the configuration option GDAL_USE_OPENCL must be set to YES to enable it.

.. option:: OpenCL_INCLUDE_DIR

    Path to an include directory with the ``CL/cl.h`` header file.

.. option:: OpenCL_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_OPENCL=ON/OFF

    Control whether to use OPENCL. Defaults to *OFF* when OPENCL is found.


OpenEXR
*******

`OpenEXR <https://github.com/AcademySoftwareFoundation/openexr>`_ is required for the :ref:`raster.exr` driver

Specify ``OpenEXR_ROOT`` variable pointing to the parent directory of
/lib and /include subdirectories, i.e. /DEV/lib/openexr-3.0.
For OpenEXR >= 3 additionally specify ``Imath_ROOT`` as this is a
separate library now, i.e. /DEV/lib/imath-3.1.3

or

Specify root directory adding to the ``CMAKE_PREFIX_PATH`` variable to find OpenEXR's pkgconfig.
For example -DCMAKE_PREFIX_PATH=/DEV/lib/openexr-3.0;/DEV/lib/imath-3.1.3

or

Get real specific and set
``OpenEXR_INCLUDE_DIR``, ``Imath_INCLUDE_DIR``,
``OpenEXR_LIBRARY``, ``OpenEXR_UTIL_LIBRARY``,
``OpenEXR_HALF_LIBRARY``, ``OpenEXR_IEX_LIBRARY``
explicitly

.. option:: GDAL_USE_OPENEXR=ON/OFF

    Control whether to use OpenEXR. Defaults to ON when OpenEXR is found.


OpenJPEG
********

The `OpenJPEG <https://github.com/uclouvain/openjpeg>`_ library is an open-source
JPEG-2000 codec written in C language. It is required for the
:ref:`raster.jp2openjpeg` driver, or other drivers that use JPEG-2000 functionality.

.. option:: OPENJPEG_INCLUDE_DIR

    Path to an include directory with the ``openjpeg.h`` header file.

.. option:: OPENJPEG_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_OPENJPEG=ON/OFF

    Control whether to use OpenJPEG. Defaults to ON when OpenJPEG is found.

.. option:: GDAL_FIND_PACKAGE_OpenJPEG_MODE=MODULE/CONFIG/empty string

    .. versionadded:: 3.9

    Control the mode used for find_package(OpenJPEG). Defaults to MODULE
    for compatibility with OpenJPEG < 2.5.1. If set to CONFIG, only Config mode
    search is attempted. If set to empty string, default CMake logic
    (https://cmake.org/cmake/help/latest/command/find_package.html) applies.


OpenSSL
*******

The Crypto component of the `OpenSSL <https://github.com/openssl/openssl>`_ library
can be used for the RSA SHA256 signing functionality used by some authentication
methods of Google Cloud. It might be required to use the :ref:`raster.eedai`
images or use the :ref:`/vsigs/ <vsigs>` virtual file system.

See https://cmake.org/cmake/help/latest/module/FindOpenSSL.html for details on
how to configure the library. For static linking, the following options may
be needed: -DOPENSSL_USE_STATIC_LIBS=TRUE -DOPENSSL_MSVC_STATIC_RT=TRUE

.. option:: GDAL_USE_OPENSSL=ON/OFF

    Control whether to use OpenSSL. Defaults to ON when OpenSSL is found.


Oracle
******

The Oracle Instant Client SDK (closed source/proprietary) is required for the
:ref:`vector.oci` and the :ref:`raster.georaster` drivers

.. option:: Oracle_ROOT

    Path to the root directory of the Oracle Instant Client SDK.

.. option:: GDAL_USE_ORACLE=ON/OFF

    Control whether to use Oracle. Defaults to ON when Oracle is found.


Parquet
*******

The Parquet component of the `Apache Arrow C++ <https://github.com/apache/arrow/tree/master/cpp>`
library is required for the :ref:`vector.parquet` driver.
Specify install prefix in the ``CMAKE_PREFIX_PATH`` variable.

.. option:: GDAL_USE_PARQUET=ON/OFF

    Control whether to use Parquet. Defaults to ON when Parquet is found.

.. option:: ARROW_USE_STATIC_LIBRARIES=ON/OFF

    Control whether to use statically built Arrow libraries. Defaults to OFF when Parquet is found.


PCRE2
*****

`PCRE2 <https://github.com/PhilipHazel/pcre2>`_ implements Perl-compatible
Regular Expressions support. It is used for the REGEXP operator in drivers using SQLite3.

.. option:: PCRE2_INCLUDE_DIR

    Path to an include directory with the ``pcre2.h`` header file.

.. option:: PCRE2_LIBRARY

    Path to a shared or static library file with "pcre2-8" in its name.

.. option:: GDAL_USE_PCRE2=ON/OFF

    Control whether to use PCRE2. Defaults to ON when PCRE2 is found.


PDFIUM
******

The `PDFium <https://github.com/rouault/pdfium_build_gdal_3_5>`_ library is one
of the possible backends for the :ref:`raster.pdf` driver.

.. option:: PDFIUM_INCLUDE_DIR

    Path to an include directory with the ``public/fpdfview.h`` header file.

.. option:: PDFIUM_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_PDFIUM=ON/OFF

    Control whether to use PDFIUM. Defaults to ON when PDFIUM is found.


PNG
***

`libpng <https://github.com/glennrp/libpng>`_ is required for the :ref:`raster.png`
driver, and may be used by a few other drivers (:ref:`raster.grib`, :ref:`raster.gpkg`, etc.)
If not found, an internal copy of libpng can be used.
See https://cmake.org/cmake/help/latest/module/FindPNG.html for more details
on how the library is detected.

.. option:: PNG_PNG_INCLUDE_DIR

    Path to an include directory with the ``png.h`` header file.

.. option:: PNG_LIBRARY_RELEASE

    Path to a shared or static library file. A similar variable
    ``PNG_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_PNG=ON/OFF

    Control whether to use external libpng. Defaults to ON when external libpng is found.

.. option:: GDAL_USE_PNG_INTERNAL=ON/OFF

    Control whether to use internal libpng copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_PNG=ON


Poppler
*******

The `Poppler <https://poppler.freedesktop.org/>`_ library is one
of the possible backends for the :ref:`raster.pdf` driver.

Note that GDAL requires Poppler private headers, that are only installed
if configuring Poppler with -DENABLE_UNSTABLE_API_ABI_HEADERS.

.. option:: Poppler_INCLUDE_DIR

    Path to an include directory with the ``poppler-config.h`` header file.

.. option:: Poppler_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_POPPLER=ON/OFF

    Control whether to use Poppler. Defaults to ON when Poppler is found.


PostgreSQL
**********

The `PostgreSQL client library <https://www.postgresql.org/>`_ is required for
the :ref:`vector.pg` and :ref:`raster.postgisraster` drivers.

.. option:: PostgreSQL_INCLUDE_DIR

    Path to an include directory with the ``libpq-fe.h`` header file.

.. option:: PostgreSQL_LIBRARY_RELEASE

    Path to a shared or static library file ``pq`` / ``libpq``. A similar variable
    ``PostgreSQL_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_POSTGRESQL=ON/OFF

    Control whether to use PostgreSQL. Defaults to ON when PostgreSQL is found.


PROJ
****

`PROJ <https://github.com/OSGeo/PROJ/>`_ >= 6.3 is a *required* dependency for GDAL.

.. option:: PROJ_INCLUDE_DIR

    Path to an include directory with the ``proj.h`` header file.

.. option:: PROJ_LIBRARY_RELEASE

    Path to a shared or static library file, such as
    ``libproj.so``, ``proj.lib``, or other name. A similar variable
    ``PROJ_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_FIND_PACKAGE_PROJ_MODE=CUSTOM/MODULE/CONFIG/empty string

    .. versionadded:: 3.9

    Control the mode used for find_package(PROJ).
    Alters how the default CMake search logic
    (https://cmake.org/cmake/help/latest/command/find_package.html) applies.
    Defaults to CUSTOM, where the CONFIG mode is applied for PROJ >= 8, and
    fallbacks to default MODULE mode otherwise.
    Other values are passed directly to find_package()

QHULL
*****

The `QHULL <https://github.com/qhull/qhull>`_ library is used for the linear
interpolation of gdal_grid. If not found, an internal copy can be used.

.. option:: QHULL_PACKAGE_NAME

   Name of the pkg-config package, typically ``qhull_r`` or ``qhullstatic_r``. Defaults to ``qhull_r``

.. option:: QHULL_INCLUDE_DIR

    Path to an include directory with the ``libqhull_r/libqhull_r.h`` header file.

.. option:: QHULL_LIBRARY

    Path to a shared or static library file to the reentrant library.

.. option:: GDAL_USE_QHULL=ON/OFF

    Control whether to use QHULL. Defaults to ON when QHULL is found.

.. option:: GDAL_USE_QHULL_INTERNAL=ON/OFF

    Control whether to use internal QHULL copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_QHULL=ON


RASTERLITE2
***********

The `RasterLite2 <https://www.gaia-gis.it/fossil/librasterlite2/index>`_ (>= 1.1.0)
library used by the :ref:`raster.rasterlite2` driver.
It can be detected with pkg-config.

.. option:: RASTERLITE2_INCLUDE_DIR

    Path to an include directory with the ``rasterlite2/rasterlite2.h`` header file.

.. option:: RASTERLITE2_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_RASTERLITE2=ON/OFF

    Control whether to use RasterLite2. Defaults to ON when RasterLite2 is found.


rdb
***

The `RDB <https://repository.riegl.com/software/libraries/rdblib>`
(closed source/proprietary) library is required for the :ref:`raster.rdb` driver.
Specify install prefix in the ``CMAKE_PREFIX_PATH`` variable.

.. option:: GDAL_USE_RDB=ON/OFF

    Control whether to use rdb. Defaults to ON when rdb is found.


SPATIALITE
**********

The `Spatialite <https://www.gaia-gis.it/fossil/libspatialite/index>`_ library
used by the :ref:`vector.sqlite` and :ref:`vector.gpkg` drivers, and the :ref:`sql_sqlite_dialect`.
It can be detected with pkg-config.

.. option:: SPATIALITE_INCLUDE_DIR

    Path to an include directory with the ``spatialite.h`` header file.

.. option:: SPATIALITE_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_SPATIALITE=ON/OFF

    Control whether to use Spatialite. Defaults to ON when Spatialite is found.


SQLite3
*******

The `SQLite3 <https://sqlite.org/index.html>`_ library  is required for the
:ref:`vector.sqlite` and :ref:`vector.gpkg` drivers (and also used by other drivers),
and the :ref:`sql_sqlite_dialect`.

.. option:: SQLite3_INCLUDE_DIR

    Path to an include directory with the ``sqlite3.h`` header file.

.. option:: SQLite3_LIBRARY

    Path to a shared or static library file, such as
    ``libsqlite3.so``, ``sqlite3.lib`` or other name.

.. option:: GDAL_USE_SQLITE3=ON/OFF

    Control whether to use SQLite3. Defaults to ON when SQLite3 is found.


SFCGAL
******

`SFCGAL <https://github.com/Oslandia/SFCGAL>`_ is a geometry library which
supports ISO 19107:2013 and OGC Simple Features Access 1.2 for 3D operations
(PolyhedralSurface, TINs, ...)

.. option:: SFCGAL_INCLUDE_DIR

    Path to the base include directory.

.. option:: SFCGAL_LIBRARY_RELEASE

    Path to a shared or static library file. A similar variable
    ``SFCGAL_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_SFCGAL=ON/OFF

    Control whether to use SFCGAL. Defaults to ON when SFCGAL is found.


SWIG
****

`SWIG <http://swig.org/>`_ is a software development tool that connects
programs written in C and C++ with a variety of high-level programming languages.
It is used for the Python, Java and CSharp bindings.

.. option:: SWIG_EXECUTABLE

    Path to the SWIG executable.

    Note that setting it explicitly might be needed, and that putting the
    directory of the installed binary into the PATH might not be sufficient.
    The reason is that when building from source, a "swig" binary will be
    generated, but FindSWIG will prefer a "swig-4.0" binary if found elsewhere
    in the PATH.


TEIGHA
******

The TEIGHA / Open Design Alliance libraries (closed source/proprietary) are
required for the :ref:`vector.dwg` and :ref:`vector.dgnv8` drivers.
Note that on Linux, with a SDK consisting of shared libraries,
the bin/{platform_name} subdirectory of the SDK should be in the LD_LIBRARY_PATH
so that the linking of applications succeeds.
The TEIGHA_ROOT variable must be set.

.. option:: TEIGHA_ROOT

    Path to the base directory where the Kernel and Drawings package must be
    extracted.

.. option:: TEIGHA_ACTIVATION_FILE_DIRECTORY

    Path to a directory where a ``OdActivationInfo`` file is located. If the
    file is somewhere under TEIGHA_ROOT, it will be automatically discovered.
    Otherwise this variable must be set for recent SDK versions (at least with
    2021 and later).

.. option:: GDAL_USE_TEIGHA=ON/OFF

    Control whether to use TEIGHA. Defaults to ON when TEIGHA is found.


TIFF
****

`libtiff <https://gitlab.com/libtiff/libtiff/>`_ is required for the
:ref:`raster.gtiff` drivers, and a few other drivers.
If not found, an internal copy of libtiff can be used.

.. option:: TIFF_INCLUDE_DIR

    Path to an include directory with the ``tiff.h`` header file.

.. option:: TIFF_LIBRARY_RELEASE

    Path to a shared or static library file, such as
    ``libtiff.so``, ``tiff.lib``, or other name. A similar variable
    ``TIFF_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_TIFF=ON/OFF

    Control whether to use external libtiff. Defaults to ON when external libtiff is found.

.. option:: GDAL_USE_TIFF_INTERNAL=ON/OFF

    Control whether to use internal libtiff copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_TIFF=ON


TileDB
******

The `TileDB <https://github.com/TileDB-Inc/TileDB>` library is required for the :ref:`raster.tiledb` driver.
Specify install prefix in the ``CMAKE_PREFIX_PATH`` variable.

TileDB >= 2.15 is required since GDAL 3.9

.. option:: GDAL_USE_TILEDB=ON/OFF

    Control whether to use TileDB. Defaults to ON when TileDB is found.


WebP
****

`WebP <https://github.com/webmproject/libwebp>`_ is a image compression library.
It is required for the :ref:`raster.webp` driver, and may be used by the
:ref:`raster.gpkg` and the internal libtiff library.

.. option:: WEBP_INCLUDE_DIR

    Path to an include directory with the ``webp/encode.h`` header file.

.. option:: WEBP_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_WEBP=ON/OFF

    Control whether to use WebP. Defaults to ON when WebP is found.


XercesC
*******

`Xerces-C <https://github.com/apache/xerces-c>`_ is a stream-oriented XML parser
library which is required to enable XML parsing capabilities in the :ref:`vector.nas`,
:ref:`vector.ili` and :ref:`vector.gmlas` drivers.
It can also be used as an alternative to Expat for the GML driver.

.. option:: XercesC_INCLUDE_DIR

    Path to the base include directory.

.. option:: XercesC_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_XERCESC=ON/OFF

    Control whether to use XercesC. Defaults to ON when XercesC is found.


ZLIB
****

`ZLib <https://github.com/madler/zlib>`_ is a compression library which offers
the lossless Deflate/Zip compression algorithm.

.. option:: ZLIB_INCLUDE_DIR

    Path to an include directory with the ``zlib.h`` header file.

.. option:: ZLIB_LIBRARY_RELEASE

    Path to a shared or static library file. A similar variable
    ``ZLIB_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: ZLIB_IS_STATIC

    Link to static external ZLIB directory.
    Only used if GDAL_USE_ZLIB_INTERNAL=OFF and MSVC.

.. option:: GDAL_USE_ZLIB=ON/OFF

    Control whether to use ZLIB. Defaults to ON when ZLIB is found.

.. option:: GDAL_USE_ZLIB_INTERNAL=ON/OFF

    Control whether to use internal zlib copy. Defaults depends on GDAL_USE_INTERNAL_LIBS. When set
    to ON, has precedence over GDAL_USE_ZLIB=ON


ZSTD
****

`ZSTD <https://github.com/facebook/zstd>`_ is a compression library which offers
the lossless ZStd compression algorithm (faster than Deflate/ZIP, but incompatible
with it). It is used by the internal libtiff library or the :ref:`raster.zarr` driver.

.. option:: ZSTD_INCLUDE_DIR

    Path to an include directory with the ``zstd.h`` header file.

.. option:: ZSTD_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_ZSTD=ON/OFF

    Control whether to use ZSTD. Defaults to ON when ZSTD is found.


.. _selection-of-drivers:

Selection of drivers
++++++++++++++++++++

By default, all drivers that have their build requirements satisfied will be
built-in in the GDAL core library.

The following options are available to select a subset of drivers:

.. option:: GDAL_ENABLE_DRIVER_<driver_name>:BOOL=ON/OFF

.. option:: OGR_ENABLE_DRIVER_<driver_name>:BOOL=ON/OFF

    Independently of options that control global behavior, drivers can be individually
    enabled or disabled with those options.

    .. note::

        <driver_name> above and below is *generally*, but not systematically the short driver name.

        Some drivers may also be grouped together for build purposes.

        - A number of "raw" raster drivers (ACE2, BT, BYN, CPG, CTable2, DIPEx, DOQ1,
          DOQ2, EHDR, EIR, ENVI, FAST, GenBIN, GSC, GTX, MFF2, ISCE, KRO, MFF, LAN,
          LCP, LOSLAS, NDF, NTv2, PAUX, PNM, ROIPAC, RRASTER, SNODAS) are controlled
          by the GDAL_ENABLE_DRIVER_RAW option.

        - Planetary raster formats (PDS, PDS4, ISIS2, ISIS3, VICAR) are controlled by
          the GDAL_ENABLE_DRIVER_PDS option.

        - The AAIGRID, GRASSASCIIGRID and ISG raster drivers are controlled by the GDAL_ENABLE_DRIVER_AAIGRID option.

        - The ECW and JP2ECW raster drivers are controlled by the GDAL_ENABLE_DRIVER_ECW option.

        - The vector EEDA and raster EEDAI drivers are controlled by the GDAL_ENABLE_DRIVER_EEDA option.

        - The GSAG, GSBG and GS7BG raster drivers are controlled by the GDAL_ENABLE_DRIVER_GSG option.

        - The HDF5 and BAG raster drivers are controlled by the GDAL_ENABLE_DRIVER_HDF5 option.

        - The MrSID and JP2MrSID raster drivers are controlled by the GDAL_ENABLE_DRIVER_MRSID option.

        - The NITF, RPFTOC and ECRGTOC raster drivers are controlled by the GDAL_ENABLE_DRIVER_NITF option.

        - The NWT_GRD and NWT_GRC raster drivers are controlled by the GDAL_ENABLE_DRIVER_NORTHWOOD option.

        - The SRP and ADRG raster drivers are controlled by the GDAL_ENABLE_DRIVER_ADRG option.

        - The Interlis 1 and Interlis 2 vector drivers are controlled by the GDAL_ENABLE_DRIVER_ILI option.

        - The WFS and OAPIF vector drivers are controlled by the GDAL_ENABLE_DRIVER_WFS option.

        - The AVCBIN and AVCE00 vector drivers are controlled by the GDAL_ENABLE_DRIVER_AVC option.

        - The DWG and DGNv8 vector drivers are controlled by the GDAL_ENABLE_DRIVER_DWG option.

        There might be variations in naming, e.g. :

        - the "AIG" raster driver is controlled by GDAL_ENABLE_DRIVER_AIGRID.

        - the "ESAT" raster driver is controlled by GDAL_ENABLE_DRIVER_ENVISAT.

        - the "GeoRaster" raster driver is controlled by GDAL_ENABLE_DRIVER_GEOR.

        - the "RST" raster driver is controlled by GDAL_ENABLE_DRIVER_IDRISI.

        - the "ElasticSearch" vector driver is controlled by OGR_ENABLE_DRIVER_ELASTIC.

        - the "PostgreSQL" vector driver is controlled by OGR_ENABLE_DRIVER_PG.

        - the "UK .NTF" vector driver is controlled by OGR_ENABLE_DRIVER_NTF.

    .. note::

        Drivers that have both a raster and vector side (and are internally implemented by a
        single GDALDriver instance) are controlled by either a GDAL_ENABLE_DRIVER_<driver_name>
        option or a OGR_ENABLE_DRIVER_<driver_name> one, but not both:

        - The CAD drivers are controlled by the OGR_ENABLE_DRIVER_CAD option.
        - The netCDF drivers are controlled by the GDAL_ENABLE_DRIVER_NETCDF option.
        - The PDF drivers are controlled by the GDAL_ENABLE_DRIVER_PDF option.
        - The GPKG drivers are controlled by the OGR_ENABLE_DRIVER_GPKG option.
        - The NGW drivers are controlled by the OGR_ENABLE_DRIVER_NGW option.
        - The SQLite drivers are controlled by the OGR_ENABLE_DRIVER_SQLITE option.

    .. note::

        The GDAL_ENABLE_DRIVER_<driver_name> and OGR_ENABLE_DRIVER_<driver_name> options are
        only created when their required dependencies are found.


.. option:: GDAL_BUILD_OPTIONAL_DRIVERS:BOOL=ON/OFF

.. option:: OGR_BUILD_OPTIONAL_DRIVERS:BOOL=ON/OFF

    Globally enable/disable all optional GDAL/raster, resp. all optional OGR/vector drivers.
    More exactly, setting those variables to ON affect the default value of the
    ``GDAL_ENABLE_DRIVER_<driver_name>`` or ``OGR_ENABLE_DRIVER_<driver_name>`` variables
    (when they are not yet set).

    This can be combined with individual activation of a subset of drivers by using
    the ``GDAL_ENABLE_DRIVER_<driver_name>:BOOL=ON`` or ``OGR_ENABLE_DRIVER_<driver_name>:BOOL=ON``
    variables. Note that changing the value of GDAL_BUILD_OPTIONAL_DRIVERS/
    OGR_BUILD_OPTIONAL_DRIVERS after a first run of CMake does not change the
    activation of individual drivers. It might be needed to pass
    ``-UGDAL_ENABLE_DRIVER_* -UOGR_ENABLE_DRIVER_*`` to reset their state.

    .. note::

        The following GDAL drivers cannot be disabled: VRT, DERIVED, GTiff, COG, HFA, MEM.
        The following OGR drivers cannot be disabled: "ESRI Shapefile", "MapInfo File", OGR_VRT, Memory, KML, GeoJSON, GeoJSONSeq, ESRIJSON, TopoJSON.

    .. note::

        Disabling all OGR/vector drivers with -DOGR_BUILD_OPTIONAL_DRIVERS=OFF may affect
        the ability to enable some GDAL/raster drivers that require some vector
        drivers to be enabled (and reciprocally with some GDAL/raster drivers depending
        on vector drivers).
        When such dependencies are not met, a CMake error will be emitted with a hint
        for the way to resolve the issue.
        It is also possible to anticipate such errors by looking at files
        :source_file:`frmts/CMakeLists.txt` for dependencies of raster drivers
        and :source_file:`ogr/ogrsf_frmts/CMakeLists.txt` for dependencies of vector drivers.


Example of minimal build with the JP2OpenJPEG and SVG drivers enabled::

    cmake .. -UGDAL_ENABLE_DRIVER_* -UOGR_ENABLE_DRIVER_* \
             -DGDAL_BUILD_OPTIONAL_DRIVERS:BOOL=OFF -DOGR_BUILD_OPTIONAL_DRIVERS:BOOL=OFF \
             -DGDAL_ENABLE_DRIVER_JP2OPENJPEG:BOOL=ON \
             -DOGR_ENABLE_DRIVER_SVG:BOOL=ON

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

.. option:: GDAL_ENABLE_DRIVER_<driver_name>_PLUGIN:BOOL=ON/OFF

.. option:: OGR_ENABLE_DRIVER_<driver_name>_PLUGIN:BOOL=ON/OFF

    Independently of options that control global behavior, drivers can be individually
    enabled or disabled with those options.

    Note that for the driver to be built, the corresponding base
    ``GDAL_ENABLE_DRIVER_{driver_name}:BOOL=ON`` or ``OGR_ENABLE_DRIVER_{driver_name}:BOOL=ON`` option must
    be set.

.. option:: GDAL_ENABLE_PLUGINS:BOOL=ON/OFF

    Globally enable/disable building all (plugin capable), GDAL and OGR, drivers as plugins.
    More exactly, setting that variable to ON affects the default value of the
    ``GDAL_ENABLE_DRIVER_<driver_name>_PLUGIN`` or ``OGR_ENABLE_DRIVER_<driver_name>_PLUGIN``
    variables (when they are not yet set).

    This can be combined with individual activation/deactivation of the plugin status with the
    ``GDAL_ENABLE_DRIVER_{driver_name}_PLUGIN:BOOL`` or ``OGR_ENABLE_DRIVER_{driver_name}_PLUGIN:BOOL`` variables.
    Note that changing the value of GDAL_ENABLE_PLUGINS after a first
    run of CMake does not change the activation of the plugin status of individual drivers.
    It might be needed to pass ``-UGDAL_ENABLE_DRIVER_* -UOGR_ENABLE_DRIVER_*`` to reset their state.

Example of build with all potential drivers as plugins, except the JP2OpenJPEG one::

    cmake .. -UGDAL_ENABLE_DRIVER_* -UOGR_ENABLE_DRIVER_* \
             -DGDAL_ENABLE_PLUGINS:BOOL=ON \
             -DGDAL_ENABLE_DRIVER_JP2OPENJPEG_PLUGIN:BOOL=OFF

There is a subtelty regarding ``GDAL_ENABLE_PLUGINS:BOOL=ON``. It only controls
the plugin status of plugin-capable drivers that have external dependencies,
that are not part of GDAL core dependencies (e.g. are netCDF, HDF4, Oracle, PDF, etc.).

.. option:: GDAL_ENABLE_PLUGINS_NO_DEPS:BOOL=ON/OFF

    Globally enable/disable building all (plugin capable), GDAL and OGR, drivers as plugins,
    for drivers that have no external dependencies (e.g. BMP, FlatGeobuf), or that have
    dependencies that are part of GDAL core dependencies (e.g GPX).
    Building such drivers as plugins is generally not necessary, hence
    the use of a different option from GDAL_ENABLE_PLUGINS.

In some circumstances, it might be desirable to prevent loading of GDAL plugins.
This can be done with:

.. option:: GDAL_AUTOLOAD_PLUGINS:BOOL=ON/OFF

    Set to OFF to disable loading of GDAL plugins. Default is ON.


Deferred loaded plugins
+++++++++++++++++++++++

Starting with GDAL 3.9, a number of in-tree drivers, that can be built as
plugins, are loaded in a deferred way. This involves that some part of their
code, which does not depend on external libraries, is included in core libgdal,
whereas most of the driver code is in a separated dynamically loaded library.
For builds where libgdal and its plugins are built in a single operation, this
is fully transparent to the user.

When a plugin driver is known of core libgdal, but not available as a plugin at
runtime, GDAL will inform the user that the plugin is not available, but could
be installed. It is possible to give more hints on how to install a plugin
by setting the following option:

.. option:: GDAL_DRIVER_<driver_name>_PLUGIN_INSTALLATION_MESSAGE:STRING

.. option:: OGR_DRIVER_<driver_name>_PLUGIN_INSTALLATION_MESSAGE:STRING

    Custom message to give a hint to the user how to install a missing plugin


For example, if doing a build with::

    cmake .. -DOGR_DRIVER_PARQUET_PLUGIN_INSTALLATION_MESSAGE="You may install it with with 'conda install -c conda-forge libgdal-arrow-parquet'"

and opening a Parquet file while the plugin is not installed will display the
following error::

    $ ogrinfo poly.parquet
    ERROR 4: `poly.parquet' not recognized as a supported file format. It could have been recognized by driver Parquet, but plugin ogr_Parquet.so is not available in your installation. You may install it with with 'conda install -c conda-forge libgdal-arrow-parquet'


For more specific builds where libgdal would be first built, and then plugin
drivers built in later incremental builds, this approach would not work, given
that the core libgdal built initially would lack code needed to declare the
plugin(s).

In that situation, the user building GDAL will need to explicitly declare at
initial libgdal build time that one or several plugin(s) will be later built.
Note that it is safe to distribute such a libgdal library, even if the plugins
are not always available at runtime.

This can be done with the following option:

.. option:: GDAL_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN:BOOL=ON

.. option:: OGR_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN:BOOL=ON

    Declares that a driver will be later built as a plugin.

Setting this option to drivers not ready for it will lead to an explicit
CMake error.


For some drivers (ECW, HEIF, JP2KAK, JPEG, JPEGXL, KEA, LERC, MrSID,
MSSQLSpatial, netCDF, OpenJPEG, PDF, TileDB, WEBP), the metadata and/or dataset
identification code embedded on libgdal, will depend on optional capabilities
of the dependent library (e.g. libnetcdf for netCDF)
In that situation, it is desirable that the dependent library is available at
CMake configuration time for the core libgdal built, but disabled with
GDAL_USE_<driver_name>=OFF. It must of course be re-enabled later when the plugin is
built.

For example for netCDF::

    cmake .. -DGDAL_REGISTER_DRIVER_NETCDF_FOR_LATER_PLUGIN=ON -DGDAL_USE_NETCDF=OFF
    cmake --build .

    cmake .. -DGDAL_USE_NETCDF=ON -DGDAL_ENABLE_DRIVER_NETCDF=ON -DGDAL_ENABLE_DRIVER_NETCDF_PLUGIN=ON
    cmake --build . --target gdal_netCDF


For other drivers, GDAL_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN /
OGR_REGISTER_DRIVER_<driver_name>_FOR_LATER_PLUGIN can be declared at
libgdal build time without requiring the dependent libraries needed to build
the plugin later to be available.

Out-of-tree deferred loaded plugins
+++++++++++++++++++++++++++++++++++

Out-of-tree drivers can also benefit from the deferred loading capability, provided
libgdal is built with CMake variable(s) pointing to external code containing the
code for registering a proxy driver.

This can be done with the following option:

.. option:: ADD_EXTERNAL_DEFERRED_PLUGIN_<driver_name>:FILEPATH=/path/to/some/file.cpp

The pointed file must declare a ``void DeclareDeferred<driver_name>(void)``
method with C linkage that takes care of creating a GDALPluginDriverProxy
instance and calling :cpp:func:`GDALDriverManager::DeclareDeferredPluginDriver` on it.

.. _building-python-bindings:

Python bindings options
+++++++++++++++++++++++

.. option:: BUILD_PYTHON_BINDINGS:BOOL=ON/OFF

    Whether Python bindings should be built. It is ON by default, but only
    effective if a Python installation is found.

A nominal Python installation should comprise the Python runtime (>= 3.6) and
the setuptools module.
numpy and its header and development library are also strongly recommended.

The Python installation is normally found if found in the path or registered
through other standard installation mechanisms of the Python installers.
It is also possible to specify it using several variables, as detailed in
https://cmake.org/cmake/help/git-stage/module/FindPython.html

GDAL also provides the following option:

.. option:: Python_LOOKUP_VERSION:STRING=major.minor.patch

    When it is specified, Python_FIND_STRATEGY=VERSION is assumed. Note that
    the patch number must be provided, as the EXACT strategy is used

Other useful options:

.. option:: Python_FIND_VIRTUALENV

    Specify 'ONLY' to use virtualenv activated.

.. option:: Python_ROOT

    Specify Python installation prefix.

Examples::

    cmake -DPython_LOOKUP_VERSION=3.6.0 ..
    cmake -DPython_FIND_VIRTUALENV=ONLY ..
    cmake -DPython_ROOT=C:\Python36 ..


The following options are advanced ones and only taken into account during
the ``install`` CMake target.

.. option:: GDAL_PYTHON_INSTALL_PREFIX

    This option can be specified to a directory name, to override the
    ``CMAKE_INSTALL_PREFIX`` option.
    It is used to set the value of the ``--prefix`` option of ``python3 setup.py install``.

.. option:: GDAL_PYTHON_INSTALL_LAYOUT

    This option can be specified to set the value of the ``--install-layout``
    option of ``python3 setup.py install``. The install layout is by default set to
    ``deb`` when it is detected that the Python installation looks for
    the ``site-packages`` subdirectory. Otherwise it is unspecified.

.. option:: GDAL_PYTHON_INSTALL_LIB

    This option can be specified to set the value of the ``--install-lib``
    option of ``python3 setup.py install``. It is only taken into account on
    MacOS systems, when the Python installation is a framework.

.. note::

    The Python bindings are made of several modules (osgeo.gdal, osgeo.ogr, etc.)
    which link each against libgdal. Consequently, a static build of libgdal is
    not compatible with the bindings.

.. _building_from_source_java:

Java bindings options
+++++++++++++++++++++

.. option:: BUILD_JAVA_BINDINGS:BOOL=ON/OFF

    Whether Java bindings should be built. It is ON by default, but only
    effective if Java runtime and development packages are found.
    The relevant options that can be set are described in
    https://cmake.org/cmake/help/latest/module/FindJava.html and
    https://cmake.org/cmake/help/latest/module/FindJNI.html.
    The ``ant`` binary must also be available in the PATH.

.. option:: GDAL_JAVA_INSTALL_DIR

    Subdirectory into which to install the :file:`gdal.jar` file.
    It defaults to "${CMAKE_INSTALL_DATADIR}/java"

    .. note::
        Prior to GDAL 3.8, the gdalalljni library was also installed in that
        directory. Starting with GDAL 3.8, this is controlled by the
        ``GDAL_JAVA_JNI_INSTALL_DIR`` variable.

.. option:: GDAL_JAVA_JNI_INSTALL_DIR

    .. versionadded:: 3.8

    Subdirectory into which to install the :file:`libgdalalljni.so` /
    :file:`libgdalalljni.dylib` / :file:`gdalalljni.dll` library.
    It defaults to "${CMAKE_INSTALL_LIBDIR}/jni".

    .. note::
        Prior to GDAL 3.8, the gdalalljni library was installed in the
        directory controlled by the ``GDAL_JAVA_INSTALL_DIR`` variable.


.. note::

    The Java bindings are made of several modules (org.osgeo.gdal, org.osgeo.ogr, etc.)
    which link each against libgdal. Consequently, a static build of libgdal is
    not compatible with the bindings.

Option only to be used by maintainers:

.. option:: GPG_KEY

    GPG key to sign build artifacts. Needed to generate bundle.jar.

.. option:: GPG_PASS

    GPG pass phrase to sign build artifacts.

C# bindings options
+++++++++++++++++++

For more details on how to build and use the C# bindings read the dedicated section :ref:`csharp_compile_cmake`.

.. option:: BUILD_CSHARP_BINDINGS:BOOL=ON/OFF

    Whether C# bindings should be built. It is ON by default, but only
    effective if C# runtime and development packages are found. Either .NET
    SDK can be used or Mono. The relevant options that can be set are described
    in ``cmake/modules/thirdparty/FindDotNetFrameworkSdk.cmake`` and
    ``cmake/modules/thirdparty/FindMono.cmake``.

.. option:: CSHARP_MONO=ON/OFF

    Forces the use of Mono as opposed to .NET to compile the C# bindings.

.. option:: CSHARP_LIBRARY_VERSION

    Sets the .NET (or Mono) target SDK to be used when compiling the C# binding libraries. `List of acceptable contents for .NET <https://docs.microsoft.com/en-us/dotnet/standard/frameworks#supported-target-frameworks>`_

.. option:: CSHARP_APPLICATION_VERSION

    Sets the .NET (or Mono) target SDK to be used when compiling the C# sample applications. `List of acceptable contents for .NET <https://docs.microsoft.com/en-us/dotnet/standard/frameworks#supported-target-frameworks>`_

.. option:: GDAL_CSHARP_ONLY=OFF/ON

    Build the C# bindings without building GDAL. This should be used when building the bindings on top of an existing GDAL installation - for instance on top of the CONDA package.

.. note::

    The C# bindings are made of several modules (OSGeo.GDAL, OSGeo.OGR, etc.)
    which link each against libgdal. Consequently, a static build of libgdal is
    not compatible with the bindings.

Driver specific options
+++++++++++++++++++++++

.. option:: GDAL_USE_PUBLICDECOMPWT

    The :ref:`raster.msg` driver is built only if this option is set to ON (default is OFF).
    Its effect is to download the https://gitlab.eumetsat.int/open-source/PublicDecompWT.git
    repository (requires the ``git`` binary to be available at configuration time)
    into the build tree and build the needed files from it into the driver.


Cross-compiling for Android
+++++++++++++++++++++++++++

First refer to https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-android
and to :source_file:`.github/workflows/android_cmake/start.sh` for
an example of a build script to cross-compile from Ubuntu.


Typical build issues
++++++++++++++++++++

How do I get PROJ ?
*******************

PROJ is the only required build-time dependency of GDAL that is not vendorized
in the GDAL source code tree. Consequently, the PROJ header and library must be available
when configuring GDAL's CMake. Consult `PROJ installation <https://proj.org/install.html>`__.

Conflicting PROJ libraries
**************************

If using a custom PROJ build (that is a PROJ build that does not come from
a distribution channel), it can sometimes happen that this custom PROJ build
conflicts with packaged dependencies, such as spatialite or libgeotiff, that
themselves link to another copy of PROJ.

The clean way to solve this is to rebuild from sources those other libraries
against the custom PROJ build.
For Linux based systems, given that C API/ABI has been preserved in the
PROJ 6, 7, 8, 9 series, if the custom PROJ build is more recent than the
PROJ used by those other libraries, doing aliases of the older ``libproj.so.XX``
name to the newer ``libproj.so.YY`` (with ``ln -s``) should work, although it is
definitely not recommended to use this solution in a production environment.

In any case, if ``ldd libgdal.so | grep libproj`` reports more than one line,
crashes will occur at runtime (often at process termination with a
``malloc_consolidate(): invalid chunk size`` and/or ``Aborted (core dumped)`` error message)


Autoconf/nmake (GDAL versions < 3.5.0)
--------------------------------------------------------------------------------

See http://web.archive.org/https://trac.osgeo.org/gdal/wiki/BuildHints for hints for GDAL < 3.5
autoconf and nmake build systems.
