.. _vector.EFAL:

MapInfo EFAL
============

.. shortname:: EFAL

This driver supports the MapInfo TAB file format. This Driver supports
the MapInfo (Native) as well as the new MapInfo Extended (NATIVEX)
formats.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Contents
--------

#. `Overview of GDAL/OGR Driver <#driver_overview>`__
#. `Differences with MITAB <#differences_with_mitab>`__
#. `Options <#options>`__
#. `Issues and Limitations <#issues>`__
#. `Building GDAL <#building_gdal>`__

--------------

.. _driver_overview:

Overview of GDAL/OGR Driver
---------------------------

This Driver supports the MapInfo Native and NativeX formats using the
EFAL library from Pitney Bowes. The driver supports reading, writing,
and creating data in these formats. The MITAB library does not support
the newer MapInfo Extended (NativeX) format which allows for larger file
sizes, unicode character encodings, and other internal enhancements.

EFAL is a new SDK provided by Pitney Bowes. It will eventually be made
available for download from Pitney Bowes and will probably be versioned
consistently with MapInfo Pro releases. The EFAL SDK enables access to
the MapInfo SQL data access engine and provides the ability to open,
create, query, and modify data in MapInfo TAB and MapInfo Enhanced TAB
file formats. This includes MapInfo Seamless tables. EFAL is a
thread-safe SDK. The SDK includes detailed documentation on the API. The
SDK must be used to build the GDAL driver. The SDK DLLs and related
files must also be distributed for the driver to execute.

The EFAL driver for GDAL is currently only supported for Windows
operating systems. Pitney Bowes plans to port the EFAL library to
non-Windows platforms.

This driver supports the following capabilities:

Opening and querying of MapInfo Native and NativeX TAB files

-  The EFAL API uses WKB as the interchange format for geometries. Due
   to this, currently, the API does not support some legacy MapInfo
   geometry types; most notably TEXT geometry types. Arc and Ellipse
   types are also not currently supported. RECT and Rounded RECT types
   are returned as polygons.
-  Coordinate systems are supported using the MITAB capabilities through
   the importFromMICoordSys and exportToMICoordSys methods on the
   OGRSpatialReference class. Due to this, the MITAB driver must be
   included in any build of GDAL that intends to include the EFAL
   driver.
-  Styles in TAB files are converted to/from OGR style strings;
   currently to a very similar level of interoperability as MITAB.
-  Spatial filtering - MBR only
-  Attribute filtering using SQL expressions
-  Read by Feature ID (random read)
-  Opening a folder as a datasource will open and expose all TAB files
   within the folder (like MITAB)
-  File locking option when opening to improve random read performance
   (details below)

Editing of features within MapInfo tables

-  Insert, Update, and Delete supported
-  File locking option when opening to improve insert, update, delete
   performance (details below)

Creation of new MapInfo Native and NativeX TAB tables (layers)

-  Can specify Native or NativeX
-  Character encoding options including UTF-8 and UTF-16 for NativeX
-  Feature property types: Integer Integer64 Real String Date DateTime
   Time
-  Blocksize options

--------------

.. _differences_with_mitab:

Differences with MITAB
----------------------

-  No MID/MIF support
-  Table (layer) schema lists geometry column as having name "OBJ"
   whereas MITAB does not.
-  EFAL reports geometry type for table level whereas MITAB reports it
   as Unknown.
-  Style strings - order of PEN and BRUSH clauses are usually reversed
   although results are the same.

--------------

Options
-------

Open Options
~~~~~~~~~~~~

These options apply to either the input dataset open option (-oo) or the
destination dataset open option (-doo)

MODE={READ-ONLY|LOCK-READ|READ-WRITE|LOCK-WRITE} - Controls how the
table is opened and files are locked.

-  READ-ONLY - table is opened in read-only mode and files are not
   locked
-  LOCK-READ - table is opened in read-only mode and files are locked
   for shareable read
-  READ-WRITE - table is opened in write mode (supports insert, update,
   and delete) but files are only locked during editing operations
-  LOCK-WRITE - table is opened in write mode and files are locked for
   write for the lifetime of the table

Layer Creation options
~~~~~~~~~~~~~~~~~~~~~~

These options apply to the layer creation options (-lco)

-  BOUNDS=[xmin],[ymin],[xmax],[ymax]

Dataset Creation options
~~~~~~~~~~~~~~~~~~~~~~~~

These options apply to the dataset creation options (-dcco)

-  FORMAT={NATIVE|NATIVEX}, Default is NATIVE
-  CHARSET - Default is WLATIN1 for NATIVE, UTF-8 for NATIVEX. The list
   of allowed values is
-  BLOCKSIZE=[number], Default is 16384, max is 32768.

--------------

.. _issues:

Issues and Limitations
----------------------

-  Driver does not support gdal virtual filesystem.

--------------

.. _building_gdal:

Building and Using GDAL
-----------------------

Building
~~~~~~~~

The EFAL driver will build as part of the GDAL even if the EFAL SDK is
not found or not on the machine. This will allow GDAL to always be
EFAL-ready. A new header file (EFALLIB.h) was created to dynamically
load the EFAL DLL if found and calls GetProcAddress for each of the
function entry points. To build GDAL for x64 architecture, for example,
from a command prompt run the following:

::

   nmake -f makefile.vc MSVC_VER=1900 DEBUG=1 WIN64=YES

| **NOTE:** vcvars must have already been run - if using a VC comamnd
  prompt this will be automatic but will cause issues with 32 bit
  builds.

Runtime
~~~~~~~

When using GDAL with this driver, the location of the EFAL runtime must
be available on the system path. For example

::

   SET PATH=%PATH%;%EFAL_SDK_DIR%

Usage examples
~~~~~~~~~~~~~~

ogr2ogr -f "MapInfo EFAL" c:\data\new\usa_caps.TAB
c:\data\samples\usa_caps.tab

ogr2ogr -dsco CHARSET=ISO8859_1 -f "MapInfo EFAL"
c:\data\new\usa_caps.TAB c:\data\samples\usa_caps.tab

ogr2ogr -dsco FORMAT=NATIVEX -f "MapInfo EFAL" -f "MapInfo EFAL"
c:\data\new\usa_caps.TAB c:\data\samples\usa_caps.tab

ogr2ogr -dsco CHARSET=ISO8859_1 -dsco FORMAT=NATIVEX -f "MapInfo EFAL"
c:\data\new\usa_caps.TAB c:\data\samples\usa_caps.tab

ogr2ogr -oo MODE=LOCK-WRITE -f "MapInfo EFAL" c:\data\new\usa_caps.TAB
c:\data\samples\usa_caps.tab

ogr2ogr -lco BOUNDS=-180,15,-60,75 -f "MapInfo EFAL"
c:\data\new\usa_caps.TAB c:\data\samples\usa_caps.tab
