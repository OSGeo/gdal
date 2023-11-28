.. _ogr_sql_sqlite_dialect:

================================================================================
OGR SQL dialect and SQLITE SQL dialect
================================================================================

The GDALDataset supports executing commands against a datasource via the
:cpp:func:`GDALDataset::ExecuteSQL` method. How such commands are evaluated
is dependent on the datasets.

- For most file formats (e.g. Shapefiles, GeoJSON, MapInfo files), the built-in
  :ref:`ogr_sql_dialect` dialect will be used by defaults. It is also possible
  to request the :ref:`sql_sqlite_dialect` alternate dialect to be used, which
  will use the SQLite engine to evaluate commands on GDAL datasets.

- All OGR drivers for database systems: :ref:`vector.mysql`, :ref:`vector.pg`,
  :ref:`vector.oci`, :ref:`vector.sqlite`, :ref:`vector.gpkg`,
  :ref:`vector.odbc`, :ref:`vector.pgeo`, :ref:`vector.hana` and :ref:`vector.mssqlspatial`,
  override the :cpp:func:`GDALDataset::ExecuteSQL` function with dedicated implementation
  and, by default, pass the SQL statements directly to the underlying RDBMS.
  In these cases the SQL syntax varies in some particulars from OGR SQL.
  Also, anything possible in SQL can then be accomplished for these particular
  databases. Generally, only the result of SELECT statements will be returned as
  layers. For those drivers, it is also possible to explicitly request the
  ``OGRSQL`` and ``SQLITE`` dialects, although performance will generally be
  much less as the native SQL engine of those database systems.

Dialects
--------

.. toctree::
   :maxdepth: 1

   ogr_sql_dialect
   sql_sqlite_dialect


ExecuteSQL()
------------

SQL is executed against an GDALDataset, not against a specific layer.  The
call looks like this:

.. code-block:: cpp

    OGRLayer * GDALDataset::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect );

The ``pszDialect`` argument is in theory intended to allow for support of
different command languages against a provider, but for now applications
should always pass an empty (not NULL) string to get the default dialect.

The ``poSpatialFilter`` argument is a geometry used to select a bounding rectangle
for features to be returned in a manner similar to the
:cpp:func:`OGRLayer::SetSpatialFilter` method.  It may be NULL for no special spatial
restriction.

The result of an ExecuteSQL() call is usually a temporary OGRLayer representing
the results set from the statement.  This is the case for a SELECT statement
for instance.  The returned temporary layer should be released with
:cpp:func:`GDALDataset::ReleaseResultsSet` method when no longer needed.  Failure
to release it before the datasource is destroyed may result in a crash.
