.. _vector.parquet:

(Geo)Parquet
============

.. versionadded:: 3.5

.. shortname:: Parquet

.. build_dependencies:: Parquet component of the Apache Arrow C++ library

From https://parquet.apache.org/:
"Apache Parquet is an open source, column-oriented data file format designed for efficient data storage and retrieval.
It provides efficient data compression and encoding schemes with enhanced performance to handle complex data in bulk.
Parquet is available in multiple languages including Java, C++, Python, etc..."

This driver also supports geometry columns using the GeoParquet specification.

The GeoParquet 1.0.0 specification is supported since GDAL 3.8.0.
The GeoParquet 1.1.0 specification is supported since GDAL 3.9.0.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

|about-open-options|
The following open options are supported:

- .. oo:: GEOM_POSSIBLE_NAMES
     :since: 3.8
     :default: geometry,wkb_geometry,wkt_geometry

     Comma separated list of possible names for geometry column(s). Only used
     for files without GeoParquet dataset-level metadata.
     Columns are recognized as geometry
     columns only if they are of type binary (they are assumed to contain
     WKB encoded geometries), or if they are of type string and contain the
     "wkt" substring in their name (they are then assumed to contain WKT encoded
     geometries).

- .. oo:: CRS
     :since: 3.8

     To set or override the CRS of geometry columns.
     The string is typically formatted as CODE:AUTH (e.g "EPSG:4326"), or can
     be a PROJ.4 or WKT CRS string.

- .. oo:: LISTS_AS_STRING_JSON
     :choices: YES, NO
     :default: NO
     :since: 3.12.1

     Whether lists of strings/integers/reals should be reported as String(JSON)
     fields rather than String/Integer[64]/RealList.
     Useful when null values in such lists must be exactly mapped as such,
     instead of being omitted (for lists of strings), or set to 0 (for list of
     boolean, integer or real).

Creation issues
---------------

The driver supports creating only a single layer in a dataset.

Layer creation options
----------------------

|about-layer-creation-options|
The following layer creation options are supported:

- .. lco:: COMPRESSION
     :choices: NONE, UNCOMPRESSED, SNAPPY, GZIP, BROTLI, ZSTD, LZ4_RAW, LZ4_HADOOP

      Compression method.
      Available values depend on how the Parquet library was compiled.
      Defaults to SNAPPY when available, otherwise NONE.

- .. lco:: COMPRESSION_LEVEL
     :choices: <integer>
     :since: 3.12

     Specify the compression level for the selected compression method. The
     compression level has a different meaning for each codec. The description
     of this option, returned at runtime for example by ``ogrinfo --format PARQUET``,
     gives the range and default value for each codec.

- .. lco:: GEOMETRY_ENCODING
     :choices: WKB, WKT, GEOARROW, GEOARROW_INTERLEAVED
     :default: WKB

     Geometry encoding.
     WKB is the default and recommended choice for maximal interoperability.
     WKT is *not* allowed by the GeoParquet specification, but are handled as
     an extension.
     As of GDAL 3.9, GEOARROW uses the GeoParquet 1.1 GeoArrow "struct" based
     encodings (where points are modeled as a struct field with a x and y subfield,
     lines are modeled as a list of such points, etc.).
     The GEOARROW_INTERLEAVED option has been renamed in GDAL 3.9 from what was
     named GEOARROW in previous versions, and uses an encoding where points uses
     a FixedSizedList of (x,y), lines a variable-size list of such
     FixedSizedList of points, etc. This GEOARROW_INTERLEAVED encoding is not
     part of the official GeoParquet specification, and its use is not encouraged.

- .. lco:: ROW_GROUP_SIZE
     :choices: <integer>
     :default: 65536

     Maximum number of rows per group.

- .. lco:: GEOMETRY_NAME
     :default: geometry

     Name of geometry column.

- .. lco:: FID

     Name of the FID (Feature Identifier) column to create. If
     none is specified, no FID column is created. Note that if using ogr2ogr with
     the Parquet driver as the target driver and a source layer that has a named
     FID column, this FID column name will be automatically used to set the FID
     layer creation option of the Parquet driver (unless ``-lco FID=`` is used to
     set an empty name)

