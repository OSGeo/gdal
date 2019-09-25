.. _vector.EFAL:

MapInfo EFAL
============

.. shortname:: EFAL

This driver supports the MapInfo TAB file format including the MapInfo (Native) and the new MapInfo Extended (NATIVEX) formats.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Contents
--------

#. `Overview of GDAL/OGR Driver <#driver_overview_efal>`__
#. `Differences with MITAB <#differences_with_mitab>`__
#. `Options <#options>`__
#. `Issues and Limitations <#issues_efal>`__
#. `Building GDAL <#building_gdal_efal>`__

--------------

.. _driver_overview_efal:

Overview of GDAL/OGR Driver
---------------------------

This Driver supports the MapInfo Native and NativeX formats using the
EFAL library from Pitney Bowes. The driver supports reading, writing,
and creating data in these formats. The MITAB library does not support
the newer MapInfo Extended (NativeX) format which allows for larger file
sizes, unicode character encodings, and other internal enhancements.

EFAL is a new thread-safe SDK provided by Pitney Bowes. You can download the SDK from 
https://www.pitneybowes.com/us/campaign/sdkrequest.html.
The EFAL SDK enables access to the MapInfo SQL data access engine.
It also provides the ability to open, create, query, and modify data 
in MapInfo TAB and MapInfo Enhanced TAB file formats including the MapInfo Seamless tables. 
The SDK includes detailed documentation on the API. The SDK DLLs and related
files must be available on your machine for the driver to execute.

The EFAL driver for GDAL is supported on 64-bit Windows, Ubuntu, AmazonLinux, OracleLinux and CentOS.

This driver supports the following capabilities:

Opening and querying of MapInfo Native and NativeX TAB files

-  The EFAL API uses WKB as the interchange format for geometries.
   -  Arc and Ellipse types are also not currently supported. 
   -  RECT and Rounded RECT types are returned as polygons.
   -  Legacy Text type is returned as custom WKB geometry.
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
-  Feature property types: Integer Integer64 Real String Date DateTime Time
-  Blocksize options

--------------

.. _differences_with_mitab:

Differences with MITAB
----------------------

-  No MID/MIF support.
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

.. _issues_efal:

Issues and Limitations
----------------------

-  Driver does not support GDAL virtual filesystem.

--------------

.. _building_gdal_efal:

Building and Using GDAL
-----------------------

Building
~~~~~~~~

The EFAL driver will build as part of the GDAL even if the EFAL SDK is
not found or not on the machine. This will allow GDAL to always be
EFAL-ready. A new header file (EFALLIB.h) is created to dynamically
load the EFAL DLL if found, and calls GetProcAddress for each of the
function entry points. To build GDAL for x64 architecture, for example,
from a command prompt, run the following:

::

   nmake -f makefile.vc MSVC_VER=1900 DEBUG=1 WIN64=YES

| **NOTE:** vcvars must have already been run - if using a VC comamnd
  prompt this will be automatic but will cause issues with 32 bit
  builds.

EFAL SDK Runtime
~~~~~~~~~~~~~~~~

The EFAL driver needs the EFAL SDK to be installed on the machine to work with GDAL.

Download the EFAL SDK by navigating to "https://www.pitneybowes.com/us/campaign/sdkrequest.html"
and fill up the SDK request form to receive the download link on your e-mail.
Once the SDK package is downloaded, unzip the package on your machine at the desired location.

Structure of EFAL SDK package includes three main folders:

-  data --> Sample data folder.
-  export --> Binaries folder.
   -  Common --> Common files used across platforms.
   -  ua64 --> 64 bit Binaries for AmazonLinux.
   -  uc64 --> 64 bit Binaries for CentOS.
   -  uo64 --> 64 bit Binaries for OracleLinux.
   -  uu64 --> 64 bit Binaries for Ubuntu.
   -  ux64 --> 64 bit Binaries for Windows.
   -  uw32 --> 32 bit Binaries for Windows.
-  Solution --> Samples folder.

Choose the binaries for the desired platform from the "export" folder and copy all files from the "export/Common" folder into the binaries folder. For example:

::
    To use binaries for Ubuntu, copy all the files from the "export/Common" folder to the "export/uu64". 
    Create the system environment variable EFAL_SDK_DIR pointing to the "export/uu64" directory.

When using GDAL with this driver, the location of the EFAL runtime must
be available on the system path. For example

::

   SET PATH=%PATH%;%EFAL_SDK_DIR%

| **NOTE:** Please refer to "https://www.pitneybowes.com/us/campaign/sdkrequest.html" for terms of
  usage of EFAL SDK.
  
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
