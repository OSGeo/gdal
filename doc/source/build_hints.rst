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
Note that the case of the package name matters for the _ROOT, _INCLUDE_DIR
and _LIBRARY variables.

Most dependencies that would be found can also be disabled by setting the
following option:

.. option:: GDAL_USE_<Packagename_in_upper_case>:BOOL=ON/OFF

    Control whether a found dependency can be used for the GDAL build.


Armadillo
*********

The Armadillo C++ library is used to speed up computations related to the
Thin Plate Spline transformer. See https://cmake.org/cmake/help/latest/module/FindArmadillo.html
for details.
On Windows builds using Conda-Forge depedencies, the following packages may also
need to be installed: ``blas blas-devel libblas libcblas liblapack liblapacke``


CFITSIO
*******

The C FITS I/O library is required for the :ref:`raster.fits` driver.
Can be detected with pkg-config.

.. option:: CFITSIO_INCLUDE_DIR

    Path to an include directory with the ``fitsio.h`` header file.

.. option:: PDFium_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_CFITSIO=ON/OFF

    Control whether to use CFITSIO. Defaults to ON when CFITSIO is found.


Crnlib
******

Crnlib / crunch is required for the :ref:`raster.dds` driver.

.. option:: Crnlib_INCLUDE_DIR

  Path to Crnlib include directory with ``crunch/crnlib.h`` header.

.. option:: Crnlib_LIBRARY

  Path to Crnlib library to be linked.

CURL
****

It is required for all network accesses (HTTP, etc.).

.. option:: CURL_INCLUDE_DIR

    Path to an include directory with the ``curl`` directory.

.. option:: CURL_LIBRARY_RELEASE

    Path to a shared or static library file, such as ``libcurl.dll``,
    ``libcurl.so``, ``libcurl.lib``, or other name.

.. option:: GDAL_USE_CURL=ON/OFF

    Control whether to use Curl. Defaults to ON when Curl is found.

ECW
***

It is required for the :ref:`raster.ecw` driver.
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

FileGDB
*******

FileGDB_ROOT or CMAKE_PREFIX_PATH should point to the directory of the SDK.
It is required for the :ref:`vector.filegdb` driver (not to be confused with
the :ref:`vector.openfilegdb` driver that has no external requirements)

.. option:: FileGDB_INCLUDE_DIR

    Path to the include directory with the ``FileGDBAPI.h`` header file.

.. option:: FileGDB_LIBRARY

    Path to library file

.. option:: FileGDB_LIBRARY_RELEASE

    Path to Release library file (only used on Windows)

.. option:: FileGDB_LIBRARY_DEBUG

    Path to Debug library file (only used on Windows)


FYBA
****

The OpenFyba libraries are needed to build the :ref:`vector.sosi` driver.

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
If not found, an internal copy of libgeotiff will be used.

.. option:: GEOTIFF_INCLUDE_DIR

    Path to an include directory with the libgeotiff header files.

.. option:: GEOTIFF_LIBRARY_RELEASE

    Path to a shared or static library file, such as ``geotiff.dll``,
    ``libgeotiff.so``, ``geotiff.lib``, or other name. A similar variable
    ``GEOTIFF_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.

.. option:: GDAL_USE_GEOTIFF=ON/OFF

    Control whether to use external libgeotiff. Defaults to ON when external libgeotiff is found.

.. option:: GDAL_USE_LIBGEOTIFF_INTERNAL=ON/OFF

    Control whether to use internal libgeotiff copy. Defaults to ON when external
    libgeotiff is not found.


GTA
***

The GTA library is required for the :ref:`raster.gta` driver.

.. option:: GTA_INCLUDE_DIR

    Path to an include directory with the ``gta/gta.h`` header file.

.. option:: GTA_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_KEY=ON/OFF

    Control whether to use GTA. Defaults to ON when GTA is found.


HEIF
****

The HEIF (>= 1.1) library used by the :ref:`raster.heif` driver.
Can be detected with pkg-config.

.. option:: HEIF_INCLUDE_DIR

    Path to an include directory with the ``libheif/heif.h`` header file.

.. option:: HEIF_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_HEIF=ON/OFF

    Control whether to use HEIF. Defaults to ON when HEIF is found.


HDF5
****

