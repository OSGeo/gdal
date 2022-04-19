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

You can assemble dependency settings in a file ``ConfigUser.cmake`` and use it with the -C option.
The file contains set() commands that use the CACHE option. You can set for example a different name
for the shared lib, *e.g.* ``set (GDAL_LIB_OUTPUT_NAME gdal_x64 CACHE STRING "" FORCE)``::

    cmake .. -C ConfigUser.cmake

.. warning::

    When iterating to configure GDAL to add/modify/remove dependencies,
    some cache variables can remain in CMakeCache.txt from previous runs, and
    conflict with new settings. If strange errors appear during cmake run,
    you may try removing CMakeCache.txt to start from a clean state.


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

.. option:: GDAL_SET_INSTALL_RELATIVE_RPATH=OFF

    Set to ON so that the rpath of installed binaries is written as a relative
    path to the library. This option overrides the
    `CMAKE_INSTALL_RPATH <https://cmake.org/cmake/help/latest/variable/CMAKE_INSTALL_RPATH.html>`__
    variable, and assumes that the
    `CMAKE_SKIP_INSTALL_RPATH <https://cmake.org/cmake/help/latest/variable/CMAKE_SKIP_INSTALL_RPATH.html>`__
    variable is not set.

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

    Path to a shared or static library file, such as ``libcurl.dll``,
    ``libcurl.so``, ``libcurl.lib``, or other name.

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

    Path to a shared or static library file, such as ``geotiff.dll``,
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


JPEG12
******

libjpeg-12 bit can be used by the :ref:`raster.jpeg`, :ref:`raster.gtiff` (when using internal libtiff),
:ref:`raster.jpeg`, :ref:`raster.marfa` and :ref:`raster.nitf` drivers to handle
JPEG images with a 12 bit depth. It is only supported with the internal libjpeg (6b).
This can be used independently of if for regular 8 bit JPEG an external or internal
libjpeg is used.

.. option:: GDAL_USE_JPEG12_INTERNAL=ON/OFF

    Control whether to use internal libjpeg-12 copy. Defaults to ON.


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



LURATECH
********

The Luratech JPEG2000 SDK (closed source/proprietary) is required for the
:ref:`raster.jp2lura` driver.

LURATECH_ROOT or CMAKE_PREFIX_PATH should point to the directory of the SDK.

.. option:: LURATECH_INCLUDE_DIR

    Path to the include directory with the ``lwf_jp2.h`` header file.

.. option:: LURATECH_LIBRARY

    Path to library file lib_lwf_jp2.a / lwf_jp2.lib

.. option:: GDAL_USE_LURATECH=ON/OFF

    Control whether to use LURATECH. Defaults to ON when LURATECH is found.


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

.. note:: It is disabled by default even when detected, since the current OpenCL
          warping implementation lags behind the generic implementation.

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


OpenSSL
*******

The Crypto component of the `OpenSSL <https://github.com/openssl/openssl>`_ library
can be used for the RSA SHA256 signing functionality used by some authentication
methods of Google Cloud. It might be required to use the :ref:`raster.eedai`
images or use the :ref:`/vsigs/ <vsigs>` virtual file system.

See https://cmake.org/cmake/help/latest/module/FindOpenSSL.html for details on
how to configure the library

.. option:: GDAL_USE_OPENSSL=ON/OFF

    Control whether to use OpenSSL. Defaults to ON when OpenSSL is found.


Oracle
******

The Oracle Instant Client SDK (closed source/proprietary) is required for the
:ref:`vector.oci` and the :ref:`raster.georaster` drivers

.. option:: Oracle_ROOT

    Path to the root directory of the Oracle Instant Client SDK

.. option:: GDAL_USE_ORACLE=ON/OFF

    Control whether to use Oracle. Defaults to ON when Oracle is found.


Parquet
*******

The Parquet component of the `Apache Arrow C++ <https://github.com/apache/arrow/tree/master/cpp>`
library is required for the :ref:`vector.parquet` driver.
Specify install prefix in the ``CMAKE_PREFIX_PATH`` variable.

.. option:: GDAL_USE_PARQUET=ON/OFF

    Control whether to use Parquet. Defaults to ON when Parquet is found.


PCRE2
*****

`PCRE2 <https://github.com/PhilipHazel/pcre2>`_ implements Perl-compatible
Regular Expressions support. It is used for the REGEXP operator in drivers using SQLite3.

.. option:: PCRE2_INCLUDE_DIR

    Path to an include directory with the ``pcre2.h`` header file.

.. option:: PCRE2_LIBRARY

    Path to a shared or static library file with "pcre2-8" in its name

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

