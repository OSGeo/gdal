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

- **COMPRESSION=string**: Compression method. Can be one of ``NONE``, ``SNAPPY``,
  ``GZIP``, ``BROTLI``, ``ZSTD``, ``LZ4``, ``BZ2``, ``LZ4_HADOOP``. Available
  values depend on how the Parquet library was compiled.
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

Links
-----

- `Apache Parquet home page <https://parquet.apache.org/>`__

- `Parquet file format <https://github.com/apache/parquet-format>`__

- `GeoParquet specification <https://github.com/opengeospatial/geoparquet>`__

- Related driver: :ref:`Arrow driver <vector.arrow>`
