.. _raster.rasterlite:

================================================================================
Rasterlite - Rasters in SQLite DB
================================================================================

.. shortname:: Rasterlite

.. build_dependencies:: libsqlite3

The Rasterlite driver allows reading and
creating Rasterlite databases.

| Those databases can be produced by the utilities of the
  `rasterlite <http://www.gaia-gis.it/spatialite>`__ distribution, such
  as rasterlite_load, rasterlite_pyramids, ....
| The driver supports reading grayscale, paletted and RGB images stored
  as GIF, PNG, TIFF or JPEG tiles. The driver also supports reading
  overviews/pyramids, spatial reference system and spatial extent.

GDAL/OGR must be compiled with OGR SQLite driver support. For read
support, linking against spatialite library is not required, but recent
enough sqlite3 library is needed to read rasterlite databases.
rasterlite library is not required either.

For write support a new table, linking against spatialite library \*is\*
required.

Although the Rasterlite documentation only mentions GIF, PNG, TIFF, JPEG
as compression formats for tiles, the
driver supports reading and writing internal tiles in any format handled
by GDAL. Furthermore, the Rasterlite driver also allow reading and
writing as many bands and as many band types as supported by the driver
for the internal tiles.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Connection string syntax in read mode
-------------------------------------

Syntax: 'rasterlitedb_name' or
'RASTERLITE:rasterlitedb_name[,table=raster_table_prefix][,minx=minx_val,miny=miny_val,maxx=maxx_val,maxy=maxy_val][,level=level_number]

where :

-  *rasterlitedb_name* is the filename of the RasterLite DB.
-  *raster_table_prefix* is the prefix of the raster table to open. For
   each raster, there are 2 corresponding SQLite tables, suffixed with
   \_rasters and \_metadata
-  *minx_val,miny_val,maxx_val,maxy_val* set a user-defined extent
   (expressed in coordinate system units) for the raster that can be
   different from the default extent.
-  *level_number* is the level of the pyramid/overview to open, 0 being
   the base pyramid.

Creation issues
---------------

The driver can create a new database if necessary, create a new raster
table if necessary and copy a source dataset into the specified raster
table.

If data already exists in the raster table, the new data will be added.
You can use the WIPE=YES creation options to erase existing data.

The driver does not support updating a block in an existing raster
table. It can only append new data.

Syntax for the name of the output dataset:
'RASTERLITE:rasterlitedb_name,table=raster_table_prefix' or
'rasterlitedb_name'

It is possible to specify only the DB name as in the later form, but
only if the database does not already exists. In that case, the raster
table name will be base on the DB name itself.

Creation options
~~~~~~~~~~~~~~~~

-  **WIPE** (=NO by default): Set to YES to erase all preexisting data
   in the specified table

-  **TILED** (=YES by default) : Set to NO if the source dataset must be
   written as a single tile in the raster table

-  **BLOCKXSIZE**\ =n: Sets tile width, defaults to 256.

-  **BLOCKYSIZE**\ =n: Sets tile height, defaults to 256.

-  **DRIVER**\ =[GTiff/GIF/PNG/JPEG/...] : name of the GDAL
   driver to use for storing tiles. Defaults to GTiff

-  **COMPRESS**\ =[LZW/JPEG/DEFLATE/...] : (GTiff driver) name of the
   compression method

-  **PHOTOMETRIC**\ =[RGB/YCbCr/...] : (GTiff driver) photometric
   interpretation

-  **QUALITY** : (JPEG-compressed GTiff, JPEG and WEBP drivers)
   JPEG/WEBP quality 1-100. Defaults to 75

Overviews
---------

The driver supports building (if the dataset is opened in update mode)
and reading internal overviews.

If no internal overview is detected, the driver will try using external
overviews (.ovr files).

Options can be used for internal overviews
building. They can be specified with the RASTERLITE_OVR_OPTIONS
configuration option, as a comma separated list of the above creation
options. See below examples.

All resampling methods supported by GDAL
overviews are available.

Performance hints
-----------------

Some of the performance hints of the OGR SQLite driver apply. In
particular setting the OGR_SQLITE_SYNCHRONOUS configuration option to
OFF when creating a dataset or adding overviews might increase
performance on some filesystems.

After having added all the raster tables and building all the needed
overview levels, it is advised to run :

::

   ogrinfo rasterlitedb.sqlite -sql "VACUUM"

in order to optimize the database, and increase read performances
afterwards. This is particularly true with big rasterlite datasets. Note
that the operation might take a long time.

