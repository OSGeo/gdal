.. _vector.odbc:

ODBC RDBMS
==========

.. shortname:: ODBC

.. build_dependencies:: ODBC library

OGR optionally supports spatial and non-spatial tables accessed via
ODBC. ODBC is a generic access layer for access to many database
systems, and data that can be represented as a database (collection of
tables). ODBC support is potentially available on Unix and Windows
platforms, but is only included in unix builds by special configuration
options.

ODBC datasources are accessed using a datasource name of the form
**ODBC:\ userid/password\ @\ dsn,\ schema.tablename(geometrycolname),...:srs_tablename(sridcolumn,srtextcolumn)**.
With optional items dropped the following are also acceptable:

-  **ODBC:\ userid/password\ @\ dsn**
-  **ODBC:\ userid\ @\ dsn,\ table_list**
-  **ODBC:\ dsn,\ table_list**
-  **ODBC:\ dsn**
-  **ODBC:\ dsn,\ table_list:srs_tablename**

The **dsn** is the ODBC Data Source Name. Normally ODBC datasources are
setup using an ODBC Administration tool, and assigned a DSN. That DSN is
what is used to access the datasource.

By default the ODBC searches for GEOMETRY_COLUMNS table. If found it is
used to identify the set of spatial tables that should be treated as
layers by OGR. If not found, then all tables in the datasource are
returned as non-spatial layers. However, if a table list (a list of
comma separated table names) is provided, then only those tables will be
represented as layers (non-spatial). Fetching the full definition of all
tables in a complicated database can be quite time consuming, so the
ability to restrict the set of tables accessed is primarily a
performance issue.

If the GEOMETRY_COLUMNS table is found, it is used to select a column to
be the geometry source. If the tables are passed in the datasource name,
then the geometry column associated with a table can be included in
round brackets after the tablename. It is currently a hardcoded
assumption that the geometry is in Well Known Binary (WKB) format if the
field is binary, or Well Known Text (WKT) otherwise. The
GEOMETRY_COLUMNS table should have at least the columns F_TABLE_NAME,
F_GEOMETRY_COLUMN and GEOMETRY_TYPE.

If the table has a geometry column, and has fields called XMIN, YMIN,
XMAX and YMAX then direct table queries with a spatial filter accelerate
the spatial query. The XMIN, YMIN, XMAX and YMAX fields should represent
the extent of the geometry in the row in the tables coordinate system.

By default, SQL statements are passed directly to the underlying
database engine. It's also possible to request the driver to handle SQL
commands with the :ref:`OGR SQL <ogr_sql_dialect>` engine, by passing
**"OGRSQL"** string to the ExecuteSQL() method, as name of the SQL
dialect.

Dataset open options
--------------------

-  **LIST_ALL_TABLES**\ =YES/NO: This may be "YES" to force all tables,
   including system and internal tables (such as the MSys* tables) to be listed (since GDAL 3.4).
   Applies to Microsoft Access Databases only. Note that the Windows Microsoft
   Access ODBC Driver always strips out MSys tables, and accordingly these
   will not be returned on Windows platforms even if LIST_ALL_TABLES is
   set to YES.

Driver capabilities
-------------------

.. supports_georeferencing::

Access Databases (.MDB) support
-------------------------------

On Windows provided that the "Microsoft
Access Driver (\*.mdb)" ODBC driver is installed, non-spatial MS Access
Databases (not Personal Geodabases or Geomedia databases) can be opened
directly by their filenames.

On Linux opening non-spatial MS Access Databases using the ODBC driver
is possible via installation of unixODBC and mdbtools. See
:ref:`MDB <vector.pgeo>` for instructions on how to enable this.

The driver supports either .mdb or .accdb extensions for
Microsoft Access databases. Additionally, it also supports
opening files with the ESRI .style database extension (which is just
an alias for the .mdb file extension).

Creation Issues
---------------

Currently the ODBC OGR driver is read-only, so new features, tables and
datasources cannot normally be created by OGR applications. This
limitation may be removed in the future.

See Also
~~~~~~~~

-  `MSDN ODBC API
   Reference <http://msdn.microsoft.com/en-us/library/ms714562(VS.85).aspx>`__
-  :ref:`PGeo driver <vector.pgeo>`
-  :ref:`Geomedia driver <vector.geomedia>`
-  :ref:`MDB driver <vector.mdb>`