- .. lco:: POLYGON_ORIENTATION
     :choices: COUNTERCLOCKWISE, UNMODIFIED
     :default: COUNTERCLOCKWISE

     Whether exterior rings
     of polygons should be counterclockwise oriented (and interior rings clockwise
     oriented), or left to their original orientation.

- .. lco:: EDGES
     :choices: PLANAR, SPHERICAL
     :default: PLANAR

     How to interpret the edges of the geometries: whether
     the line between two points is a straight cartesian line (PLANAR) or the
     shortest line on the sphere (geodesic line) (SPHERICAL).

- .. lco:: CREATOR

     Name of creating application.

- .. lco:: WRITE_COVERING_BBOX
     :choices: AUTO, YES, NO
     :default: AUTO
     :since: 3.9

     Whether to write xmin/ymin/xmax/ymax columns with the bounding box of
     geometries. Writing the geometry bounding box may help applications to
     perform faster spatial filtering. Writing a geometry bounding box is less
     necessary for the GeoArrow geometry encoding than for the default WKB, as
     implementations may be able to directly use the geometry columns.

     If the :lco:`USE_PARQUET_GEO_TYPES` layer creation option is set to ``ONLY``,
     and :lco:`WRITE_COVERING_BBOX` is set or let to its default ``AUTO`` value,
     no covering bounding box columns is written.

- .. lco:: COVERING_BBOX_NAME
     :choices: <string>
     :since: 3.13

     Name of the bounding box of geometries Only used if :lco:`WRITE_COVERING_BBOX` is set.
     If not set, it defaults to the geometry column name, suffixed with ``_bbox``.

- .. lco:: USE_PARQUET_GEO_TYPES
     :choices: YES, NO, ONLY
     :default: NO
     :since: 3.12

     Only available with libarrow >= 21.

     Whether to use Parquet Geometry/Geography logical types (introduced in libarrow 21),
     when using the default GEOMETRY_ENCODING=WKB encoding.

     - ``YES``: use the Geometry logical type (or the Geography
       one if the EDGES=SPHERICAL creation option is also set), and also
       write file-level GeoParquet metadata. Such files can be read by older
       GDAL, but require libarrow >= 20.

     - ``NO`` (default): only file-level GeoParquet metadata is written. Such
       files can be read by older GDAL and libarrow versions.

     - ``ONLY``: use the Geometry logical type (or the Geography
       one if the EDGES=SPHERICAL creation option is also set), but do not
       write file-level GeoParquet metadata. Such files will only be fully compatible
       of GDAL >= 3.12 and libarrow >= 21.
       With libarrow 20, the geometry column of such files
       will only be recognized if it is among one of the GEOM_POSSIBLE_NAMES open
       option value, and the CRS of such files will not be recognized.
       With older libarrow, such files cannot be opened at all.

- .. lco:: SORT_BY_BBOX
     :choices: YES, NO
     :default: NO
     :since: 3.9

     Whether features should be sorted based on the bounding box of their
     geometries, before being written in the final file. Sorting them enables
     faster spatial filtering on reading, by grouping together spatially close
     features in the same group of rows.

     Note however that enabling this option involves creating a temporary
     GeoPackage file (in the same directory as the final Parquet file),
     and thus requires temporary storage (possibly up to several times the size
     of the final Parquet file, depending on Parquet compression) and additional
     processing time.

     The efficiency of spatial filtering depends on the ROW_GROUP_SIZE. If it
     is too large, too many features that are not spatially close will be grouped
     together. If it is too small, the file size will increase, and extra
     processing time will be necessary to browse through the row groups.

     Note also that when this option is enabled, the Arrow writing API (which
     is for example triggered when using ogr2ogr to convert from Parquet to Parquet),
     fallbacks to the generic implementation, which does not support advanced
     Arrow types (lists, maps, etc.).

- .. lco:: TIMESTAMP_WITH_OFFSET
     :choices: AUTO, YES, NO
     :default: AUTO
     :since: 3.13

     Whether OGR datetime fields should be written as Arrow timestamp with offset fields, following the
     `Timestamp With Offset extension <https://github.com/apache/arrow/blob/main/docs/source/format/CanonicalExtensions.rst#timestamp-with-offset>`__ specification.
     Such fields store both the datetime as a timestamp expressed in the UTC timezone and the
     offset to UTC of the timezone in which the datetime is defined.
     In AUTO mode, they are used as soon as a DateTime field reports a mixed
     time zone flag (i.e. :cpp:func:`OGRFieldDefn::GetTZFlag` returns ``OGR_TZFLAG_MIXED_TZ``).
     As few drivers are able to automatically set this flag, it may be useful
     to override the flag by setting this option to YES. Setting it to NO forces the use
     of a DateTime field with the UTC timezone.