The HDF5 C library is needed for the :ref:`raster.hdf5` and :ref:`raster.bag` drivers.
The HDF5 CXX library is needed for the :ref:`raster.kea` driver.
The https://cmake.org/cmake/help/latest/module/FindHDF5.html module is used to
detect the HDF5 library.


IDB
***

The Informix DataBase Client SDK is needed to build the :ref:`vector.idb` driver.
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


JXL
***

JPEG-XL library used by the :ref:`raster.gtiff` driver, when built against internal libtiff.
Can be detected with pkg-config.

.. option:: JXL_INCLUDE_DIR

    Path to an include directory with the ``jxl/decode.h`` header file.

.. option:: JXL_LIBRARY

    Path to a shared or static library file.


KDU
***

The Kakadu library is required for the :ref:`raster.jp2kak` and :ref:`raster.jpipkak` drivers.
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

KEA
***

The KEA library is required for the :ref:`raster.kea` driver. The HDF5 CXX library is also
required.

.. option:: KEA_INCLUDE_DIR

    Path to an include directory with the ``libkea/KEACommon.h`` header file.

.. option:: KEA_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_KEA=ON/OFF

    Control whether to use KEA. Defaults to ON when KEA is found.


LURATECH
********

The Luratech JPEG2000 SDK is required for the :ref:`raster.jp2lura` driver.

LURATECH_ROOT or CMAKE_PREFIX_PATH should point to the directory of the SDK.

.. option:: LURATECH_INCLUDE_DIR

    Path to the include directory with the ``lwf_jp2.h`` header file.

.. option:: LURATECH_LIBRARY

    Path to library file lib_lwf_jp2.a / lwf_jp2.lib


MONGOCXX
********

The MongoCXX and BsonCXX libraries are needed to build the :ref:`vector.mongodbv3` driver.
Can be detected with pkg-config.

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

The MRSID Raster DSDK is required for the :ref:`raster.mrsid` driver.

MRSID_ROOT or CMAKE_PREFIX_PATH should point to the directory of the SDK ending with
Raster_DSDK. Note that on Linux, its lib subdirectory should be in the
LD_LIBRARY_PATH so that the linking of applications succeeds and libtbb.so can
be found.

.. option:: MRSID_INCLUDE_DIR

    Path to the include directory with the ``lt_base.h`` header file.

.. option:: MRSID_LIBRARY

    Path to library file libltidsdk

.. option:: GDAL_ENABLE_FRMT_JP2MRSID

    Whether to enable JPEG2000 support through the MrSID SDK. The default value
    of this option is OFF.


MSSQL_NCLI
**********

Microsoft SQL Native Client Library to enable bulk copy in the :ref:`vector.mssqlspatial`
driver. If both MSSQL_NCLI and MSSQL_ODBC are found and enabled, MSSQL_ODBC
will be used.
The library is normally found if installed in standard location, and at version 11.

.. option:: MSSQL_NCLI_VERSION

  Major version of the Native Client, typically 11

.. option:: MSSQL_NCLI_INCLUDE_DIR

  Path to include directory with ``sqlncli.h`` header.

.. option:: MSSQL_NCLI_LIBRARY

  Path to library to be linked.

MSSQL_ODBC
**********

Microsoft SQL Native ODBC driver Library to enable bulk copy in the :ref:`vector.mssqlspatial`
driver. If both MSSQL_NCLI and MSSQL_ODBC are found and enabled, MSSQL_ODBC
will be used.
The library is normally found if installed in standard location, and at version 17.

.. option:: MSSQL_ODBC_VERSION

  Major version of the Native Client, typically 17

.. option:: MSSQL_ODBC_INCLUDE_DIR

  Path to include directory with ``msodbcsql.h`` header.

.. option:: MSSQL_ODBC_LIBRARY

  Path to library to be linked.

ODBC
****

ODBC is required for various drivers: :ref:`vector.odbc`, :ref:`vector.pgeo`,
:ref:`vector.walk` and :ref:`vector.mssqlspatial`.
It is normally automatically found in system directories on Unix and Windows.

.. option:: ODBC_INCLUDE_DIR

  Path to ODBC include directory with ``sql.h`` header.

.. option:: ODBC_LIBRARY

  Path to ODBC library to be linked.


Oracle
******

The Oracle Instant Client SDK is required for the :ref:`vector.oci` and
the :ref:`raster.georaster` drivers

