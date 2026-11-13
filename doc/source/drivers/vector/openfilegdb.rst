.. _vector.openfilegdb:

ESRI File Geodatabase vector (OpenFileGDB)
==========================================

.. shortname:: OpenFileGDB

.. built_in_by_default::

The OpenFileGDB driver provides read, write and update access to vector layers of File
Geodatabases (.gdb directories) created by ArcGIS 10 and above (it has also
read-only support for ArcGIS 9.x geodatabases). The
dataset name must be the directory/folder name, and it must end with the
.gdb extension.

It can also read directly zipped .gdb directories (with .gdb.zip
extension), provided they contain a .gdb directory at their first level.

A specific .gdbtable file (including "system" tables) can also be opened
directly.

Curve geometries are supported.

Write and update capabilities are supported since GDAL 3.6.

The driver also supports :ref:`raster layers<raster.openfilegdb>` since GDAL 3.7.

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

When no .spx file is available, or its use is disabled with the
:config:`OPENFILEGDB_USE_SPATIAL_INDEX` configuration option, the driver
builds an in-memory spatial index on the fly during the first complete
sequential read of a layer, and later spatial filters on that layer use it
until the dataset is closed.

The building of this in-memory spatial index can be disabled by setting
the :config:`OPENFILEGDB_IN_MEMORY_SPI` configuration option to ``NO``.

SQL support
-----------

SQL statements are run through the OGR SQL engine. When attribute
indexes (.atx files) exist, the driver will use them to speed up WHERE
clauses or SetAttributeFilter() calls.

This can be disabled with the :config:`OPENFILEGDB_USE_INDEX` configuration option.

Special SQL requests
~~~~~~~~~~~~~~~~~~~~

"GetLayerDefinition a_layer_name" and "GetLayerMetadata a_layer_name"
can be used as special SQL requests to get respectively the definition
and metadata of a FileGDB table as XML content (only available in
Geodatabases created with ArcGIS 10 or above).

The "CREATE INDEX idx_name ON layer_name(field_name)" SQL request can be
used to create an attribute index. idx_name must have 16 characters or less,
start with a letter and contain only alphanumeric characters or underscore.
Multiple column indices are not supported.

The "RECOMPUTE EXTENT ON layer_name" SQL request can be used to trigger
an update of the layer extent in layer metadata. This is useful when updating
or deleting features that modify the general layer extent.

The "REPACK" or "REPACK layer_name" SQL requests can be used respectively to
compact the whole database or a given layer. This is useful when doing edits
(updates or feature deletions) that may leave holes in .gdbtable files. The REPACK
command causes the .gdbtable to be rewritten without holes. Note that compaction
does not involve extent recomputation.

.. warning::

   REPACK removes the attribute (.atx) and spatial (.spx) index
   files of the tables it rewrites, without recreating them.

Configuration options
---------------------

|about-config-options|
The following configuration options are available:

-  .. config:: OPENFILEGDB_USE_SPATIAL_INDEX
      :choices: YES, NO
      :default: YES

      Whether to use the native .spx spatial index for spatial filtering.
      If ``NO``, the in-memory spatial index is used instead, unless
      :config:`OPENFILEGDB_IN_MEMORY_SPI` is set to ``NO``.
      See `Spatial filtering`_.

-  .. config:: OPENFILEGDB_IN_MEMORY_SPI
      :choices: YES, NO
      :default: YES

      If ``YES`` (the default), an in-memory spatial index is built
      when the native .spx spatial index is not available, or is disabled with
      :config:`OPENFILEGDB_USE_SPATIAL_INDEX` set to ``NO``. See `Spatial filtering`_.

-  .. config:: OPENFILEGDB_USE_INDEX
      :choices: YES, NO
      :default: YES

      Whether to use attribute indexes (.atx files) to speed up attribute
      filters, WHERE clauses, and MIN, MAX, SUM, AVG and COUNT in OGR SQL
      SELECT statements. If ``NO``, all features are read instead.


