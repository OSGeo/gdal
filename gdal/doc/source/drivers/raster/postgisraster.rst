.. _raster.postgisraster:

================================================================================
PostGISRaster -- PostGIS Raster driver
================================================================================

.. shortname:: PostGISRaster

.. build_dependencies:: PostgreSQL library

PostGIS Raster (previously known as WKT Raster) is the project that
provides raster support on PostGIS. Since September 26st, 2010, is an
official part of PostGIS 2.0+.

This driver was started during the Google Summer of Code 2009, and
significantly improved since then.

Currently, the driver provides read-only support to PostGIS Raster data
sources.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

Connecting to a database
------------------------

To connect to a PostGIS Raster datasource, use a connection string
specifying the database name, with additional parameters as necessary

::

   PG:"[host=''] [port:''] dbname='' [user=''] [password=''] [schema=''] [table=''] [column=''] [where=''] [mode=''] [outdb_resolution='']"

Note that the string, up to the part starting with "table='" is a
libpq-style connection string. That means that you can leave out
unnecessary fields (like password, in some cases).

-  **schema** - name of PostgreSQL schema where requested raster table
   is stored.
-  **table** - name of PostGIS Raster table. The table was created by
   the raster loader (eg. raster2pgsql utility).
-  **column** - name of raster column in raster table
-  **where** - option is used to filter the results of the raster table.
   Any SQL-WHERE expression is valid.
-  **mode** - option is used to know the expected arrangement of the
   raster table. There are 2 possible values

   -  **mode=1** - ONE_RASTER_PER_ROW mode. In this case, a raster table
      is considered as a bunch of different raster files. This mode is
      intended for raster tables storing different raster files. It's
      the default mode if you don't provide this field in connection
      string.
   -  **mode=2** - ONE_RASTER_PER_TABLE mode. In this case, a raster
      table is considered as a unique raster file, even if the table has
      more than one row. This mode is intended for reading tiled rasters
      from database.

-  **outdb_resolution** - (GDAL >= 2.3.1) option to specify how
   out-database rasters should be resolved. Default is server_side.

   -  **server_side**: The outDB raster will be fetched by the
      PostgreSQL server. This implies that outdb rasters are enabled on
      the server.
   -  **client_side**: The outDB raster filenames will be returned to
      the GDAL PostGISRaster client, which will open it on the client
      side. This implies that the filename stored on te server can be
      accessed by the client.
   -  **client_side_if_possible**: The outDB raster filenames will be
      returned to the GDAL PostGISRaster client, which will check if it
      can access them. If it can, that's equivalent to client_side.
      Otherwise that's equivalent to server_side. Note that this mode
      involves extra queries to the server.

Additional notes
~~~~~~~~~~~~~~~~

If a table stores a tiled raster and you execute the driver with mode=1,
each image tile will be considered as a different image, and will be
reported as a subdataset. There are use cases the driver can't still
work with. For example: non-regular blocked rasters. That cases are
detected and an error is raised. Anyway, as I've said, the driver is
under development, and will work with more raster arrangements ASAP.

There's an additional working mode. If you don't provide a table name,
the driver will look for existing raster tables in all allowed database'
schemas, and will report each table as a subdataset.

You must use this connection string's format in all the gdal tools, like
gdalinfo, gdal_translate, gdalwarp, etc.

Performance hints
~~~~~~~~~~~~~~~~~

To get the maximum performance from the driver, it is best to load the
raster in PostGIS raster with the following characteristics:

-  tiled: -t switch of raster2pgsql
-  with overview: -l 2,4,8,... switch of raster2pgsql
-  with a GIST spatial index on the raster column: -I switch of
   raster2pgsql
-  with constraints registered: -C switch of raster2pgsql

Examples
--------

To get a summary about your raster via GDAL use gdalinfo:

::

   gdalinfo  "PG:host=localhost port=5432 dbname='mydb' user='postgres' password='secret' schema='public' table=mytable"

For more examples, check the PostGIS Raster FAQ section: `Can I export
my PostGIS Raster data to other raster
formats? <https://postgis.net/docs/RT_FAQ.html#idm28288>`__

Credits
-------

The driver developers

-  Jorge Arévalo (jorgearevalo at libregis.org)
-  David Zwarg (dzwarg at azavea.com)
-  Even Rouault (even.rouault at spatialys.com)

See Also
--------

-  `GDAL PostGISRaster driver
   Wiki <https://trac.osgeo.org/gdal/wiki/frmts_wtkraster.html>`__
-  `PostGIS Raster
   documentation <https://postgis.net/docs/RT_reference.html>`__
