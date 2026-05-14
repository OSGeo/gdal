.. _vector.adbc:

ADBC -- Arrow Database Connectivity
===================================

.. versionadded:: 3.11

.. shortname:: ADBC

.. build_dependencies:: adbc-driver-manager

ADBC is a set of APIs and libraries for Arrow-native access to database.

This driver has 2 modes:

- either it has been built against the ``adbc-driver-manager`` library. In that
  case, it can directly be used to connect to available ADBC drivers, and expose
  content as classic OGR features, or as a ArrowArrayStream.
  In that mode the driver metadata exposes the ``HAS_ADBC_DRIVER_MANAGER``
  metadata item.
- or it has not, in which case applications embedding GDAL must use
  :cpp:func:`GDALSetAdbcLoadDriverOverride` as detailed in a below paragraph.
  Note that use of that function can also be done even if the driver has been built
  against the ``adbc-driver-manager`` library.

Consult the `installation instruction <https://arrow.apache.org/adbc/current/driver/installation.html>`__
for the various ADBC drivers. At time of writing, there are drivers for
SQLite3, PostgreSQL, Snowflake, BigQuery, DuckDB, Flight SQL, etc.

The driver is read-only.

There is spatial support when the underlying ADBC driver is DuckDB, for
native spatial DuckDB databases and GeoParquet datasets, and when the spatial
extension is installed.

Google BigQuery support
-----------------------

Consult the :ref:`vector.adbc_bigquery` page.

Connection string
-----------------

Several connection strings are supported:

- ``ADBC:{some_uri}``, together with the ``ADBC_DRIVER`` open option.
- a SQLite3 database filename, if the ``adbc_driver_sqlite`` is available.
- a DuckDB database filename, if the :file:`libduckdb.so`, :file:`libduckdb.dylib`
  or :file:`duckdb.dll` is available (and it is in a system location, or can be
  located through LD_LIBRARY_PATH on Linux, DYLD_LIBRARY_PATH on MacOSX or PATH on Windows).
- a Parquet database filename, if the :file:`libduckdb.so`, :file:`libduckdb.dylib`
  or :file:`duckdb.dll` is available (and it is in a system location, or can be
  located through LD_LIBRARY_PATH on Linux, DYLD_LIBRARY_PATH on MacOSX or PATH on Windows).
- a PostgreSQL URI starting with ``postgresql://``, if the ``adbc_driver_postgresql`` is available.

Note: if present, the :ref:`vector.sqlite`, :ref:`vector.gpkg` or
:ref:`vector.parquet` drivers are registered before the ADBC driver, so they will
be used in priority when available. The ``ADBC:`` prefix or the ``-if ADBC``
switch of :program:`ogrinfo` or :program:`ogr2ogr` can be used to use the ADBC
driver instead.

Dataset open options
--------------------

|about-open-options|
The following open options are supported:

-  .. oo:: ADBC_DRIVER
      :choices: <string>

      ADBC driver name. Examples: ``adbc_driver_sqlite``, ``adbc_driver_postgresql``,
      ``adbc_driver_bigquery``, ``adbc_driver_snowflake`` or a path to the
      DuckDB shared library.

- .. oo:: SQL
      :choices: <string>

      A SQL-like statement recognized by the driver, used to create a result
      layer from the dataset.

- .. oo:: ADBC_OPTION_xxx
      :choices: <string>

      Custom ADBC option to pass to AdbcDatabaseSetOption(). Options are
      driver specific.
      For example ``ADBC_OPTION_uri=some_value`` to pass the ``uri`` option.

- .. oo:: PRELUDE_STATEMENTS
      :choices: <string>

      SQL-like statement recognized by the driver that must be executed before
      discovering layers. Can be repeated multiple times.
      For example ``PRELUDE_STATEMENTS=INSTALL spatial`` and
      ``PRELUDE_STATEMENTS=LOAD spatial`` to load DuckDB spatial extension.

"table_list" special layer
--------------------------

For PostgreSQL, SQLite3, DuckDB and Parquet datasets, the driver automatically
instantiates OGR layers from available tables.
For other databases, the user must explicit provide a SQL open option or issue
a :cpp:func:`GDALDataset::ExecuteSQL` request.
To facilitate that process, a special OGR ``table_list`` layer can be queried
through :cpp:func:`GDALDataset::GetLayerByName` (or as the layer name with
:program:`ogrinfo`).
It returns for each table a OGR feature with the following fields (some
potentially unset or with an empty string): ``catalog_name``, ``schema_name``,
``table_name``, ``table_type``.

Custom driver entry point
-------------------------

A custom driver entry point can be specified by applications by calling
:cpp:func:`GDALSetAdbcLoadDriverOverride` (defined in header :file:`gdal_adbc.h`)
before using the driver. The specified init function will be used by the
GDAL ADBC driver as a way of locating and loading the ADBC driver if GDAL was
not built with ADBC Driver Manager support or if an embedding application has
an updated or augmented collection of drivers available.

Filtering
---------

Attribute filters are passed to the underlying ADBC engine.

Spatial filters are passed to DuckDB when it is the underlying ADBC engine
and for DuckDB spatial databases and GeoParquet datasets. GeoParquet bounding
box column and/or DuckDB native RTree spatial indices are used when available.

Examples
--------

.. example::
   :title: Convert a Parquet file to GeoPackage

   Assuming :file:`libduckdb.so`, :file:`libduckdb.dylib` or :file:`duckdb.dll`
   is available (and it is in a system location, or can be located through
   LD_LIBRARY_PATH on Linux, DYLD_LIBRARY_PATH on MacOSX or PATH on Windows).

   .. code-block:: bash

      ogr2ogr out.gpkg in.parquet

.. example::
   :title: Convert a DuckDB database to GeoPackage

   Assuming :file:`libduckdb.so`, :file:`libduckdb.dylib` or :file:`duckdb.dll`
   is available (and it is in a system location, or can be located through
   LD_LIBRARY_PATH on Linux, DYLD_LIBRARY_PATH on MacOSX or PATH on Windows).

  
   .. code-block:: bash

      ogr2ogr out.gpkg in.duckdb


See Also
--------

`ADBC: Arrow Database Connectivity <https://arrow.apache.org/adbc/current/index.html>`__
