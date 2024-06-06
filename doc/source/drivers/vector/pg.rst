.. _vector.pg:

PostgreSQL / PostGIS
====================

.. shortname:: PostgreSQL

.. build_dependencies:: PostgreSQL client library (libpq)

This driver implements support for access to spatial tables in
PostgreSQL extended with the `PostGIS <http://postgis.net/>`__ spatial
data support. Some support exists in the driver for use with PostgreSQL
without PostGIS but with less functionalities.

This driver requires a connection to a Postgres database. If you want to
prepare a SQL dump to inject it later into a Postgres database, you can
instead use the :ref:`PostgreSQL SQL Dump driver <vector.pgdump>`.

You can find additional information on the driver in the :ref:`Advanced OGR
PostgreSQL driver Information <vector.pg_advanced>` page.

Starting with GDAL 3.9, only PostgreSQL >= 9 and PostGIS >= 2 are supported.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Connecting to a database
------------------------

| To connect to a Postgres datasource, use a connection string
  specifying the database name, with additional parameters as necessary.
  The PG: prefix is used to mark the name as a postgres connection
  string.

   ::

      PG:dbname=databasename

   *or*

   ::

      PG:"dbname='databasename' host='addr' port='5432' user='x' password='y'"

   In this syntax each parameter setting is in the form keyword = value.
   Spaces around the equal sign are optional. To write an empty value, or a
   value containing spaces, surround it with single quotes, e.g.,
   keyword = 'a value'. Single quotes and backslashes within the value must
   be escaped with a backslash, i.e., \' and \\.


   Starting with GDAL 3.1 also this syntax is supported:

   ::

      PG:service=servicename


   Starting with GDAL 3.4, the URI syntax is also supported

   ::

      postgresql://[user[:password]@][netloc][:port][/dbname][?param1=value1&...]

| It's also possible to omit the database name and connect to a
  *default* database, with the same name as the user name.
| **Note**: We use PQconnectdb() to make the connection. See details from
  `PostgreSQL libpq documentation <https://www.postgresql.org/docs/12/libpq-connect.html>`__).


Geometry columns
----------------

If the *geometry_columns* table exists (i.e. PostGIS is enabled for the
accessed database), then all tables and named views listed in the
*geometry_columns* table will be treated as OGR layers. Otherwise
(PostGIS disabled for the accessed database), all regular user tables
and named views will be treated as layers.

The driver also supports the
`geography <http://postgis.net/docs/manual-1.5/ch04.html#PostGIS_Geography>`__
column type.

The driver also supports reading and writing the
following non-linear geometry types :CIRCULARSTRING, COMPOUNDCURVE,
CURVEPOLYGON, MULTICURVE and MULTISURFACE

SQL statements
--------------

The PostgreSQL driver passes SQL statements directly to PostgreSQL by
default, rather than evaluating them internally when using the
ExecuteSQL() call on the OGRDataSource, or the -sql command option to
ogr2ogr. Attribute query expressions are also passed directly through to
PostgreSQL. It's also possible to request the ogr Pg driver to handle
SQL commands with the :ref:`OGR SQL <ogr_sql_dialect>` engine, by
passing **"OGRSQL"** string to the ExecuteSQL() method, as the name of
the SQL dialect.

Note that the PG driver uses PostgreSQL cursors to browse through the result
set of a ExecuteSQL() request, and that, at time of writing, PostgreSQL default
settings aren't optimized when the result set is small enough to fit in one
result page. If you experiment bad performance, specifying the
``PRELUDE_STATEMENTS=SET cursor_tuple_fraction = 1.0;`` open option might help.

The PostgreSQL driver in OGR supports the
OGRDataSource::StartTransaction(), OGRDataSource::CommitTransaction()
and OGRDataSource::RollbackTransaction() calls in the normal SQL sense.

Creation Issues
---------------

The PostgreSQL driver does not support creation of new datasets (a
database within PostgreSQL), but it does allow creation of new layers
within an existing database.