-  .. config:: OPENFILEGDB_DEFAULT_STRING_WIDTH
      :choices: <integer>

      Width of string fields to use on creation, when the width specified to
      CreateField() is the unspecified value 0. This defaults to 65536.


Dataset open options
--------------------

|about-open-options|
The following open options are supported:

-  .. oo:: LIST_ALL_TABLES
      :choices: YES, NO
      :since: 3.4

      This may be "YES" to force all tables,
      including system and internal tables (such as the GDB_* tables) to be listed.
      These are marked as [private] in the layer list. By default, only user tables are listed.

Dataset Creation Options
------------------------

None.

Layer Creation Options
----------------------

|about-layer-creation-options|
The following layer creation options are supported:

-  .. lco:: TARGET_ARCGIS_VERSION
      :choices: ALL, ARCGIS_PRO_3_2_OR_LATER
      :default: ALL
      :since: 3.9

      ArcGIS version that the dataset must be compatible with.
      If creation of Integer64, Date, Time field types is needed,
      ``ARCGIS_PRO_3_2_OR_LATER`` must be selected.
      If set or left to the default value ``ALL``, those types will
      respectively be encoded to fallback types (Real instead of Integer64,
      DateTime instead of Date or Time).

-  .. lco:: FEATURE_DATASET
      :choices: <string>

      When this option is set, the new layer will be
      created inside the named FeatureDataset folder. If the folder does
      not already exist, it will be created.

-  .. lco:: LAYER_ALIAS

      Set layer name alias.

      On reading, this information is exposed in the ALIAS_NAME layer metadata
      item (GDAL >= 3.8).

-  .. lco:: GEOMETRY_NAME
      :default: SHAPE

      Set name of geometry column in new layer.

-  .. lco:: GEOMETRY_NULLABLE
      :choices: YES, NO
      :default: YES

      Whether the values of the
      geometry column can be NULL. Can be set to NO so that geometry is
      required.

-  .. lco:: FID
      :default: OBJECTID

      Name of the OID column to create.

-  **XYTOLERANCE, ZTOLERANCE, MTOLERANCE**\=value: These parameters control the snapping
   tolerance used for advanced ArcGIS features like network and topology
   rules. They won't affect any OGR operations, but they will be used by
   ArcGIS. The units of the parameters are the units of the coordinate
   reference system.

   ArcMap 10.0 and OGR defaults for XYTOLERANCE are 0.001m (or
   equivalent) for projected coordinate systems, and 0.000000008983153°
   for geographic coordinate systems.
   ArcMap 10.0 and OGR defaults for ZTOLERANCE and MTOLERANCE are 0.0001.

-  **XORIGIN, YORIGIN, ZORIGIN, MORIGIN, XYSCALE, ZSCALE, MSCALE**\=value: These parameters
   control the `coordinate precision
   grid <https://help.arcgis.com/en/sdk/10.0/java_ao_adf/conceptualhelp/engine/index.html#//00010000037m000000>`__
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

-  .. lco:: COLUMN_TYPES

      A list of strings of format field_name=fgdb_field_type
      (separated by comma) to force the FileGDB column type of fields to be created.

-  .. lco:: DOCUMENTATION
      :choices: <string>

      XML documentation for the layer.

-  .. lco:: CONFIGURATION_KEYWORD
      :choices: DEFAULTS, MAX_FILE_SIZE_4GB, MAX_FILE_SIZE_256TB

      Customize how data is stored. By default text in UTF-8 and data up to 1TB.

-  .. lco:: CREATE_SHAPE_AREA_AND_LENGTH_FIELDS
      :choices: YES, NO
      :default: NO

      When this option is set,
      Shape_Area and Shape_Length special fields will be created for polygonal
      layers (Shape_Length only for linear layers). These fields will automatically
      be populated with the feature's area or length whenever a new feature is
      added to the dataset or an existing feature is amended.
      When using ogr2ogr with a source layer that has Shape_Area/Shape_Length
      special fields, and this option is not explicitly specified, it will be
      automatically set, so that the resulting FileGeodatabase has those fields
      properly tagged.

