.. _vector.mysql:

MySQL
=====

.. shortname:: MySQL

.. build_dependencies:: MySQL library

This driver implements read and write access for spatial data in
`MySQL <http://www.mysql.org/>`__ tables.

When opening a database, its name should be specified in the form
"MYSQL:dbname[,options]" where the options can include comma separated
items like "user=*userid*", "password=*password*", "host=*host*" and
"port=*port*".

As well, a "tables=*table*;*table*..." option can be added to restrict
access to a specific list of tables in the database. This option is
primarily useful when a database has a lot of tables, and scanning all
their schemas would take a significant amount of time.

Currently all regular user tables are assumed to be layers from an OGR
point of view, with the table names as the layer names. Named views are
not currently supported.

If a single integer field is a primary key, it will be used as the FID
otherwise the FID will be assigned sequentially, and fetches by FID will
be extremely slow.

By default, SQL statements are passed directly to the MySQL database
engine. It's also possible to request the driver to handle SQL commands
with :ref:`OGR SQL <ogr_sql_dialect>` engine, by passing **"OGRSQL"**
string to the ExecuteSQL() method, as name of the SQL dialect.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Caveats
-------

-  In the case of a layer defined by a SQL statement, fields either
   named "OGC_FID" or those that are defined as NOT NULL, are a PRIMARY
   KEY, and are an integer-like field will be assumed to be the FID.
-  Geometry fields are read from MySQL using WKB format. Versions older
   than 5.0.16 of MySQL are known to have issues with some WKB
   generation and may not work properly.
-  The OGR_FID column, which can be overridden with the MYSQL_FID layer
   creation option, is implemented as a **INT UNIQUE NOT NULL
   AUTO_INCREMENT** field. This appears to implicitly create an index on
   the field.
-  The geometry column, which defaults to *SHAPE* and can be overridden
   with the GEOMETRY_NAME layer creation option, is created as a **NOT
   NULL** column in unless SPATIAL_INDEX is disabled. By default a
   spatial index is created at the point the table is created.
-  SRS information is stored using the OGC Simple Features for SQL
   layout, with *geometry_columns* and *spatial_ref_sys* metadata tables
   being created in the specified database if they do not already exist.
   The *spatial_ref_sys* table is **not** pre-populated with SRS and
   EPSG values like PostGIS. If no EPSG code is found for a given table,
   the MAX(SRID) value will be used. With MySQL 8.0 or later, the
   *ST_SPATIAL_REFERENCE_SYSTEMS* table provided by the database is used
   instead of *spatial_ref_sys*.
-  Connection timeouts to the server can be specified with the
   **MYSQL_TIMEOUT** environment variable. For example, SET
   MYSQL_TIMEOUT=3600. It is possible this variable only has an impact
   when the OS of the MySQL server is Windows.
-  The MySQL driver opens a connection to the database using
   CLIENT_INTERACTIVE mode. You can adjust this setting
   (interactive_timeout) in your mysql.ini or mysql.cnf file of your
   server to your liking.
-  We are using WKT to insert geometries into the database. If you are
   inserting big geometries, you will need to be aware of the
   *max_allowed_packet* parameter in the MySQL configuration. By default
   it is set to 1M, but this will not be large enough for really big
   geometries. If you get an error message like: *Got a packet bigger
   than 'max_allowed_packet' bytes*, you will need to increase this
   parameter.

Creation Issues
---------------

The MySQL driver does not support creation of new datasets (a database
within MySQL), but it does allow creation of new layers within an
existing database.

By default, the MySQL driver will attempt to preserve the precision of
OGR features when creating and reading MySQL layers. For integer fields
with a specified width, it will use **DECIMAL** as the MySQL field type
with a specified precision of 0. For real fields, it will use **DOUBLE**
with the specified width and precision. For string fields with a
specified width, **VARCHAR** will be used.

The MySQL driver makes no allowances for character encodings at this
time.

The MySQL driver is not transactional at this time.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **OVERWRITE**: This may be "YES" to force an existing layer of the
   desired name to be destroyed before creating the requested layer.
-  **LAUNDER**: This may be "YES" to force new fields created on this
   layer to have their field names "laundered" into a form more
   compatible with MySQL. This converts to lower case and converts some
   special characters like "-" and "#" to "_". If "NO" exact names are
   preserved. The default value is "YES".
-  **PRECISION**: This may be "TRUE" to attempt to preserve field widths
   and precisions for the creation and reading of MySQL layers. The
   default value is "TRUE".
-  **GEOMETRY_NAME**: This option specifies the name of the geometry
   column. The default value is "SHAPE".
-  **FID**: This option specifies the name of the FID column. The
   default value is "OGR_FID". Note: option was called MYSQL_FID in
   releases before GDAL 2
-  **FID64**: This may be "TRUE" to create a FID column
   that can support 64 bit identifiers. The default value is "FALSE".
-  **SPATIAL_INDEX**: May be "NO" to stop automatic creation of a
   spatial index on the geometry column, allowing NULL geometries and
   possibly faster loading.
-  **ENGINE**: Optionally specify database engine to use. In MySQL 4.x
   this must be set to MyISAM for spatial tables.

The following example datasource name opens the database schema
*westholland* with password *psv9570* for userid *root* on the port
*3306*. No hostname is provided, so localhost is assumed. The tables=
directive means that only the bedrijven table is scanned and presented
as a layer for use.

::

   MYSQL:westholland,user=root,password=psv9570,port=3306,tables=bedrijven

The following example uses ogr2ogr to create copy the world_borders
layer from a shapefile into a MySQL table. It overwrites a table with
the existing name *borders2*, sets a layer creation option to specify
the geometry column name to *SHAPE2*.

::

   ogr2ogr -f MySQL MySQL:test,user=root world_borders.shp -nln borders2 -update -overwrite -lco GEOMETRY_NAME=SHAPE2

The following example uses ogrinfo to return some summary information
about the borders2 layer in the test database.

::

   ogrinfo MySQL:test,user=root borders2 -so

       Layer name: borders2
       Geometry: Polygon
       Feature Count: 3784
       Extent: (-180.000000, -90.000000) - (180.000000, 83.623596)
       Layer SRS WKT:
       GEOGCS["GCS_WGS_1984",
           DATUM["WGS_1984",
               SPHEROID["WGS_84",6378137,298.257223563]],
           PRIMEM["Greenwich",0],
           UNIT["Degree",0.017453292519943295]]
       FID Column = OGR_FID
       Geometry Column = SHAPE2
       cat: Real (0.0)
       fips_cntry: String (80.0)
       cntry_name: String (80.0)
       area: Real (15.2)
       pop_cntry: Real (15.2)