As mentioned above the type system is impoverished, and many OGR types
are not appropriately mapped into PostgreSQL.

If the database has PostGIS types loaded (i.e. the geometry type), newly
created layers will be created with the PostGIS Geometry type. Otherwise
they will use OID.

By default it is assumed that text being sent to Postgres is in the
UTF-8 encoding. This is fine for plain ASCII, but can result in errors
for extended characters (ASCII 155+, LATIN1, etc). While OGR provides no
direct control over this, you can set the PGCLIENTENCODING environment
variable to indicate the format being provided. For instance, if your
text is LATIN1 you could set the environment variable to LATIN1 before
using OGR and input would be assumed to be LATIN1 instead of UTF-8. An
alternate way of setting the client encoding is to issue the following
SQL command with ExecuteSQL() : "SET client_encoding TO encoding_name"
where encoding_name is LATIN1, etc. Errors can be caught by enclosing
this command with a CPLPushErrorHandler()/CPLPopErrorHandler() pair.

Updating existing tables
------------------------
When data is appended to an existing table (for example, using the
``-append`` option in ``ogr2ogr``) the driver will, by default,
emit an INSERT statement for each row of data to be added. This may
be significantly slower than the COPY-based approach taken when creating
a new table, but ensures consistency of unique identifiers if multiple
connections are accessing the table simultaneously.

If only one connection is accessing the table when data is appended, the
COPY-based approach can be chosen by setting the config option
``PG_USE_COPY`` to ``YES``, which may significantly speed up the operation.

Dataset open options
~~~~~~~~~~~~~~~~~~~~

-  .. oo:: DBNAME
      :choices: <string>

      Database name.

-  .. oo:: PORT
      :choices: <integer>

      Port.

-  .. oo:: USER
      :choices: <string>

      User name.

-  .. oo:: PASSWORD
      :choices: <string>

      Password.

-  .. oo:: HOST
      :choices: <string>

      Server hostname.

-  .. oo:: SERVICE
      :choices: <string>
      :since: 3.1

      Service name

-  .. oo:: ACTIVE_SCHEMA
      :choices: <string>

      Active schema.

-  .. oo:: SCHEMAS

      Restricted sets of schemas to explore (comma separated).

-  .. oo:: TABLES

      Restricted set of tables to list (comma separated).

-  .. oo:: LIST_ALL_TABLES
      :choices: YES, NO

      This may be "YES" to force all tables,
      including non-spatial ones, to be listed.

-  .. oo:: SKIP_VIEWS
      :choices: YES, NO
      :since: 3.7

      This may be "YES" to prevent views from being listed.

-  .. oo:: PRELUDE_STATEMENTS
      :since: 2.1

      SQL statement(s) to
      send on the PostgreSQL client connection before any other ones. In
      case of several statements, they must be separated with the
      semi-column (;) sign. The driver will specifically recognize BEGIN as
      the first statement to avoid emitting BEGIN/COMMIT itself. This
      option may be useful when using the driver with pg_bouncer in
      transaction pooling, e.g. 'BEGIN; SET LOCAL statement_timeout TO
      "1h";'

-  .. oo:: CLOSING_STATEMENTS
      :since: 2.1

      SQL statement(s) to
      send on the PostgreSQL client connection after any other ones. In
      case of several statements, they must be separated with the
      semi-column (;) sign. With the above example value for
      :oo::`PRELUDE_STATEMENTS`, the appropriate CLOSING_STATEMENTS would be
      "COMMIT".

Dataset Creation Options
~~~~~~~~~~~~~~~~~~~~~~~~