64-bit integer field support
----------------------------

.. versionadded:: 3.9

On creation, 64-bit integer field support requires setting the :lco:`TARGET_ARCGIS_VERSION`
layer creation option to ``ARCGIS_PRO_3_2_OR_LATER``.
Note that Esri `recommends <https://doc.esri.com/en/arcgis-pro/latest/help/data/geodatabases/overview/arcgis-field-data-types.html#ESRI_SECTION2_8BF2454C879941258DC44AF6BB31F386>`__ to restrict the
range of 64-bit integer values to [-9007199254740991, 9007199254740991] for
wider compatibility. GDAL will allow writing values outside of that range without
warning, and can also read them fine.

Field domains
-------------

.. versionadded:: 3.3

Coded and range field domains are supported.

Relationships
-------------

.. versionadded:: 3.6

Relationship retrieval, creation, deletion and updating is supported.

Hierarchical organization
-------------------------

.. versionadded:: 3.4

The hierarchical organization of tables and feature classes as top-level
elements or within feature datasets can be explored using the methods
:cpp:func:`GDALDataset::GetRootGroup`,
:cpp:func:`GDALGroup::GetGroupNames`, :cpp:func:`GDALGroup::OpenGroup`,
:cpp:func:`GDALGroup::GetVectorLayerNames` and :cpp:func:`GDALGroup::OpenVectorLayer`.

Transaction support
-------------------

The driver implements transactions at the database level,
through emulation (as per :ref:`rfc-54`). This works by backing up
the current state of the modified parts of a geodatabase after
StartTransaction(force=TRUE) is called.
If the transaction is committed, the backup copy is destroyed.
If the transaction is rolled back, the backup copy is restored.

Note that this emulation has an unspecified behavior in case of
concurrent updates (with different connections in the same or another
process).

Geometry coordinate precision
-----------------------------

.. versionadded:: GDAL 3.9

The driver supports reading and writing the geometry coordinate
precision, using the XYResolution, ZResolution and MResolution members of
the :cpp:class:`OGRGeomCoordinatePrecision` settings of the
:cpp:class:`OGRGeomFieldDefn`. ``XYScale`` is computed as 1.0 / ``XYResolution``
(and similarly for the Z and M components). The tolerance setting is computed
as being one tenth of the resolution.

On reading, the coordinate precision grid parameters are returned as format
specific options of :cpp:class:`OGRGeomCoordinatePrecision` with the
``FileGeodatabase`` format key, with the following option key names:
``XYScale``, ``XYTolerance``, ``XOrigin``, ``YOrigin``,
``ZScale``, ``ZTolerance``, ``ZOrigin``,
``MScale``, ``MTolerance``, ``MOrigin``. On writing, they are also honored
(they will have precedence over XYResolution, ZResolution and MResolution).

On layer creation, the XORIGIN, YORIGIN, ZORIGIN, MORIGIN, XYSCALE, ZSCALE,
MSCALE, XYTOLERANCE, ZTOLERANCE, MTOLERANCE layer creation options will be
used in priority over the settings of :cpp:class:`OGRGeomCoordinatePrecision`.

Advantages of the OpenFileGDB driver, compared to the FileGDB driver
--------------------------------------------------------------------

-  Can read ArcGIS 9.X Geodatabases, and not only 10 or above.

-  Can open layers with any spatial reference system.

-  Thread-safe (i.e. datasources can be processed in parallel).

-  Uses the VSI Virtual File API, enabling the user to read a
   Geodatabase in a ZIP file or stored on a HTTP server.

-  Faster on databases with a large number of fields.

-  Does not depend on a third-party library.

-  Robust against corrupted Geodatabase files.

Limitations
-----------

