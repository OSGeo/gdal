.. _vector.tiledb:

================================================================================
TileDB -- TileDB vector
================================================================================

.. shortname:: TileDB

.. versionadded:: 3.7

.. build_dependencies:: TileDB >= 2.7

GDAL can read and write TileDB sparse arrays containing vector data.

The driver relies on the Open Source TileDB
`library <https://github.com/TileDB-Inc/TileDB>`__ (MIT licensed).

Driver capabilities
-------------------

.. supports_create::

.. supports_virtualio::

Supported datasets
------------------

The driver can read TileDB sparse arrays that contain at least 2 dimensions
of type Float64. By default dimensions names ``_X`` and ``_Y`` are looked up,
but this can be customized with the :oo:`DIM_X` and :oo:`DIM_Y` open options.

Attributes (or extra dimensions) of the following TileDB data types are
recognized as OGR fields: Bool, Int16, Int32, Int64, Float32, Float64,
String_ASCII, String_UTF8, Blob, varying size UInt8 (as Binary), DateTime_Day,
DateTime_MS, Time_MS. Attributes of other data types will be ignored.

If a ``wkb_geometry`` named attribute of type Binary is found, it is used as
the geometry column, and is assumed to contain WKB encoded geometries.
If there is no such column, Point geometries are assumed and the X, Y
(and optional Z) dimensions are used to create the corresponding OGR point
geometries.

Dataset connection string
-------------------------

Valid dataset connection strings are paths to a local or remote TileDB sparse
array, or to a local or remote TileDB group that contains TileDB sparse arrays.

Filtering
---------

The bounding box of OGR spatial filters is forwarded to the TileDB query engine.

OGR attribute filters are translated to TileDB query conditions for the following
elements:

- AND
- OR
- NOT
- comparisons of the form "attribute_name operator constant"
  where operator is ``=``, ``<>``, ``<``, ``<=``, ``>``, ``>=``
- attribute_name IS NULL
- attribute_name IS NOT NULL
- attribute_name IN (value1, ... valueN)
- attribute_name BETWEEN min_val AND max_val

Other OGR attribute filter elements may be used, but this may cause the filter
to be fully or partially evaluated on OGR side. For example, given the filter
"int_attribute = 5 AND string_attribute LIKE '%foo%'", the condition on
int_attribute will be translated as a TileDB query condition, and the right
side of the AND operation will be evaluated as a post-filter on OGR side.

Arrow C Stream data interface
-----------------------------

The driver has an efficient implementation of the
:ref:`Arrow C Stream data interface <vector_api_tut_arrow_stream>`

The ``INCLUDE_FID=YES/NO`` and ``MAX_FEATURES_IN_BATCH=number`` options of
:cpp:func:`OGRLayer::GetArrowStream()` are supported. If MAX_FEATURES_IN_BATCH
is not specified, it defaults to the value of the BATCH_SIZE open option.

Creation issues
---------------

By default, when creating a layer, the driver will create a TileDB sparse
array at the location of the dataset connection string. Consequently only one
OGR layer can be created in that mode. If several layers need to be created
and accessible through a OGR dataset connection, a TileDB group needs to be
created to point to the different arrays (layers), by specifying the
:dsco:`CREATE_GROUP=YES` dataset creation option.

The driver supports appending features to exiting layers.

The driver does not support adding new fields to a layer where features have
already been written.

Open options
------------

|about-open-options|
The following open options are available:

- .. oo:: TILEDB_CONFIG
     :choices: <filename>

     A local file with TileDB configuration
     `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

- .. oo:: TILEDB_TIMESTAMP
     :choices: <integer>

     Open array at this timestamp. The timestamp
     should be greater than 0.

- .. oo:: BATCH_SIZE
     :choices: <integer>
     :default: 500000

     Number of features to fetch/write at once.

- .. oo:: DIM_X
     :default: _X

     Name of the X dimension.

- .. oo:: DIM_Y
     :default: _Y

     Name of the Y dimension.

- .. oo:: DIM_Z
     :default: _Z

     Name of the Z dimension.

Dataset creation options
------------------------

|about-dataset-creation-options|
The following dataset creation options are available:

- .. dsco:: TILEDB_CONFIG
     :choices: <filename>

     A local file with TileDB configuration
     `options <https://docs.tiledb.io/en/stable/tutorials/config.html>`__

- .. dsco:: CREATE_GROUP
     :choices: YES, NO
     :default: NO

     (TileDB >= 2.9) Whether to create a group for
     multiple layer support.
     When set to YES, a TileDB group will be created in
     the directory of the dataset name, and layers will be created as members of the
     group, and written in subdirectories of a ``layers`` subdirectory.

Layer creation options
----------------------

|about-layer-creation-options|
The following layer creation options are available:

- .. lco:: COMPRESSION
     :choices: NONE, GZIP, ZSTD, LZ4, RLE, BZIP2, DOUBLE-DELTA, POSITIVE_DELTA
     :default: NONE

     compression method for dimensions and attributes.

- .. lco:: COMPRESSION_LEVEL
     :choices: <integer>

     compression level

- .. lco:: BATCH_SIZE
     :choices: <integer>
     :default: 500000

     Number of features to write at once.

- .. lco:: TILE_CAPACITY
     :choices: <integer>
     :default: 10000

     Number of non-empty cells stored in a data tile.

- .. lco:: TILE_EXTENT
     :choices: <float>

     The square TileDB tile extents in the X and Y dimensions. Default is auto-calculated.

- .. lco:: TILE_Z_EXTENT
     :choices: <float>

     The TIleDB tile extent in the Z dimension. Default is auto-calculated.

- .. lco:: BOUNDS
     :choices: <minx\,miny\,[minz\,]maxx\,maxy[\, maxz]>

     Specify bounds for sparse array.
     If not specified, the CRS passed at layer creation will be used to infer
     default values for bounds.

- .. lco:: ADD_Z_DIM
     :choices: AUTO, YES, NO
     :default: AUTO

     Whether to add a Z dimension. In the default AUTO
     mode, a Z dimension is only added if the layer geometry type has a Z component
     or is unknown. Setting it to YES or NO explicitly force or disable creation of
     a Z dimension.

- .. lco:: FID
     :default: FID

     Feature id column name. Set to empty to disable its creation.

- .. lco:: GEOMETRY_NAME
     :default: wkb_geometry

     Name of the geometry column that will receive WKB
     encoded geometries. Set to empty to disable its creation (only for point).

- .. lco:: TILEDB_TIMESTAMP
     :choices: <integer>

     Timestamp at which to create the array.
     The timestamp should be greater than 0.

- .. lco:: TILEDB_STRING_TYPE
     :choices: UTF8, ASCII

     Which TileDB type to create string attributes.
     Default is UTF8 starting with TileDB 2.14 (ASCII for earlier versions)

See Also
--------

- `TileDB home page <https://tiledb.io/>`__

- :ref:`TileDB raster <raster.tiledb>` documentation page