`PROJ <https://github.com/OSGeo/PROJ/>`_ >= 6 is a *required* dependency for GDAL.

.. option:: PROJ_INCLUDE_DIR

    Path to an include directory with the ``proj.h`` header file.

.. option:: PROJ_LIBRARY_RELEASE

    Path to a shared or static library file, such as ``proj.dll``,
    ``libproj.so``, ``proj.lib``, or other name. A similar variable
    ``PROJ_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.


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

    Path to a shared or static library file, such as ``sqlite3.dll``,
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

    Path to the SWIG executable


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

    Path to a shared or static library file, such as ``tiff.dll``,
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


Selection of drivers
++++++++++++++++++++

By default, all drivers that have their build requirements satisfied will be
built-in in the GDAL core library.

The following options are available to select a subset of drivers:

.. option:: GDAL_ENABLE_DRIVER_<driver_name>:BOOL=ON/OFF

.. option:: OGR_ENABLE_DRIVER_<driver_name>:BOOL=ON/OFF

    Independently of options that control global behavior, drivers can be individually
    enabled or disabled with those options.

.. option:: GDAL_BUILD_OPTIONAL_DRIVERS:BOOL=ON/OFF

.. option:: OGR_BUILD_OPTIONAL_DRIVERS:BOOL=ON/OFF

    Globally enable/disable all GDAL/raster or OGR/vector drivers.
    More exactly, setting those variables to ON affect the default value of the
    ``GDAL_ENABLE_DRIVER_<driver_name>`` or ``OGR_ENABLE_DRIVER_<driver_name>`` variables
    (when they are not yet set).

    This can be combined with individual activation of a subset of drivers by using
    the ``GDAL_ENABLE_DRIVER_<driver_name>:BOOL=ON`` or ``OGR_ENABLE_DRIVER_<driver_name>:BOOL=ON``
    variables. Note that changing the value of GDAL_BUILD_OPTIONAL_DRIVERS/
    OGR_BUILD_OPTIONAL_DRIVERS after a first run of CMake does not change the
    activation of individual drivers. It might be needed to pass
    ``-UGDAL_ENABLE_DRIVER_* -UOGR_ENABLE_DRIVER_*`` to reset their state.


Example of minimal build with the JP2OpenJPEG and SVG drivers enabled::

    cmake .. -UGDAL_ENABLE_DRIVER_* -UOGR_ENABLE_DRIVER_* \
             -DGDAL_BUILD_OPTIONAL_DRIVERS:BOOL=OFF -DOGR_BUILD_OPTIONAL_DRIVERS:BOOL=OFF \
             -DGDAL_ENABLE_DRIVER_JP2OPENPEG:BOOL=ON \
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
             -DGDAL_ENABLE_DRIVER_JP2OPENPEG_PLUGIN:BOOL=OFF

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
    It is used to set the value of the ``--prefix`` option of ``python setup.py install``.

.. option:: GDAL_PYTHON_INSTALL_LAYOUT

    This option can be specified to set the value of the ``--install-layout``
    option of ``python setup.py install``. The install layout is by default set to
    ``deb`` when it is detected that the Python installation looks for
    the ``site-packages`` subdirectory. Otherwise it is unspecified.

.. option:: GDAL_PYTHON_INSTALL_LIB

    This option can be specified to set the value of the ``--install-lib``
    option of ``python setup.py install``. It is only taken into account on
    MacOS systems, when the Python installation is a framework.

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

    Subdirectory into which to install the gdalalljni library and the .jar
    files. It defaults to "${CMAKE_INSTALL_DATADIR}/java"

Option only to be used by maintainers:

.. option:: GPG_KEY

    GPG key to sign build artifacts. Needed to generate bundle.jar.

.. option:: GPG_PASS

    GPG pass phrase to sign build artifacts.


Driver specific options
+++++++++++++++++++++++

.. option:: GDAL_USE_PUBLICDECOMPWT

    The :ref:`raster.msg` driver is built only if this option is set to ON (default is OFF).
    Its effect is to download the https://gitlab.eumetsat.int/open-source/PublicDecompWT.git
    repository (requires the ``git`` binary to be available at configuration time)
    into the build tree and build the needed files from it into the driver.


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
        sqlite tiledb zstd charls cryptopp cgal librttopo libkml openssl xz

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

Cross-compiling for Android
+++++++++++++++++++++++++++

First refer to https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-android
and to https://github.com/OSGeo/gdal/blob/master/.github/workflows/android_cmake/start.sh for
an example of a build script to cross-compile from Ubuntu.