-  Reading data compressed in SDC format (Smart Data Compression) or in
   CDF format (Compressed Data Format) is not supported. For CDF,
   the :ref:`FileGDB driver <vector.filegdb>` can be used.

-  Support for tables with 64-bit OBJECTIDs (which require GDAL >= 3.10), where
   OBJECTIDs are sparse, is read-only and incomplete.
   The driver will emit a warning if such situation occurs, and will assign
   non-faithful OBJECTIDs.

Examples
--------

.. example::
   :title: Reading a layer from FileGDB and loading into PostGIS

   .. code-block:: bash

      ogr2ogr -overwrite -f "PostgreSQL" PG:"host=myhost user=myuser dbname=mydb password=mypass" "C:\somefolder\BigFileGDB.gdb" "MyFeatureClass"

.. example::
   :title: Getting detailed info for FileGDB

   .. code-block:: bash

      ogrinfo -al "C:\somefolder\MyGDB.gdb"

.. example::
   :title: Getting detailed info for a zipped FileGDB

   .. code-block:: bash

      ogrinfo -al "C:\somefolder\MyGDB.gdb.zip"

The following examples use datasets from the ``autotest/ogr/data/filegdb``
folder of the GDAL source tree.

.. example::
   :title: Listing the layers of a FileGDB with the gdal CLI

   .. code-block:: bash

      gdal vector info --summary testopenfilegdb.gdb.zip
      # and with all tables
      gdal vector info --summary --oo LIST_ALL_TABLES=YES testopenfilegdb.gdb.zip

.. example::
   :title: Reading a zipped FileGDB directly from a web server

   .. code-block:: bash

      gdal vector info --summary /vsizip//vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/ogr/data/filegdb/test_spatial_index.gdb.zip

.. example::
   :title: Converting a layer from a zipped FileGDB to GeoPackage, with an attribute filter

   .. code-block:: bash

      gdal vector pipeline read test_spatial_index.gdb.zip --layer test ! filter --where "id = 1" ! write out.gpkg

.. example::
   :title: Creating a FileGDB for ArcGIS Pro 3.2 or later, and listing its field types

   .. code-block:: bash

      gdal vector convert arcgis_pro_32_types.gdb out.gdb --of OpenFileGDB --lco TARGET_ARCGIS_VERSION=ARCGIS_PRO_3_2_OR_LATER
      gdal vector info --format json out.gdb | jq -r '.layers[] | .name as $layer | .fields[] | [$layer, .name, .type] | @tsv'

   Extract of the output. Without ``TARGET_ARCGIS_VERSION``, these types would
   be written as Date → DateTime, Time → DateTime, and Integer64 → Real:

   .. code-block:: text

      date_types  date_only  Date
      date_types  time_only  Time
      big_int     big        Integer64

.. example::
   :title: Creating an attribute index, and checking it is used

   .. code-block:: bash

      # Make a writable copy (zipped geodatabases are read-only)
      gdal vector convert test_spatial_index.gdb.zip work.gdb --of OpenFileGDB
      # Create the index
      gdal vector sql --update --sql "CREATE INDEX idx_id ON test(id)" work.gdb
      # Check that the index is used
      gdal vector info work.gdb --layer test --where "id = 1" --features --config CPL_DEBUG=ON
      # INFO: Open of `work.gdb'
      #       using driver `OpenFileGDB' successful.
      # OpenFileGDB: Using index on field id (= 1)

Links
-----

-  :ref:`OpenFileGDB raster <raster.openfilegdb>` documentation page
-  :ref:`FileGDB driver <vector.filegdb>`, relying on the FileGDB API SDK
-  Reverse-engineered specification of the `FileGDB
   format <https://github.com/rouault/dump_gdbtable/wiki/FGDB-Spec>`__


Credits
-------

Edit/write capabilities of the driver have been funded by the following
organizations: Provincie Zuid-Holland, Provincie Gelderland and Gemeente Amsterdam.
