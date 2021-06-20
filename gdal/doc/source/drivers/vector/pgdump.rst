.. _vector.pgdump:

PostgreSQL SQL Dump
===================

.. shortname:: PGDump

.. built_in_by_default::

This write-only driver implements support for generating a SQL dump file
that can later be injected into a live PostgreSQL instance. It supports
PostgreSQL extended with the `PostGIS <http://postgis.net/>`__
geometries.

This driver is very similar to the PostGIS shp2pgsql utility.

Most creation options are shared with the regular PostgreSQL driver.

The PGDump driver supports creating tables with
multiple PostGIS geometry columns (following :ref:`rfc-41`)

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation options
----------------

Dataset Creation Options
~~~~~~~~~~~~~~~~~~~~~~~~

-  **LINEFORMAT**: By default files are created with the line
   termination conventions of the local platform (CR/LF on win32 or LF
   on all other systems). This may be overridden through use of the
   LINEFORMAT layer creation option which may have a value of **CRLF**
   (DOS format) or **LF** (Unix format).

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **GEOM_TYPE**: The GEOM_TYPE layer creation option can be set to one
   of "geometry" or "geography" (PostGIS >= 1.5) to force the type of
   geometry used for a table. "geometry" is the default value.
-  **LAUNDER**: This may be "YES" to force new fields created on this
   layer to have their field names "laundered" into a form more
   compatible with PostgreSQL. This converts to lower case and converts
   some special characters like "-" and "#" to "_". If "NO" exact names
   are preserved. The default value is "YES". If enabled the table
   (layer) name will also be laundered.
-  **PRECISION**: This may be "YES" to force new fields created on this
   layer to try and represent the width and precision information, if
   available using NUMERIC(width,precision) or CHAR(width) types. If
   "NO" then the types FLOAT8, INTEGER and VARCHAR will be used instead.
   The default is "YES".
-  **DIM={2,3,XYM,XYZM}**: Control the dimension of the layer. Important
   to set to 2 for 2D layers with PostGIS 1.0+ as it has constraints on
   the geometry dimension during loading.
-  **GEOMETRY_NAME**: Set name of geometry column in new table. If
   omitted it defaults to *wkb_geometry* for GEOM_TYPE=geometry, or
   *the_geog* for GEOM_TYPE=geography.
-  **SCHEMA**: Set name of schema for new table. Using the same layer
   name in different schemas is supported, but not in the public schema
   and others.
-  **CREATE_SCHEMA**: To be used in combination with
   SCHEMA. Set to ON by default so that the CREATE SCHEMA instruction is
   emitted. Turn to OFF to prevent CREATE SCHEMA from being emitted.
-  **SPATIAL_INDEX**\ =NONE/GIST/SPGIST/BRIN (starting with GDAL 2.4) or
   YES/NO for earlier versions and backward compatibility: Set to GIST
   (GDAL >=2.4, or YES for earlier versions) by default. Creates a
   spatial index (GiST) on the geometry column to speed up queries (Has
   effect only when PostGIS is available). Set to NONE (GDAL >= 2.4, or
   FALSE for earlier versions) to disable. BRIN is only available with
   PostgreSQL >= 9.4 and PostGIS >= 2.3. SPGIST is only available with
   PostgreSQL >= 11 and PostGIS >= 2.5
-  **TEMPORARY**: Set to OFF by default. Creates a temporary table
   instead of a permanent one.
-  **UNLOGGED**: Set to OFF by default. Whether to
   create the table as a unlogged one. Unlogged tables are only
   supported since PostgreSQL 9.1, and GiST indexes used for spatial
   indexing since PostgreSQL 9.3.
-  **WRITE_EWKT_GEOM**: Set to OFF by default. Turn to ON to write EWKT
   geometries instead of HEX geometries. This option will have no effect
   if PG_USE_COPY environment variable is to YES.
-  **CREATE_TABLE**: Set to ON by default so that tables are recreated
   if necessary. Turn to OFF to disable this and use existing table
   structure.
-  **DROP_TABLE**\ =ON/OFF/IF_EXISTS: Defaults to IF_EXISTS. Set to ON so that
   tables are destroyed before being recreated. Set to OFF to prevent
   DROP TABLE from being emitted. Set to IF_EXISTS
   in order DROP TABLE IF EXISTS to be emitted (needs PostgreSQL >= 8.2)
-  **SRID**: Set the SRID of the geometry. Defaults to -1, unless a SRS
   is associated with the layer. In the case, if the EPSG code is
   mentioned, it will be used as the SRID. (Note: the spatial_ref_sys
   table must be correctly populated with the specified SRID)
-  **NONE_AS_UNKNOWN**: Can be set to TRUE to force
   non-spatial layers (wkbNone) to be created as spatial tables of type
   GEOMETRY (wkbUnknown).
   Defaults to NO, in which case a regular table is created and not
   recorded in the PostGIS geometry_columns table.
-  **FID**: Name of the FID column to create. Defaults
   to 'ogc_fid'.
-  **FID64**: This may be "TRUE" to create a FID column
   that can support 64 bit identifiers. The default value is "FALSE".
-  **EXTRACT_SCHEMA_FROM_LAYER_NAME**: Can be set to
   NO to avoid considering the dot character as the separator between
   the schema and the table name. Defaults to YES.
-  **COLUMN_TYPES**: A list of strings of format
   field_name=pg_field_type (separated by comma) that should be use when
   CreateField() is invoked on them. This will override the default
   choice that OGR would have made. This can for example be used to
   create a column of type
   `HSTORE <http://www.postgresql.org/docs/9.0/static/hstore.html>`__.
-  **POSTGIS_VERSION**: Defaults to 2.2 starting with GDAL 3.2 (1.5 previously)
   Possible values: 1.5, 2.0 or 2.2.
   PostGIS 2.0 encodes differently non-linear geometry types.
   And 2.2 brings special handling for POINT EMPTY geometries.
-  **DESCRIPTION** (From GDAL 2.1) Description string to put in the
   pg_description system table. The description can also be written with
   SetMetadataItem("DESCRIPTION", description_string). Descriptions are
   preserved by default by ogr2ogr, unless the -nomd option is used.

Environment variables
~~~~~~~~~~~~~~~~~~~~~

-  **PG_USE_COPY**: This may be "YES" for using COPY for inserting data
   to Postgresql. COPY is significantly faster than INSERT.

VSI Virtual File System API support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The driver supports rwriting to files managed by VSI Virtual File System
API, which include "regular" files, as well as files in the /vsizip/,
/vsigzip/ domains.

Writing to /dev/stdout or /vsistdout/ is also supported.

Example
~~~~~~~

-  Simple translation of a shapefile into PostgreSQL into a file
   abc.sql. The table 'abc' will be created with the features from
   abc.shp and attributes from abc.dbf. The SRID is specified.
   PG_USE_COPY is set to YES to improve the performance.

   ::

      ogr2ogr --config PG_USE_COPY YES -f PGDump abc.sql abc.shp -lco SRID=32631

-  Pipe the output of the PGDump driver into the psql utility.

   ::

      ogr2ogr --config PG_USE_COPY YES -f PGDump /vsistdout/ abc.shp | psql -d my_dbname -f -

See Also
~~~~~~~~

-  :ref:`OGR PostgreSQL driver Page <vector.pg>`
-  `PostgreSQL Home Page <http://www.postgresql.org/>`__
-  `PostGIS <http://postgis.net/>`__
-  `PostGIS / OGR Wiki Examples
   Page <http://trac.osgeo.org/postgis/wiki/UsersWikiOGR>`__