Examples
--------

-  Accessing a rasterlite DB with a single raster table :

   ::

      $ gdalinfo rasterlitedb.sqlite -noct

   Output:

   ::

      Driver: Rasterlite/Rasterlite
      Files: rasterlitedb.sqlite
      Size is 7200, 7200
      Coordinate System is:
      GEOGCS["WGS 84",
          DATUM["WGS_1984",
              SPHEROID["WGS 84",6378137,298.257223563,
                  AUTHORITY["EPSG","7030"]],
              AUTHORITY["EPSG","6326"]],
          PRIMEM["Greenwich",0,
              AUTHORITY["EPSG","8901"]],
          UNIT["degree",0.01745329251994328,
              AUTHORITY["EPSG","9122"]],
          AUTHORITY["EPSG","4326"]]
      Origin = (-5.000000000000000,55.000000000000000)
      Pixel Size = (0.002083333333333,-0.002083333333333)
      Metadata:
        TILE_FORMAT=GIF
      Image Structure Metadata:
        INTERLEAVE=PIXEL
      Corner Coordinates:
      Upper Left  (  -5.0000000,  55.0000000) (  5d 0'0.00"W, 55d 0'0.00"N)
      Lower Left  (  -5.0000000,  40.0000000) (  5d 0'0.00"W, 40d 0'0.00"N)
      Upper Right (  10.0000000,  55.0000000) ( 10d 0'0.00"E, 55d 0'0.00"N)
      Lower Right (  10.0000000,  40.0000000) ( 10d 0'0.00"E, 40d 0'0.00"N)
      Center      (   2.5000000,  47.5000000) (  2d30'0.00"E, 47d30'0.00"N)
      Band 1 Block=480x480 Type=Byte, ColorInterp=Palette
        Color Table (RGB with 256 entries)

-  Listing a multi-raster table DB :

   ::

      $ gdalinfo multirasterdb.sqlite

   Output:

   ::

      Driver: Rasterlite/Rasterlite
      Files:
      Size is 512, 512
      Coordinate System is `'
      Subdatasets:
        SUBDATASET_1_NAME=RASTERLITE:multirasterdb.sqlite,table=raster1
        SUBDATASET_1_DESC=RASTERLITE:multirasterdb.sqlite,table=raster1
        SUBDATASET_2_NAME=RASTERLITE:multirasterdb.sqlite,table=raster2
        SUBDATASET_2_DESC=RASTERLITE:multirasterdb.sqlite,table=raster2
      Corner Coordinates:
      Upper Left  (    0.0,    0.0)
      Lower Left  (    0.0,  512.0)
      Upper Right (  512.0,    0.0)
      Lower Right (  512.0,  512.0)
      Center      (  256.0,  256.0)

-  Accessing a raster table within a multi-raster table DB:

   ::

      $ gdalinfo RASTERLITE:multirasterdb.sqlite,table=raster1

-  Creating a new rasterlite DB with data encoded in JPEG tiles :

   ::

      $ gdal_translate -of Rasterlite source.tif RASTERLITE:my_db.sqlite,table=source -co DRIVER=JPEG

-  Creating internal overviews :

   ::

      $ gdaladdo RASTERLITE:my_db.sqlite,table=source 2 4 8 16

-  Cleaning internal overviews :

   ::

      $ gdaladdo -clean RASTERLITE:my_db.sqlite,table=source

-  Creating external overviews in a .ovr file:

   ::

      $ gdaladdo -ro RASTERLITE:my_db.sqlite,table=source 2 4 8 16

-  Creating internal overviews with options (GDAL 1.10 or later):

   ::

      $ gdaladdo RASTERLITE:my_db.sqlite,table=source 2 4 8 16 --config RASTERLITE_OVR_OPTIONS DRIVER=GTiff,COMPRESS=JPEG,PHOTOMETRIC=YCbCr

:

See Also
--------

-  `Spatialite and Rasterlite home
   page <http://www.gaia-gis.it/spatialite>`__
-  `Rasterlite
   manual <http://www.gaia-gis.it/gaia-sins/rasterlite-docs/rasterlite-man.pdf>`__
-  `Rasterlite
   howto <http://www.gaia-gis.it/gaia-sins/rasterlite-docs/rasterlite-how-to.pdf>`__
-  `Sample
   databases <http://www.gaia-gis.it/spatialite-2.3.1/resources.html>`__
-  :ref:`OGR SQLite driver <vector.sqlite>`
