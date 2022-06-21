.. _vector.openfilegdb:

ESRI File Geodatabase (OpenFileGDB)
===================================

.. shortname:: OpenFileGDB

.. built_in_by_default::

The OpenFileGDB driver provides read, write and update access to vector layers of File
Geodatabases (.gdb directories) created by ArcGIS 9 and above. The
dataset name must be the directory/folder name, and it must end with the
.gdb extension.

It can also read directly zipped .gdb directories (with .gdb.zip
extension), provided they contain a .gdb directory at their first level.

A specific .gdbtable file (including "system" tables) can also be opened
directly.

Curve in geometries are supported with GDAL >= 2.2.

Write and update capabilities are supported since GDAL >= 3.6

Driver capabilities
-------------------

.. supports_create::

    .. versionadded:: GDAL 3.6

.. supports_georeferencing::

.. supports_virtualio::

Spatial filtering
-----------------

Since GDAL 3.2, the driver can use the native .spx spatial indices for
spatial filtering.

In earlier versions, it uses the minimum bounding rectangle included
at the beginning of the geometry blobs to speed up spatial filtering. By
default, it also builds on the fly a in-memory spatial index during
the first sequential read of a layer. Following spatial filtering
operations on that layer will then benefit from that spatial index. The
building of this in-memory spatial index can be disabled by setting the
:decl_configoption:`OPENFILEGDB_IN_MEMORY_SPI` configuration option to NO.

SQL support
-----------

SQL statements are run through the OGR SQL engine. When attribute
indexes (.atx files) exist, the driver will use them to speed up WHERE
clauses or SetAttributeFilter() calls.

Special SQL requests
~~~~~~~~~~~~~~~~~~~~

"GetLayerDefinition a_layer_name" and "GetLayerMetadata a_layer_name"
can be used as special SQL requests to get respectively the definition
and metadata of a FileGDB table as XML content (only available in
Geodatabases created with ArcGIS 10 or above)

The "CREATE INDEX idx_name ON layer_name(field_name)" SQL request can be
used to create an attribute index. idx_name must have 16 characters or less,
start with a letter and contain only alpha-numeric characters or underscore.

The "RECOMPUTE EXTENT ON layer_name" SQL request can be used to trigger
an update of the layer extent in layer metadata. This is useful when updating
or deleting features that modify the general layer extent.

The "REPACK" or "REPACK layer_name" SQL requests can be used respectively to
compact the whole database or a given layer. This is useful when doing editions
(updates or feature deletions) that may leave holes in .gdbtable files. The REPACK
command causes the .gdbtable to be rewritten without holes. Note that compaction
does not involve extent recomputation.

Dataset open options
--------------------

-  **LIST_ALL_TABLES**\ =YES/NO: This may be "YES" to force all tables,
   including system and internal tables (such as the GDB_* tables) to be listed (since GDAL 3.4)

Dataset Creation Options
------------------------

None.

Layer Creation Options
----------------------

-  **FEATURE_DATASET**\=string: When this option is set, the new layer will be
   created inside the named FeatureDataset folder. If the folder does
   not already exist, it will be created.
-  **LAYER_ALIAS**\=string: Set layer name alias.
-  **GEOMETRY_NAME**\=string: Set name of geometry column in new layer. Defaults
   to "SHAPE".
-  **GEOMETRY_NULLABLE**\=YES/NO: Whether the values of the
   geometry column can be NULL. Can be set to NO so that geometry is
   required. Default to "YES"
-  **FID**: Name of the OID column to create. Defaults to "OBJECTID".
-  **XYTOLERANCE, ZTOLERANCE, MTOLERANCE**\=value: These parameters control the snapping
   tolerance used for advanced ArcGIS features like network and topology
   rules. They won't effect any OGR operations, but they will by used by
   ArcGIS. The units of the parameters are the units of the coordinate
   reference system.

   ArcMap 10.0 and OGR defaults for XYTOLERANCE are 0.001m (or
   equivalent) for projected coordinate systems, and 0.000000008983153°
   for geographic coordinate systems.
   ArcMap 10.0 and OGR defaults for ZTOLERANCE and MTOLERANCE are 0.0001.

-  **XORIGIN, YORIGIN, ZORIGIN, MORIGIN, XYSCALE, ZSCALE, ZORIGIN**\=value: These parameters
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

-  **COLUMN_TYPES**\=string. A list of strings of format field_name=fgdb_filed_type
   (separated by comma) to force the FileGDB column type of fields to be created.

-  **DOCUMENTATION**\=string. XML documentation for the layer.

-  **CONFIGURATION_KEYWORD**\=DEFAULTS/MAX_FILE_SIZE_4GB/MAX_FILE_SIZE_256TB:
   Customize how data is stored. By default text in UTF-8 and data up to 1TB

-  **CREATE_SHAPE_AREA_AND_LENGTH_FIELDS**\=YES/NO.
   Defaults to NO (through CreateLayer() API). When this option is set,
   a Shape_Area and Shape_Length special fields will be created for polygonal
   layers (Shape_Length only for linear layers). These fields will automatically
   be populated with the feature's area or length whenever a new feature is
   added to the dataset or an existing feature is amended.
   When using ogr2ogr with a source layer that has Shape_Area/Shape_Length
   special fields, and this option is not explicitly specified, it will be
   automatically set, so that the resulting FileGeodatabase has those fields
   properly tagged.

Field domains
-------------

.. versionadded:: 3.3

Coded and range field domains are supported.

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

The driver implements transactions at the database level,
through an emulation (as per :ref:`rfc-54`). This works by backing up
the current state of the modified parts of a geodatabase after
StartTransaction(force=TRUE) is called.
If the transaction is committed, the backup copy is destroyed.
If the transaction is rolled back, the backup copy is restored.

Note that this emulation has an unspecified behavior in case of
concurrent updates (with different connections in the same or another
process).

Comparison with the FileGDB driver
----------------------------------

(Comparison done with a FileGDB driver using FileGDB API SDK 1.4)

Advantages of the OpenFileGDB driver:

-  Can read ArcGIS 9.X Geodatabases, and not only 10 or above.
-  Can open layers with any spatial reference system.
-  Thread-safe (i.e. datasources can be processed in parallel).
-  Uses the VSI Virtual File API, enabling the user to read a
   Geodatabase in a ZIP file or stored on a HTTP server.
-  Faster on databases with a big number of fields.
-  Does not depend on a third-party library.
-  Robust against corrupted Geodatabase files.

Drawbacks of the OpenFileGDB driver:

-  Cannot read data from compressed data in CDF format (Compressed Data
   Format).

Examples
--------

-  Read layer from FileGDB and load into PostGIS:

   ::

      ogr2ogr -overwrite -f "PostgreSQL" PG:"host=myhost user=myuser dbname=mydb password=mypass" "C:\somefolder\BigFileGDB.gdb" "MyFeatureClass"

-  Get detailed info for FileGDB:

   ::

      ogrinfo -al "C:\somefolder\MyGDB.gdb"

-  Get detailed info for a zipped FileGDB:

   ::

      ogrinfo -al "C:\somefolder\MyGDB.gdb.zip"

Links
-----

-  :ref:`FileGDB driver <vector.filegdb>`, relying on the FileGDB API SDK
-  Reverse-engineered specification of the `FileGDB
   format <https://github.com/rouault/dump_gdbtable/wiki/FGDB-Spec>`__


Credits
-------

Edition/write capabilities of the driver have been funded by the following
organizations: Provincie Zuid-Holland, Provincie Gelderland and Gemeente Amsterdam.
