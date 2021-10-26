.. _raster.plmosaic:

================================================================================
PLMosaic (Planet Labs Mosaics API)
================================================================================

.. shortname:: PLMosaic

.. build_dependencies:: libcurl

This driver can connect to Planet Labs Mosaics API. GDAL/OGR must be
built with Curl support in order for the PLMosaic driver to be compiled.

The driver supports listing mosaics and reading them. Mosaics are
accessed at their highest resolution. Mosaics are typically composed of
quads of 4096x4096 pixels.

For mosaics of type Byte, overviews are available by using the tile API.
For other data types, there is no support for overviews, so requests
that involve downsampling may take a long time to complete.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a datasource is :

::

   PLMosaic:[options]

Additional optional parameters can be specified after the ':' sign.
Currently the following one is supported :

-  **api_key**\ =value: To specify the Planet API key. It is mandatory,
   unless it is supplied through the open option API_KEY, or the
   configuration option PL_API_KEY.
-  **mosaic**\ =mosaic_name: To specify the mosaic name.
-  **cache_path**\ =path: To specify the path to a directory where
   cached quads (and tiles) are stored. A plmosaic_cache/{mosaic_name}
   subdirectory will be created under that path. The empty string can be
   used to disable any disk caching.
-  **trust_cache**\ =YES/NO: Whether already cached quads should be
   reused directly, without prior checking if the server has a more
   recent version. Note: this only applies to quads, and not tiles.
   Default is NO.
-  **use_tiles**\ =YES/NO: Whether to use the tile API to access full
   resolution data, instead of downloading quads. Only apply for Byte
   mosaics. Default is NO.

If several parameters are specified, they must be separated by a comma.

If no mosaic parameter is supplied, the list of available mosaics will
be returned as subdatasets. If only one mosaic is available, it will be
directly opened.

Open options
------------

The following open options are available : API_KEY, MOSAIC, CACHE_PATH,
TRUST_CACHE and USE_TILES. They have the same semantics as the above
describe parameters of same name.

Configuration options
---------------------

The following configuration options are available :

-  **PL_API_KEY**\ =value: To specify the Planet API key.

Location information
--------------------

The special *Pixel_{x}_{y}* metadata item of the *LocationInfo* metadata
domain, where x is the column and y is the line in the mosaic, can be
queried to get information about the scenes that compose the underneath
quad. This is the syntax used by the gdallocationinfo utility (see :ref:`rfc-32`)

Below an example of the return :

::

   <LocationInfo>
     <Scenes>
       <Scene>
         <link>https://api.planet.com/data/v1/item-types/PSScene3Band/items/20161025_000336_0e19</link>
       </Scene>
       <Scene>
         <link>https://api.planet.com/data/v1/item-types/PSScene3Band/items/20161119_000453_0e14</link>
       </Scene>
       <Scene>
         <link>https://api.planet.com/data/v1/item-types/PSScene3Band/items/20161010_000309_0e26</link>
       </Scene>
       <Scene>
         <link>https://api.planet.com/data/v1/item-types/PSScene3Band/items/20161119_000452_0e14</link>
       </Scene>
     </Scenes>
   </LocationInfo>

Examples
~~~~~~~~

Listing all mosaics available (with the rights of the account) :

::

   gdalinfo "PLMosaic:" -oo API_KEY=some_value

or

::

   gdalinfo "PLMosaic:api_key=some_value"

or

::

   gdalinfo "PLMosaic:" --config PL_API_KEY some_value

returns (in case of multiple mosaics):

::

   Driver: PLMOSAIC/Planet Labs Mosaics API
   Files: none associated
   Size is 512, 512
   Coordinate System is `'
   Image Structure Metadata:
     INTERLEAVE=PIXEL
   Subdatasets:
     SUBDATASET_1_NAME=PLMOSAIC:mosaic=global_quarterly_2017q1_mosaic
     SUBDATASET_1_DESC=Mosaic global_quarterly_2017q1_mosaic
     ...
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,  512.0)
   Upper Right (  512.0,    0.0)
   Lower Right (  512.0,  512.0)
   Center      (  256.0,  256.0)

Open a particular mosaic :

::

   gdalinfo "PLMosaic:mosaic=global_quarterly_2017q1_mosaic" -oo API_KEY=some_value

returns:

::

   Driver: PLMOSAIC/Planet Labs Mosaics API
   Files: none associated
   Size is 8388608, 4427776
   Coordinate System is:
   PROJCS["WGS 84 / Pseudo-Mercator",
       GEOGCS["WGS 84",
           DATUM["WGS_1984",
               SPHEROID["WGS 84",6378137,298.257223563,
                   AUTHORITY["EPSG","7030"]],
               AUTHORITY["EPSG","6326"]],
           PRIMEM["Greenwich",0,
               AUTHORITY["EPSG","8901"]],
           UNIT["degree",0.0174532925199433,
               AUTHORITY["EPSG","9122"]],
           AUTHORITY["EPSG","4326"]],
       PROJECTION["Mercator_1SP"],
       PARAMETER["central_meridian",0],
       PARAMETER["scale_factor",1],
       PARAMETER["false_easting",0],
       PARAMETER["false_northing",0],
       UNIT["metre",1,
           AUTHORITY["EPSG","9001"]],
       AXIS["X",EAST],
       AXIS["Y",NORTH],
       EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs"],
       AUTHORITY["EPSG","3857"]]
   Origin = (-20037508.342789243906736,13384429.400847502052784)
   Pixel Size = (4.777314267823516,-4.777314267823516)
   Metadata:
     FIRST_ACQUIRED=2017-01-01T00:00:00.000Z
     LAST_ACQUIRED=2017-04-01T00:00:00.000Z
     NAME=global_quarterly_2017q1_mosaic
   Image Structure Metadata:
     INTERLEAVE=PIXEL
   Corner Coordinates:
   Upper Left  (-20037508.343,13384429.401) (180d 0' 0.00"W, 76d 0'57.94"N)
   Lower Left  (-20037508.343,-7768448.059) (180d 0' 0.00"W, 57d 2'26.63"S)
   Upper Right (20037508.343,13384429.401) (180d 0' 0.00"E, 76d 0'57.94"N)
   Lower Right (20037508.343,-7768448.059) (180d 0' 0.00"E, 57d 2'26.63"S)
   Center      (       0.000, 2807990.671) (  0d 0' 0.01"E, 24d26'49.74"N)
   Band 1 Block=256x256 Type=Byte, ColorInterp=Red
     Overviews: 4194304x4194304, ..., 256x256
     Mask Flags: PER_DATASET ALPHA
     Overviews of mask band: Overviews: 4194304x4194304, ..., 256x256
   Band 2 Block=256x256 Type=Byte, ColorInterp=Green
     Overviews: 4194304x4194304, ..., 256x256
     Mask Flags: PER_DATASET ALPHA
     Overviews of mask band: Overviews: 4194304x4194304, ..., 256x256
   Band 3 Block=256x256 Type=Byte, ColorInterp=Blue
     Overviews: 4194304x4194304, ..., 256x256
     Mask Flags: PER_DATASET ALPHA
     Overviews of mask band: Overviews: 4194304x4194304, ..., 256x256
   Band 4 Block=256x256 Type=Byte, ColorInterp=Alpha
     Overviews: 4194304x4194304, ..., 256x256

See Also
--------

-  `Documentation of Planet Mosaics
   API <https://docs.planet.com/reference#basemaps-and-mosaics>`__
-  `API
   Authentication <https://docs.planet.com/docs/api-mechanics#section-authentication>`__
-  :ref:`Vector PLScenes / Planet Scenes API driver <vector.plscenes>`
