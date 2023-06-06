.. _vector.filegdb:

ESRI File Geodatabase (FileGDB)
===============================

.. shortname:: FileGDB

.. build_dependencies:: FileGDB API library

The FileGDB driver provides read and write access to vector layers of
File Geodatabases (.gdb directories) created by ArcGIS 10 and above. The
dataset name must be the directory/folder name, and it must end with the
.gdb extension.

Note : the :ref:`OpenFileGDB driver <vector.openfilegdb>` driver exists as an
alternative built-in (i.e. not depending on a third-party library) driver.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Requirements
------------

`FileGDB API SDK <http://www.esri.com/apps/products/download/#File_Geodatabase_API_1.3>`__

Curve in geometries are supported on reading with GDAL >= 2.2.

Bulk feature loading
--------------------

The :config:`FGDB_BULK_LOAD` configuration option can be set to YES to speed-up
feature insertion (or sometimes solve problems when inserting a lot of
features (see http://trac.osgeo.org/gdal/ticket/4420). The effect of
this configuration option is to cause a write lock to be taken and a
temporary disabling of the indexes. Those are restored when the
datasource is closed or when a read operation is done.

Bulk load is enabled by default for newly
created layers (unless otherwise specified).

SQL support
-----------

SQL statements are run through the SQL engine of
the FileGDB SDK API. This holds for non-SELECT statements. However, due
to partial/inaccurate support for SELECT statements in current FileGDB
SDK API versions (v1.2), SELECT statements will be run by default by the
OGR SQL engine. This can be changed by specifying the *-dialect FileGDB*
option to ogrinfo or ogr2ogr.

Special SQL requests
~~~~~~~~~~~~~~~~~~~~

"GetLayerDefinition a_layer_name" and "GetLayerMetadata a_layer_name"
can be used as special SQL requests to get respectively the definition
and metadata of a FileGDB table as XML content.

Starting with GDAL 3.5, the "REPACK" special SQL request can be issued to
ask for database compaction.

Field domains
-------------

.. versionadded:: 3.3

Retrieving coded and range field domains are supported.
Writing support has been added in GDAL 3.5.

Relationships
-------------

.. versionadded:: 3.6

Relationship retrieval is supported.

Hiearchical organization
------------------------

.. versionadded:: 3.4

The hiearchical organization of tables and feature classes as top-level
element or within a feature dataset can be explored using the methods
:cpp:func:`GDALDataset::GetRootGroup`,
:cpp:func:`GDALGroup::GetGroupNames`, :cpp:func:`GDALGroup::OpenGroup`,
:cpp:func:`GDALGroup::GetVectorLayerNames` and :cpp:func:`GDALGroup::OpenVectorLayer`

Transaction support
-------------------

The FileGDB driver implements transactions at the database level,
through an emulation (as per :ref:`rfc-54`),
since the FileGDB SDK itself does not offer it. This works by backing up
the current state of a geodatabase when StartTransaction(force=TRUE) is
called. If the transaction is committed, the backup copy is destroyed.
If the transaction is rolled back, the backup copy is restored. So this
might be costly when operating on huge geodatabases.

Starting with GDAL 2.1, on Linux/Unix, instead of a full backup copy
only layers that are modified are backed up.

Note that this emulation has an unspecified behavior in case of
concurrent updates (with different connections in the same or another
process).

CreateFeature() support
-----------------------

The FileGDB SDK API does not allow to create a feature with a FID
specified by the user. Starting with GDAL 2.1, the FileGDB driver
implements a special FID remapping technique to enable the user to
create features at the FID of their choice.


Dataset Creation Options
------------------------

None.

Layer Creation Options
----------------------

-  .. lco:: FEATURE_DATASET

      When this option is set, the new layer will be
      created inside the named FeatureDataset folder. If the folder does
      not already exist, it will be created.

-  .. lco:: LAYER_ALIAS
      :since: 2.3

      Set layer name alias.

-  .. lco:: GEOMETRY_NAME
      :default: SHAPE

      Set name of geometry column in new layer.

-  .. lco:: GEOMETRY_NULLABLE
      :default: YES
      :since: 2.0

      Whether the values of the geometry column can be NULL.
      Can be set to NO so that geometry is required.

-  .. lco:: FID
      :default: OBJECTID

      Name of the OID column to create.
      Note: option was called OID_NAME in releases before GDAL 2

-  .. lco:: XYTOLERANCE
      :default: 0.01

      Controls (with :lco:`ZTOLERANCE` and :lco:`MTOLERANCE`) the snapping
      tolerance used for advanced ArcGIS features like network and topology
      rules. They won't effect any OGR operations, but they will by used by
      ArcGIS. The units of the parameters are the units of the coordinate
      reference system.

      ArcMap 10.0 and OGR defaults for XYTOLERANCE are 0.001m (or
      equivalent) for projected coordinate systems, and 0.000000008983153°
      for geographic coordinate systems.
      ArcMap 10.0 and OGR defaults for ZTOLERANCE and MTOLERANCE are 0.0001.

   .. lco:: ZTOLERANCE
      :default: 0.0001

   .. lco:: MTOLERANCE
      :default: 0.0001
      :since: 3.5.1

-  **XORIGIN, YORIGIN, ZORIGIN, MORIGIN, XYSCALE, ZSCALE, MSCALE**: These parameters
   control the `coordinate precision
   grid <http://help.arcgis.com/en/sdk/10.0/java_ao_adf/conceptualhelp/engine/index.html#//00010000037m000000>`__
   inside the file geodatabase. The dimensions of the grid are
   determined by the origin, and the scale. The origin defines the
   location of a reference grid point in space. The scale is the
   reciprocal of the resolution. So, to get a grid with an origin at 0
   and a resolution of 0.001 on all axes, you would set all the origins
   to 0 and all the scales to 1000.

   *Important*: The domain specified by
   ``(xmin=XORIGIN, ymin=YORIGIN, xmax=(XORIGIN + 9E+15 / XYSCALE), ymax=(YORIGIN + 9E+15 / XYSCALE))``
   needs to encompass every possible coordinate value for the feature
   class. If features are added with coordinates that fall outside the
   domain, errors will occur in ArcGIS with spatial indexing, feature
   selection, and exporting data.

   ArcMap 10.0 and OGR defaults:

   -  For geographic coordinate systems: XORIGIN=-400, YORIGIN=-400,
      XYSCALE=1000000000
   -  For projected coordinate systems: XYSCALE=10000 for the default
      XYTOLERANCE of 0.001m. XORIGIN and YORIGIN change based on the
      coordinate system, but the OGR default of -2147483647 is suitable
      with the default XYSCALE for all coordinate systems.
   -  ZORIGIN and MORIGIN: -100000
   -  ZSCALE and MSCALE: 10000

   ..  note:: MORIGIN and MSCALE added in GDAL 3.5.1

-  .. lco:: XML_DEFINITION

      When this option is set, its
      value will be used as the XML definition to create the new table. The
      root node of such a XML definition must be a <esri:DataElement>
      element conformant to FileGDBAPI.xsd

-  .. lco:: CREATE_MULTIPATCH
      :choices: YES, NO

      When this option is set,
      geometries of layers of type MultiPolygon will be written as
      MultiPatch

-  .. lco:: CONFIGURATION_KEYWORD
      :choices: DEFAULTS, TEXT_UTF16, MAX_FILE_SIZE_4GB, MAX_FILE_SIZE_256TB, GEOMETRY_OUTOFLINE, BLOB_OUTOFLINE, GEOMETRY_AND_BLOB_OUTOFLINE

      Customize how data is stored. By default text in UTF-8 and data up to 1TB

-  .. lco:: CREATE_SHAPE_AREA_AND_LENGTH_FIELDS
      :choices: YES, NO
      :default: NO
      :since: 3.6.0

      When this option is set,
      a Shape_Area and Shape_Length special fields will be created for polygonal
      layers (Shape_Length only for linear layers). These fields will automatically
      be populated with the feature's area or length whenever a new feature is
      added to the dataset or an existing feature is amended.
      When using ogr2ogr with a source layer that has Shape_Area/Shape_Length
      special fields, and this option is not explicitly specified, it will be
      automatically set, so that the resulting FileGeodatabase has those fields
      properly tagged.

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are
available:

- .. config:: FGDB_BULK_LOAD
     :choices: YES, NO

     Can be set to YES to speed-up
     feature insertion (or sometimes solve problems when inserting a lot of
     features (see http://trac.osgeo.org/gdal/ticket/4420). The effect of
     this configuration option is to cause a write lock to be taken and a
     temporary disabling of the indexes. Those are restored when the
     datasource is closed or when a read operation is done. Bulk load is
     enabled by default for newly created layers (unless otherwise specified).

Examples
--------

-  Read layer from FileGDB and load into PostGIS:
-  Get detailed info for FileGDB:

Building Notes
--------------

Read the `GDAL Windows Building example for
Plugins <http://trac.osgeo.org/gdal/wiki/BuildingOnWindows>`__. You will
find a similar section in nmake.opt for FileGDB. After you are done, go
to the *$gdal_source_root\ogr\ogrsf_frmts\filegdb* folder and execute:

``nmake /f makefile.vc plugin         nmake /f makefile.vc plugin-install``

Known Issues
------------

-  The SDK is known to be unable to open layers with particular spatial
   reference systems. This might be the case if messages "FGDB: Error
   opening XXXXXXX. Skipping it (Invalid function arguments.)" when
   running ``ogrinfo --debug on the.gdb`` (reported as warning in GDAL
   2.0). Using the OpenFileGDB driver will generally solve that issue.
-  FGDB coordinate snapping will cause geometries to be altered during
   writing. Use the origin and scale layer creation options to control
   the snapping behavior.
-  Driver can't read data in SDC format (Smart Data Compression) because
   operation is not supported by the ESRI SDK.
-  Reading data compressed in CDF format (Compressed Data Format)
   requires ESRI SDK 1.4 or later.
-  Some applications create FileGeodatabases with non-spatial tables which are
   not present in the GDB_Items metadata table. These tables cannot be opened
   by the ESRI SDK, so GDAL will automatically fallback to the OpenFileGDB
   driver to read these tables. Accordingly they will be opened with the
   limitations of the OpenFileGDB driver (for instance, they will be
   read only).


Other limitations
-----------------

- The FileGeodatabase format (and thus the driver) does not support 64-bit integers.

Links
-----

-  `ESRI File Geodatabase API
   Page <https://github.com/Esri/file-geodatabase-api/>`__
-  :ref:`OpenFileGDB driver <vector.openfilegdb>`, not depending on a
   third-party library/SDK
