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


Creation issues
---------------

The driver supports creating only a single layer in a dataset.

Layer creation options
----------------------

- **COMPRESSION=string**: Compression method. Can be one of ``NONE`` (or
  ``UNCOMPRESSED``), ``SNAPPY``, ``GZIP``, ``BROTLI``, ``ZSTD``, ``LZ4_RAW``,
  ``LZ4_HADOOP``. Available values depend on how the Parquet library was compiled.
  Defaults to SNAPPY when available, otherwise NONE.

- **GEOMETRY_ENCODING=WKB/WKT/GEOARROW**: Geometry encoding. Defaults to WKB.
  Other encodings (WKT and GEOARROW) are *not* allowed by the GeoParquet
  specification, but are handled as an extension, for symmetry with the Arrow
  driver.

- **ROW_GROUP_SIZE=integer**: Maximum number of rows per group. Default is 65536.

- **GEOMETRY_NAME=string**: Name of geometry column. Default is ``geometry``

- **FID=string**: Name of the FID (Feature Identifier) column to create. If
  none is specified, no FID column is created. Note that if using ogr2ogr with
  the Parquet driver as the target driver and a source layer that has a named
  FID column, this FID column name will be automatically used to set the FID
  layer creation option of the Parquet driver (unless ``-lco FID=`` is used to
  set an empty name)

- **POLYGON_ORIENTATION=COUNTERCLOCKWISE/UNMODIFIED**: Whether exterior rings
  of polygons should be counterclockwise oriented (and interior rings clockwise
  oriented), or left to their original orientation. The default is COUNTERCLOCKWISE.

- **EDGES=PLANAR/SPHERICAL**: How to interpret the edges of the geometries: whether
  the line between two points is a straight cartesian line (PLANAR) or the
  shortest line on the sphere (geodesic line) (SPHERICAL). The default is PLANAR.

- **CREATOR=string**: Name of creating application.

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
:decl_configoption:`GDAL_NUM_THREADS`, which can be set to an integer value or
``ALL_CPUS``.

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