None

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  .. lco:: GEOM_TYPE
      :choices: geometry, geography, BYTEA, OID

      The GEOM_TYPE layer creation option can be set to one
      of "geometry", "geography", "BYTEA" or "OID" to
      force the type of geometry used for a table. For a PostGIS database,
      "geometry" is the default value. PostGIS "geography" assumes a geographic
      SRS (before PostGIS 2.2, it was even required to be EPSG:4326), but the
      driver has no built-in reprojection logic so it is
      safest to use always `-t_srs EPSG:4326` (or the canonical geographic CRS
      corresponding to the projected CRS of interest to avoid datum transformations)
      with :lco:`GEOM_TYPE=geography`.

-  .. lco:: OVERWRITE
      :choices: YES, NO

      This may be "YES" to force an existing layer of the
      desired name to be destroyed before creating the requested layer.

-  .. lco:: LAUNDER
      :choices: YES, NO
      :default: YES

      This may be "YES" to force new fields created on this
      layer to have their field names "laundered" into a form more
      compatible with PostgreSQL. This converts to lower case and converts
      some special characters like "-" and "#" to "_". If "NO" exact names
      are preserved. If enabled the table (layer) name will also be laundered.

-  .. lco:: LAUNDER_ASCII
      :choices: YES, NO
      :default: NO
      :since: 3.9

      Implies LAUNDER=YES, with the extra substitution of UTF-8 accented
      characters in the `Latin-1 Supplement <https://en.wikipedia.org/wiki/Latin-1_Supplement>`__
      and `Latin Extented-A <https://en.wikipedia.org/wiki/Latin_Extended-A>`__
      sets with the closest ASCII letter. Other non-ASCII characters are
      replaced with underscore.
      Consequently this option is not appropriate for non-Latin languages.

-  .. lco:: PRECISION
      :choices: YES, NO
      :default: YES

      This may be "YES" to force new fields created on this
      layer to try and represent the width and precision information, if
      available using NUMERIC(width,precision) or CHAR(width) types. If
      "NO" then the types FLOAT8, INTEGER and VARCHAR will be used instead.

-  .. lco:: DIM
      :choices: 2, 3, XYM, XYZM

      Control the dimension of the layer. Important
      to set to 2 for 2D layers as it has constraints on
      the geometry dimension during loading.

-  .. lco:: GEOMETRY_NAME

      Set name of geometry column in new table. If
      omitted it defaults to *wkb_geometry* for GEOM_TYPE=geometry, or
      *the_geog* for GEOM_TYPE=geography.

-  .. lco:: SCHEMA

      Set name of schema for new table. Using the same layer
      name in different schemas is supported, but not in the public schema
      and others. Note that using the -overwrite option of ogr2ogr and -lco
      SCHEMA= option at the same time will not work, as the ogr2ogr utility
      will not understand that the existing layer must be destroyed in the
      specified schema. Use the -nln option of ogr2ogr instead, or better
      the active_schema connection string. See below example.

-  .. lco:: SPATIAL_INDEX
      :choices: NONE, GIST, SPGIST, BRIN
      :default: GIST

      Creates a
      spatial index (GiST) on the geometry column to speed up queries (Has
      effect only when PostGIS is available). Set to NONE (GDAL >= 2.4, or
      FALSE for earlier versions) to disable. BRIN is only available with
      PostgreSQL >= 9.4 and PostGIS >= 2.3. SPGIST is only available with
      PostgreSQL >= 11 and PostGIS >= 2.5

-  .. lco:: TEMPORARY
      :choices: ON, OFF
      :default: OFF

      Set to OFF by default. Creates a
      temporary table instead of a permanent one.

-  .. lco:: UNLOGGED
      :choices: ON, OFF

      Set to OFF by default. Whether to
      create the table as a unlogged one. Unlogged tables are only
      supported since PostgreSQL 9.1, and GiST indexes used for spatial
      indexing since PostgreSQL 9.3.

-  .. lco:: NONE_AS_UNKNOWN
      :choices: YES, NO

      Can bet set to YES to force
      non-spatial layers (wkbNone) to be created as spatial tables of type
      GEOMETRY (wkbUnknown).
      Defaults to NO, in which case a regular table is created and not
      recorded in the PostGIS geometry_columns table.

