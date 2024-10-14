.. _vector.adbc:

ADBC -- Arrow Database Connectivity
===================================

.. versionadded:: 3.11

.. shortname:: ADBC

.. build_dependencies:: adbc-driver-manager

ADBC is a set of APIs and libraries for Arrow-native access to database. This
driver uses the ``adbc-driver-manager`` library to connect to available ADBC
drivers, and expose content as classic OGR features, or as a ArrowArrayStream.

Consult the `installation instruction <https://arrow.apache.org/adbc/current/driver/installation.html>`__
for the various ADBC drivers. At time of writing, there are drivers for
SQLite3, PostgreSQL, Snowflake, BigQuery, DuckDB, Flight SQL, etc.

The driver is read-only, and there is no support for spatial data currently.

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

Examples
--------

- Assuming :file:`libduckdb.so`, :file:`libduckdb.dylib` or :file:`duckdb.dll`
  is available (and it is in a system location, or can be located through
  LD_LIBRARY_PATH on Linux, DYLD_LIBRARY_PATH on MacOSX or PATH on Windows).

  Convert a Parquet file to GeoPackage:

  ::

      ogr2ogr out.gpkg in.parquet


- Assuming :file:`libduckdb.so`, :file:`libduckdb.dylib` or :file:`duckdb.dll`
  is available (and it is in a system location, or can be located through
  LD_LIBRARY_PATH on Linux, DYLD_LIBRARY_PATH on MacOSX or PATH on Windows).

  Convert a DuckDB database to GeoPackage:

  ::

      ogr2ogr out.gpkg in.duckdb


See Also
--------

`ADBC: Arrow Database Connectivity <https://arrow.apache.org/adbc/current/index.html>`__