.. option:: Oracle_ROOT

    Path to the root directory of the Oracle Instant Client SDK


PCRE2
*****

Perl-compatible Regular Expressions support, for the REGEXP operator in drivers
using SQLite3.

.. option:: PCRE2_INCLUDE_DIR

    Path to an include directory with the ``pcre2.h`` header file.

.. option:: PCRE2_LIBRARY

    Path to a shared or static library file with "pcre2-8" in its name


PDFium
******

The PDFium library is one of the possible backends for the :ref:`raster.pdf` driver.

.. option:: PDFium_INCLUDE_DIR

    Path to an include directory with the ``public/fpdfview.h`` header file.

.. option:: PDFium_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_PDFIUM=ON/OFF

    Control whether to use PDFium. Defaults to ON when PDFium is found.


PROJ
****

PROJ >= 6 is a required dependency for GDAL.

.. option:: PROJ_INCLUDE_DIR

    Path to an include directory with the ``proj.h`` header file.

.. option:: PROJ_LIBRARY_RELEASE

    Path to a shared or static library file, such as ``proj.dll``,
    ``libproj.so``, ``proj.lib``, or other name. A similar variable
    ``PROJ_LIBRARY_DEBUG`` can also be specified to a similar library for
    building Debug releases.


QHULL
*****

The QHULL library is used for the linear interpolation of gdal_grid. If not
found, an internal copy is used.

.. option:: QHULL_INCLUDE_DIR

    Path to an include directory with the ``libqhull_r/libqhull_r.h`` header file.

.. option:: QHULL_LIBRARY

    Path to a shared or static library file to the reentrant library.

.. option:: GDAL_USE_QHULL=ON/OFF

    Control whether to use QHULL. Defaults to ON when QHULL is found.

.. option:: GDAL_USE_QHULL_INTERNAL=ON/OFF

    Control whether to use internal QHULL copy. Defaults to ON when external
    QHULL is not found.


RASTERLITE2
***********

The RasterLite2 (>= 1.1.0) library used by the :ref:`raster.rasterlite2` driver.
Can be detected with pkg-config.

.. option:: RASTERLITE2_INCLUDE_DIR

    Path to an include directory with the ``rasterlite2/rasterlite2.h`` header file.

.. option:: RASTERLITE2_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_RASTERLITE2=ON/OFF

    Control whether to use RasterLite2. Defaults to ON when RasterLite2 is found.


SPATIALITE
**********

The Spatialite library used by the :ref:`vector.sqlite` and :ref:`vector.gpkg` drivers,
and the :ref:`sql_sqlite_dialect`.
Can be detected with pkg-config.

.. option:: SPATIALITE_INCLUDE_DIR

    Path to an include directory with the ``spatialite.h`` header file.

.. option:: SPATIALITY_LIBRARY

    Path to a shared or static library file.

.. option:: GDAL_USE_SPATIALITE=ON/OFF

    Control whether to use Spatialite. Defaults to ON when Spatialite is found.


SQLite3
*******

It is required for the :ref:`vector.sqlite` and :ref:`vector.gpkg` drivers
(and also used by other drivers), and the :ref:`sql_sqlite_dialect`.

.. option:: SQLite3_INCLUDE_DIR

    Path to an include directory with the ``sqlite3.h`` header file.

.. option:: SQLite3_LIBRARY

    Path to a shared or static library file, such as ``sqlite3.dll``,
    ``libsqlite3.so``, ``sqlite3.lib`` or other name.

.. option:: GDAL_USE_SQLITE3=ON/OFF

    Control whether to use SQLite3. Defaults to ON when SQLite3 is found.

TEIGHA
******

The TEIGHA / Open Design Alliance libraries are required for the
:ref:`vector.dwg` and :ref:`vector.dgnv8` drivers.
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

TIFF
****

libtiff required for the :ref:`raster.gtiff` drivers, and a few other drivers.
If not found, an internal copy of libtiff will be used.

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
It is required for the :ref:`raster.tiledb` driver


OpenEXR
*******

It is required for the :ref:`raster.exr` driver

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

.. option:: GDAL_USE_PUBLICDECOMPWT=ON

    The :ref:`raster.msg` driver is built only if this option is set. Its effect is to
    download the https://gitlab.eumetsat.int/open-source/PublicDecompWT.git
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