-  .. lco:: FID
      :default: ogc_fid

      Name of the FID column to create.

-  .. lco:: FID64
      :choices: TRUE, FALSE
      :default: FALSE

      This may be "TRUE" to create a FID column
      that can support 64 bit identifiers.

-  .. lco:: EXTRACT_SCHEMA_FROM_LAYER_NAME
      :choices: YES, NO
      :default: YES

      Can be set to
      NO to avoid considering the dot character as the separator between
      the schema and the table name.

-  .. lco:: COLUMN_TYPES

      A list of strings of format
      field_name=pg_field_type (separated by comma) that should be use when
      CreateField() is invoked on them. This will override the default
      choice that OGR would have made. This can for example be used to
      create a column of type
      `HSTORE <http://www.postgresql.org/docs/9.0/static/hstore.html>`__.

-  .. lco:: DESCRIPTION
      :since: 2.1

      Description string to put in the
      pg_description system table. On reading, if such a description is
      found, it is exposed in the DESCRIPTION metadata item. The
      description can also be written with SetMetadataItem("DESCRIPTION",
      description_string). Descriptions are preserved by default by
      ogr2ogr, unless the -nomd option is used.

Configuration Options
~~~~~~~~~~~~~~~~~~~~~

The following :ref:`configuration options <configoptions>` are
available:

-  .. config:: PG_USE_COPY

      This may be "YES" for using COPY for inserting data
      to Postgresql. COPY is significantly faster than INSERT. COPY is used by
      default when inserting from a table that has just been created.

      .. warning:: At time of writing, PgPoolII is not compatible with COPY
                   mode as used by the OGR PostgreSQL driver. Thus you should
                   force PG_USE_COPY=NO when using PgPoolII.

-  .. config:: PGSQL_OGR_FID

      Set name of primary key instead of 'ogc_fid'. Only
      used when opening a layer whose primary key cannot be autodetected.
      Ignored by CreateLayer() that uses the FID creation option.

-  .. config:: PG_LIST_ALL_TABLES
      :choices: YES, NO

      Equivalent of :oo:`LIST_ALL_TABLES`.

-  .. config:: PG_USE_BASE64
      :choices: YES, NO
      :default: NO

      If set to "YES", geometries will
      be fetched as BASE64 encoded EWKB instead of canonical HEX encoded
      EWKB. This reduces the amount of data to be transferred from 2 N to
      1.333 N, where N is the size of EWKB data. However, it might be a bit
      slower than fetching in canonical form when the client and the server
      are on the same machine, so the default is NO.

-  .. config:: OGR_PG_CURSOR_PAGE

      Set the cursor page size, or
      number of features that are fetched from the database and held in memory
      at a single time.

-  .. config:: OGR_PG_RETRIEVE_FID
      :choices: YES, NO
      :default: YES

      If set to "YES" (the default),
      writing an OGRFeature will cause its FID to be set to the value assigned by
      the database. If a single feature is to be inserted multiple times,
      this option can be set to "NO" to allow the database to assign a new FID
      for each insertion.

-  .. config:: OGR_TRUNCATE

      If set to "YES", the content of the
      table will be first erased with the SQL TRUNCATE command before
      inserting the first feature. This is an alternative to using the
      -overwrite flag of ogr2ogr, that avoids views based on the table to
      be destroyed. Typical use case: ``ogr2ogr -append PG:dbname=foo
      abc.shp --config OGR_TRUNCATE YES``.

-  .. config:: OGR_PG_ENABLE_METADATA
      :choices: YES, NO
      :default: YES
      :since: 3.9

      If set to "YES" (the default), the driver will try to use (and potentially
      create) the ``ogr_system_tables.metadata`` table to retrieve and store
      layer metadata.

Examples
~~~~~~~~

-  Simple translation of a shapefile into PostgreSQL. The table 'abc'
   will be created with the features from abc.shp and attributes from
   abc.dbf. The database instance (warmerda) must already exist, and the
   table abc must not already exist.

   ::

      ogr2ogr -f PostgreSQL PG:dbname=warmerda abc.shp

