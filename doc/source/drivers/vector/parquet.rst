.. _vector.parquet:

(Geo)Parquet
============

.. versionadded:: 3.5

.. shortname:: Parquet

.. build_dependencies:: Parquet component of the Apache Arrow C++ library

From https://databricks.com/glossary/what-is-parquet:
"Apache Parquet is an open source, column-oriented data file format designed
for efficient data storage and retrieval. It provides efficient data compression
and encoding schemes with enhanced performance to handle complex data in bulk.
Apache Parquet is designed to be a common interchange format for both batch and interactive workloads."

This driver also supports geometry columns using the GeoParquet specification.

.. note:: The driver should be considered experimental as the GeoParquet specification is not finalized yet.

The GeoParquet 1.0.0-beta1 specification is supported since GDAL 3.6.2

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

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

Creation issues
---------------

The driver supports creating only a single layer in a dataset.

Layer creation options
----------------------

- .. lco:: COMPRESSION
     :choices: NONE, UNCOMPRESSED, SNAPPY, GZIP, BROTLI, ZSTD, LZ4_RAW, LZ4_HADOOP

      Compression method.
      Available values depend on how the Parquet library was compiled.
      Defaults to SNAPPY when available, otherwise NONE.

- .. lco:: GEOMETRY_ENCODING
     :choices: WKB, WKT, GEOARROW
     :default: WKB

     Geometry encoding.
     Other encodings (WKT and GEOARROW) are *not* allowed by the GeoParquet
     specification, but are handled as an extension, for symmetry with the Arrow
     driver.

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

SQL support
-----------

SQL statements are run through the OGR SQL engine. Statistics can be used to
speed-up evaluations of SQL requests like:
"SELECT MIN(colname), MAX(colname), COUNT(colname) FROM layername"

Dataset/partitioning read support
---------------------------------

Starting with GDAL 3.6.0, the driver can read directories that contain several
Parquet files, and expose them as a single layer. This support is only enabled
if the driver is built against the ``arrowdataset`` C++ library.

Note that no optimization is currently done regarding filtering.

Multithreading
--------------

Starting with GDAL 3.6.0, the driver will use up to 4 threads for reading (or the
maximum number of available CPUs returned by :cpp:func:`CPLGetNumCPUs()` if
it is lower by 4). This number can be configured with the configuration option
:config:`GDAL_NUM_THREADS`, which can be set to an integer value or
``ALL_CPUS``.

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


Links
-----

- `Apache Parquet home page <https://parquet.apache.org/>`__

- `Parquet file format <https://github.com/apache/parquet-format>`__

- `GeoParquet specification <https://github.com/opengeospatial/geoparquet>`__

- Related driver: :ref:`Arrow driver <vector.arrow>`