SQL support
-----------

SQL statements are run through the OGR SQL engine. Statistics can be used to
speed-up evaluations of SQL requests like:
"SELECT MIN(colname), MAX(colname), COUNT(colname) FROM layername"

.. _target_drivers_vector_parqquet_dataset_partitioning:

Dataset/partitioning read support
---------------------------------

Starting with GDAL 3.6.0, the driver can read directories that contain several
Parquet files, and expose them as a single layer. This support is only enabled
if the driver is built against the ``arrowdataset`` C++ library.

It is also possible to force opening single Parquet file in that mode by prefixing
their filename with ``PARQUET:``.

Optimized spatial and attribute filtering for Arrow datasets is available since
GDAL 3.10.

Metadata
--------

.. versionadded:: 3.9.0

Layer metadata can be read and written. It is serialized as JSON content in a
``gdal:metadata`` domain.

Multithreading
--------------

Starting with GDAL 3.6.0, the driver will use up to 4 threads for reading (or the
maximum number of available CPUs returned by :cpp:func:`CPLGetNumCPUs()` if
it is lower by 4). This number can be configured with the configuration option
:config:`GDAL_NUM_THREADS`, which can be set to an integer value or
``ALL_CPUS``.

Update support
--------------

.. versionadded:: 3.12.0

The driver supports adding, updating, removing features and fields. Note that
this is accomplished by rewriting the whole file during :cpp:func:`GDALDataset::FlushCache`
or when closing the dataset, and keeping into memory all modifications between
two flushes.

If the file to be updated has been created by GDAL >= 3.12, the creation options
to create the original file will be re-applied when creating the updated file.
Stability of feature ids is only guaranteed if the original file has been
created with an explicit ``FID`` creation option.

If the file to be updated has been created outside of GDAL, the original
non-scalar/nested field types will not always be preserved, but a JSON
representation may be used instead.

Validation script
-----------------

The :source_file:`swig/python/gdal-utils/osgeo_utils/samples/validate_geoparquet.py`
Python script can be used to check compliance of a Parquet file against the
GeoParquet specification.

To validate only metadata:

::

    python3 validate_geoparquet.py my_geo.parquet


To validate metadata and check content of geometry column(s):

::

    python3 validate_geoparquet.py --check-data my_geo.parquet


Conda-forge package
-------------------

The driver can be installed as a plugin for the ``libgdal`` conda-forge package with:

::

    conda install -c conda-forge libgdal-arrow-parquet

Standalone plugin compilation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. versionadded:: 3.10

While this driver may be built as part of a whole GDAL build, either in libgdal
itself, or as a plugin, it is also possible to only build this driver as a plugin,
against an already built libgdal.

The version of the GDAL sources used to build the driver must match the version
of the libgdal it is built against.

For example, from a "build_parquet" directory under the root of the GDAL source tree:

::

    cmake -S ../ogr/ogrsf_frmts/parquet -DCMAKE_PREFIX_PATH=/path/to/GDAL_installation_prefix -DArrow_DIR=/path/to/lib/cmake/Arrow -DParquet_DIR=/path/to/lib/cmake/Parquet
    cmake --build .


Note that such a plugin, when used against a libgdal not aware of it, will be
systematically loaded at GDAL driver initialization time, and will not benefit from
`deferred plugin loading capabilities <rfc-96>`. For that, libgdal itself must be built with the
CMake variable OGR_REGISTER_DRIVER_PARQUET_FOR_LATER_PLUGIN=ON set.

Links
-----

- `Apache Parquet home page <https://parquet.apache.org/>`__

- `Parquet file format <https://github.com/apache/parquet-format>`__

- `GeoParquet specification <https://github.com/opengeospatial/geoparquet>`__

- Related driver: :ref:`Arrow driver <vector.arrow>`