-  This second example loads a political boundaries layer from VPF (via
   the :ref:`OGDI driver <vector.ogdi>`), and renames the layer from the
   cryptic OGDI layer name to something more sensible. If an existing
   table of the desired name exists it is overwritten.

   ::

      ogr2ogr -f PostgreSQL PG:dbname=warmerda \
              gltp:/vrf/usr4/mpp1/v0eur/vmaplv0/eurnasia \
              -lco OVERWRITE=yes -nln polbndl_bnd 'polbndl@bnd(*)_line'

- Export a single Postgres table to GeoPackage:

   ::

     ogr2ogr \
       -f GPKG output.gpkg \
       PG:"dbname='my_database'" "my_table"

- Export many Postgres tables to GeoPackage:

   ::

     ogr2ogr \
       -f GPKG output.gpkg \
       PG:"dbname='my_database' tables='table_1,table_3'"

- Export a whole Postgres database to GeoPackage:

   ::

     ogr2ogr \
       -f GPKG output.gpkg \
       PG:dbname=my_database


- Load a single layer GeoPackage into Postgres:

   ::

     ogr2ogr \
       -f "PostgreSQL" PG:"dbname='my_database'" \
       input.gpkg \
       -nln "name_of_new_table"


-  In this example we merge tiger line data from two different
   directories of tiger files into one table. Note that the second
   invocation uses -append and no :lco:`OVERWRITE=yes`.

   ::

      ogr2ogr -f PostgreSQL PG:dbname=warmerda tiger_michigan \
           -lco OVERWRITE=yes CompleteChain
      ogr2ogr -update -append -f PostgreSQL PG:dbname=warmerda tiger_ohio \
           CompleteChain

-  This example shows using ogrinfo to evaluate an SQL query statement
   within PostgreSQL. More sophisticated PostGIS specific queries may
   also be used via the -sql commandline switch to ogrinfo.

   ::

      ogrinfo -ro PG:dbname=warmerda -sql "SELECT pop_1994 from canada where province_name = 'Alberta'"

-  This example shows using ogrinfo to list PostgreSQL/PostGIS layers on
   a different host.

   ::

      ogrinfo -ro PG:"host='myserver.velocet.ca' user='postgres' dbname='warmerda'"

-  This example shows use of :oo:`PRELUDE_STATEMENTS` and :oo:`CLOSING_STATEMENTS`
   as destination open options of ogr2ogr.

   ::

      ogrinfo PG:"dbname='mydb'" poly -doo "PRELUDE_STATEMENTS=BEGIN; SET LOCAL statement_timeout TO '1h';" -doo CLOSING_STATEMENTS=COMMIT

FAQs
----

-  **Why can't I see my tables? PostGIS is installed and I have data**
   You must have permissions on all tables you want to read *and*
   geometry_columns and spatial_ref_sys.
   Misleading behavior may follow without an error message if you do not
   have permissions to these tables. Permission issues on
   geometry_columns and/or spatial_ref_sys tables can be generally
   confirmed if you can see the tables by setting the configuration
   option :config:`PG_LIST_ALL_TABLES` to YES. (e.g. ``ogrinfo --config
   PG_LIST_ALL_TABLES YES PG:xxxxx``)

See Also
--------

-  :ref:`Advanced OGR PostgreSQL driver Information <vector.pg_advanced>`
-  :ref:`OGR PostgreSQL SQL Dump driver Page <vector.pgdump>`
-  `PostgreSQL Home Page <http://www.postgresql.org/>`__
-  `PostGIS <http://postgis.net/>`__
-  `PostGIS / OGR Wiki Examples
   Page <http://trac.osgeo.org/postgis/wiki/UsersWikiOGR>`__

.. toctree::
   :maxdepth: 1
   :hidden:

   pg_advanced
