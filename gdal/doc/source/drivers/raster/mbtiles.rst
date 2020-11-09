.. _raster.mbtiles:

================================================================================
MBTiles
================================================================================

.. shortname:: MBTiles

.. build_dependencies:: libsqlite3

The MBTiles driver allows reading rasters in
the MBTiles format, which is a specification for storing tiled map data
in SQLite databases.

Starting with GDAL 2.1, the MBTiles driver has creation and write
support for MBTiles raster datasets.

Starting with GDAL 2.3, the MBTiles driver has read and write support
for MBTiles vector datasets. For standalone Mapbox Vector Tile files or
set of MVT files, see the :ref:`MVT <vector.mvt>` driver. Note: vector
write support requires GDAL to be built with GEOS.

GDAL/OGR must be compiled with OGR SQLite driver support, and JPEG and
PNG drivers.

The SRS is always the Pseudo-Mercator (a.k.a Google Mercator)
projection.

Starting with GDAL 2.3, the driver will open a dataset as RGBA. For
previous versions, the driver will try to determine the number of bands
by probing the content of one tile. It is possible to alter this
behavior by defining the MBTILES_BAND_COUNT configuration option (or
starting with GDAL 2.1, the BAND_COUNT open option) to the number of
bands. The values supported are 1, 2, 3 or 4. Four band
(Red,Green,Blue,Alpha) dataset gives the maximum compatibility with the
various encodings of tiles that can be stored.

The driver will use the 'bounds' metadata in the metadata table and do
necessary tile clipping, if needed, to respect that extent. However that
information being optional, if omitted, the driver will use the extent
of the tiles at the maximum zoom level. The user can also specify the
USE_BOUNDS=NO open option to force the use of the actual extent of tiles
at the maximum zoom level. Or it can specify any of MINX/MINY/MAXX/MAXY
to have a custom extent.

The driver can retrieve pixel attributes encoded according to the
UTFGrid specification available in some MBTiles files. They can be
obtained with the gdallocationinfo utility, or with a
GetMetadataItem("Pixel_iCol_iLine", "LocationInfo") call on a band
object.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Opening options
---------------

Starting with GDAL 2.1, the following open options are available:

-  Raster and vector:

   -  **ZOOM_LEVEL**\ =value: Integer value between 0 and the maximum
      filled in the *tiles* table. By default, the driver will select
      the maximum zoom level, such as at least one tile at that zoom
      level is found in the 'tiles' table.
   -  **USE_BOUNDS**\ =YES/NO: Whether to use the 'bounds' metadata,
      when available, to determine the AOI. Defaults to YES.
   -  **MINX**\ =value: Minimum easting (in EPSG:3857) of the area of
      interest.
   -  **MINY**\ =value: Minimum northing (in EPSG:3857) of the area of
      interest.
   -  **MAXX**\ =value: Maximum easting (in EPSG:3857) of the area of
      interest.
   -  **MAXY**\ =value: Maximum northing (in EPSG:3857) of the area of
      interest.

-  Raster only:

   -  **BAND_COUNT**\ =AUTO/1/2/3/4: Number of bands of the dataset
      exposed after opening. Some conversions will be done when possible
      and implemented, but this might fail in some cases, depending on
      the BAND_COUNT value and the number of bands of the tile. Defaults
      to AUTO.
   -  **TILE_FORMAT**\ =PNG/PNG8/JPEG: Format used to store tiles. See
      `Tile formats <#tile_formats>`__ section. Only used in update
      mode. Defaults to PNG.
   -  **QUALITY**\ =1-100: Quality setting for JPEG compression. Only
      used in update mode. Default to 75.
   -  **ZLEVEL**\ =1-9: DEFLATE compression level for PNG tiles. Only
      used in update mode. Default to 6.
   -  **DITHER**\ =YES/NO: Whether to use Floyd-Steinberg dithering (for
      TILE_FORMAT=PNG8). Only used in update mode. Defaults to NO.

-  Vector only (GDAL >= 2.3):

   -  **CLIP**\ =YES/NO: Whether to clip geometries of vector features
      to tile extent. Defaults to YES.
   -  **ZOOM_LEVEL_AUTO**\ =YES/NO: Whether to auto-select the zoom
      level for vector layers according to the spatial filter extent.
      Only for display purpose. Defaults to NO.

Raster creation issues
----------------------

Depending of the number of bands of the input dataset and the tile
format selected, the driver will do the necessary conversions to be
compatible with the tile format. When using the CreateCopy() API (such
as with gdal_translate), automatic reprojection of the input dataset to
EPSG:3857 (WebMercator) will be done, with selection of the appropriate
zoom level.

Fully transparent tiles will not be written to the database, as allowed
by the format.

The driver implements the Create() and IWriteBlock() methods, so that
arbitrary writing of raster blocks is possible, enabling the direct use
of MBTiles as the output dataset of utilities such as gdalwarp.

On creation, raster blocks can be written only if the geotransformation
matrix has been set with SetGeoTransform() This is effectively needed to
determine the zoom level of the full resolution dataset based on the
pixel resolution, dataset and tile dimensions.

Technical/implementation note: in the general case, GDAL blocks do not
exactly match a single MBTiles tile. In which case, each GDAL block will
overlap four MBTiles tiles. This is easily handled on the read side, but
on creation/update side, such configuration could cause numerous
decompression/ recompression of tiles to be done, which might cause
unnecessary quality loss when using lossy compression (JPEG). To avoid
that, the driver will create a temporary database next to the main
MBTiles file to store partial MBTiles tiles in a lossless (and
uncompressed) way. Once a tile has received data for its four quadrants
and for all the bands (or the dataset is closed or explicitly flushed
with FlushCache()), those uncompressed tiles are definitely transferred
to the MBTiles file with the appropriate compression. All of this is
transparent to the user of GDAL API/utilities

Tile formats
~~~~~~~~~~~~

MBTiles can store tiles in PNG or JPEG. Support for those tile formats
depend if the underlying drivers are available in GDAL. By default, GDAL
will PNG tiles.

It is possible to select the tile format by setting the creation/open
option TILE_FORMAT to one of PNG, PNG8 or JPEG. When using JPEG, the
alpha channel will not be stored.

PNG8 can be selected to use 8-bit PNG with a color table up to 256
colors. On creation, an optimized color table is computed for each tile.
The DITHER option can be set to YES to use Floyd/Steinberg dithering
algorithm, which spreads the quantization error on neighbouring pixels
for better rendering (note however than when zooming in, this can cause
non desirable visual artifacts). Setting it to YES will generally cause
less effective compression. Note that at that time, such an 8-bit PNG
formulation is only used for fully opaque tiles, as the median-cut
algorithm currently implemented to compute the optimal color table does
not support alpha channel (even if PNG8 format would potentially allow
color table with transparency). So when selecting PNG8, non fully opaque
tiles will be stored as 32-bit PNG.

Vector creation issues
----------------------

Tiles are generated with WebMercator (EPSG:3857) projection. It is possible
to decide at which zoom level ranges a given layer is written. Several
layers can be written but the driver has only write-once support for
vector data. For writing several vector datasets into MBTiles file an
intermediate format like GeoPackage must be used as a container so that
all layers can be converted at the same time. Write-once support means also
that existing vector layers can't be edited.
   

Creation options
----------------

The following creation options are available:

-  Raster and vector:

   -  **NAME**\ =string. Tileset name, used to set the 'name' metadata
      item. If not specified, the basename of the filename will be used.
   -  **DESCRIPTION**\ =string. A description of the layer, used to set
      the 'description' metadata item. If not specified, the basename of
      the filename will be used.
   -  **TYPE**\ =overlay/baselayer. The layer type, used to set the
      'type' metadata item. Default to 'overlay'.

-  Raster only:

   -  **VERSION**\ =string. The version of the tileset, as a plain
      number, used to set the 'version' metadata item. Default to '1.1'.
   -  **BLOCKSIZE**\ =integer. (GDAL >= 2.3) Block/tile size in width
      and height in pixels. Defaults to 256. Maximum supported is 4096.
   -  **TILE_FORMAT**\ =PNG/PNG8/JPEG: Format used to store tiles. See
      `Tile formats <#tile_formats>`__ section. Defaults to PNG.
   -  **QUALITY**\ =1-100: Quality setting for JPEG compression. Default
      to 75.
   -  **ZLEVEL**\ =1-9: DEFLATE compression level for PNG tiles. Default
      to 6.
   -  **DITHER**\ =YES/NO: Whether to use Floyd-Steinberg dithering (for
      TILE_FORMAT=PNG8). Defaults to NO.
   -  **ZOOM_LEVEL_STRATEGY**\ =AUTO/LOWER/UPPER. Strategy to determine
      zoom level. LOWER will select the zoom level immediately below the
      theoretical computed non-integral zoom level, leading to
      subsampling. On the contrary, UPPER will select the immediately
      above zoom level, leading to oversampling. Defaults to AUTO which
      selects the closest zoom level.
   -  **RESAMPLING**\ =NEAREST/BILINEAR/CUBIC/CUBICSPLINE/LANCZOS/MODE/AVERAGE.
      Resampling algorithm. Defaults to BILINEAR.
   -  **WRITE_BOUNDS**\ =YES/NO: Whether to write the bounds 'metadata'
      item. Defaults to YES.

-  Vector only (GDAL >= 2.3):

   -  **MINZOOM**\ =integer: Minimum zoom level at which tiles are
      generated. Defaults to 0.
   -  **MAXZOOM**\ =integer: Minimum zoom level at which tiles are
      generated. Defaults to 5. Maximum supported value is 22
   -  **CONF**\ =string: Layer configuration as a JSon serialized
      string.
   -  **SIMPLIFICATION**\ =float: Simplification factor for linear or
      polygonal geometries. The unit is the integer unit of tiles after
      quantification of geometry coordinates to tile coordinates.
      Applies to all zoom levels, unless SIMPLIFICATION_MAX_ZOOM is also
      defined.
   -  **SIMPLIFICATION_MAX_ZOOM**\ =float: Simplification factor for
      linear or polygonal geometries, that applies only for the maximum
      zoom level.
   -  **EXTENT**\ =positive_integer. Number of units in a tile. The
      greater, the more accurate geometry coordinates (at the expense of
      tile byte size). Defaults to 4096
   -  **BUFFER**\ =positive_integer. Number of units for geometry
      buffering. This value corresponds to a buffer around each side of
      a tile into which geometries are fetched and clipped. This is used
      for proper rendering of geometries that spread over tile
      boundaries by some rendering clients. Defaults to 80 if
      EXTENT=4096.
   -  **COMPRESS**\ =YES/NO. Whether to compress tiles with the
      Deflate/GZip algorithm. Defaults to YES. Should be left to YES for
      FORMAT=MBTILES.
   -  **TEMPORARY_DB**\ =string. Filename with path for the temporary
      database used for tile generation. By default, this will be a file
      in the same directory as the output file/directory.
   -  **MAX_SIZE**\ =integer. Maximum size of a tile in bytes (after
      compression). Defaults to 500 000. If a tile is greater than this
      threshold, features will be written with reduced precision, or
      discarded.
   -  **MAX_FEATURES**\ =integer. Maximum number of features per tile.
      Defaults to 200 000.
   -  **BOUNDS**\ =min_long,min_lat,max_long,max_lat. Override default
      value for bounds metadata item which is computed from the extent
      of features written.
   -  **CENTER**\ =long,lat,zoom_level. Override default value for
      center metadata item, which is the center of BOUNDS at minimum
      zoom level.

Layer configuration (vector)
----------------------------

The above mentioned CONF dataset creation option can be set to a string
whose value is a JSon serialized document such as the below one:

::

           {
               "boundaries_lod0": {
                   "target_name": "boundaries",
                   "description": "Country boundaries",
                   "minzoom": 0,
                   "maxzoom": 2
               },
               "boundaries_lod1": {
                   "target_name": "boundaries",
                   "minzoom": 3,
                   "maxzoom": 5
               }
           }

*boundaries_lod0* and *boundaries_lod1* are the name of the OGR layers
that are created into the target MVT dataset. They are mapped to the MVT
target layer *boundaries*.

It is also possible to get the same behavior with the below layer
creation options, although that is not convenient in the ogr2ogr use
case.

Layer creation options (vector)
-------------------------------

-  **MINZOOM**\ =integer: Minimum zoom level at which tiles are
   generated. Defaults to the dataset creation option MINZOOM value.
-  **MAXZOOM**\ =integer: Minimum zoom level at which tiles are
   generated. Defaults to the dataset creation option MAXZOOM value.
   Maximum supported value is 22
-  **NAME**\ =string: Target layer name. Defaults to the layer name, but
   can be overridden so that several OGR layers map to a single target
   MVT layer. The typical use case is to have different OGR layers for
   mutually exclusive zoom level ranges.
-  **DESCRIPTION**\ =string: A description of the layer.

Overviews (raster)
------------------

gdaladdo / BuildOverviews() can be used to compute overviews. Only
power-of-two overview factors (2,4,8,16,...) are supported.

If more overview levels are specified than available, the extra ones are
silently ignored.

Overviews can also be cleared with the -clean option of gdaladdo (or
BuildOverviews() with nOverviews=0)

Vector tiles
------------

Starting with GDAL 2.3, the MBTiles driver can read MBTiles files
containing vector tiles conforming to the Mapbox Vector Tile format
(format=pbf).

The driver requires the 'metadata' table to contain a name='json' entry,
that has a 'vector_layers' array describing layers and their schema. See
:ref:`metadata.json <mvt_metadata_json>`

Note: The driver will make no effort of stiching together geometries for
features that overlap several tiles.

Examples:
---------

-  Accessing a remote MBTiles raster :

   ::

      $ gdalinfo /vsicurl/http://a.tiles.mapbox.com/v3/kkaefer.iceland.mbtiles

   Output:

   ::

      Driver: MBTiles/MBTiles
      Files: /vsicurl/http://a.tiles.mapbox.com/v3/kkaefer.iceland.mbtiles
      Size is 16384, 16384
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
          EXTENSION["PROJ4","+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs"],
          AUTHORITY["EPSG","3857"]]
      Origin = (-3757031.250000000000000,11271093.750000000000000)
      Pixel Size = (152.873992919921875,-152.873992919921875)
      Image Structure Metadata:
        INTERLEAVE=PIXEL
      Corner Coordinates:
      Upper Left  (-3757031.250,11271093.750) ( 33d44'59.95"W, 70d36'45.36"N)
      Lower Left  (-3757031.250, 8766406.250) ( 33d44'59.95"W, 61d36'22.97"N)
      Upper Right (-1252343.750,11271093.750) ( 11d14'59.98"W, 70d36'45.36"N)
      Lower Right (-1252343.750, 8766406.250) ( 11d14'59.98"W, 61d36'22.97"N)
      Center      (-2504687.500,10018750.000) ( 22d29'59.97"W, 66d30'47.68"N)
      Band 1 Block=256x256 Type=Byte, ColorInterp=Red
        Overviews: 8192x8192, 4096x4096, 2048x2048, 1024x1024, 512x512
        Mask Flags: PER_DATASET ALPHA
        Overviews of mask band: 8192x8192, 4096x4096, 2048x2048, 1024x1024, 512x512
      Band 2 Block=256x256 Type=Byte, ColorInterp=Green
        Overviews: 8192x8192, 4096x4096, 2048x2048, 1024x1024, 512x512
        Mask Flags: PER_DATASET ALPHA
        Overviews of mask band: 8192x8192, 4096x4096, 2048x2048, 1024x1024, 512x512
      Band 3 Block=256x256 Type=Byte, ColorInterp=Blue
        Overviews: 8192x8192, 4096x4096, 2048x2048, 1024x1024, 512x512
        Mask Flags: PER_DATASET ALPHA
        Overviews of mask band: 8192x8192, 4096x4096, 2048x2048, 1024x1024, 512x512
      Band 4 Block=256x256 Type=Byte, ColorInterp=Alpha
        Overviews: 8192x8192, 4096x4096, 2048x2048, 1024x1024, 512x512

-  Reading pixel attributes encoded according to the UTFGrid
   specification :

   ::

      $ gdallocationinfo /vsicurl/http://a.tiles.mapbox.com/v3/mapbox.geography-class.mbtiles -wgs84 2 49 -b 1 -xml

   Output:

   ::

      <Report pixel="33132" line="22506">
        <BandReport band="1">
          <LocationInfo>
            <Key>74</Key>
            <JSon>{"admin":"France","flag_png":"iVBORw0KGgoAAAANSUhEUgAAAGQAAABDEAIAAAC1uevOAAAACXBIWXMAAABIAAAASABGyWs+AAAABmJLR0T///////8JWPfcAAABPklEQVR42u3cMRLBQBSA4Zc9CgqcALXC4bThBA5gNFyFM+wBVNFqjYTszpfi1Sm++bOv2ETEdNK2pc/T9ny977rCn+fx8rjtc7dMmybnxXy9KncGWGCBBRZYYIEFFlhggQUWWGCBBRZYYIE1/GzSLB0CLLAUCyywwAILLLDAAgsssGyFlcAqnJRiKRZYYIEFFlhggQUWWGDZCsFSLLDAAgsssP4DazQowVIssMACy1ZYG6wP30qxwFIssMACCyywwOr/HAYWWIplKwQLLLDAAgssZyywwAILLLDAqh6We4VgKZatECywFAsssMACCyywwAILLLBshWCBpVhggQUWWGCBBRZYYIFlKwQLLMUCCyywwAILLLBG+T8ZsMBSLFshWIoFFlhg/fp8BhZYigUWWGB9C+t9ggUWWGD5FA44XxBz7mcwZM9VAAAAJXRFWHRkYXRlOmNyZWF0ZQAyMDExLTA5LTAyVDIzOjI5OjIxLTA0OjAwcQbBWgAAACV0RVh0ZGF0ZTptb2RpZnkAMjAxMS0wMi0yOFQyMTo0ODozMS0wNTowMJkeu+wAAABSdEVYdHN2ZzpiYXNlLXVyaQBmaWxlOi8vL2hvbWUvYWovQ29kZS90bS1tYXN0ZXIvZXhhbXBsZXMvZ2VvZ3JhcGh5LWNsYXNzL2ZsYWdzL0ZSQS5zdmen2JoeAAAAAElFTkSuQmCC"}</JSon>
          </LocationInfo>
          <Value>238</Value>
        </BandReport>
      </Report>

-  Converting a dataset to MBTiles and adding overviews :

   ::

      $ gdal_translate my_dataset.tif my_dataset.mbtiles -of MBTILES
      $ gdaladdo -r average my_dataset.mbtiles 2 4 8 16

-  Opening a vector MBTiles:

   ::

      $ ogrinfo /home/even/gdal/data/mvt/out.mbtiles
      INFO: Open of `/home/even/gdal/data/mvt/out.mbtiles'
            using driver `MBTiles' successful.
      Metadata:
        ZOOM_LEVEL=5
        name=out.mbtiles
        description=out.mbtiles
        version=2
        minzoom=0
        maxzoom=5
        center=16.875000,44.951199,5
        bounds=-180.000000,-85.051129,180.000000,83.634101
        type=overlay
        format=pbf
      1: ne_10m_admin_1_states_provinces_shpgeojson (Multi Polygon)

-  Converting a GeoPackage to a Vector tile MBTILES:

   ::

      $ ogr2ogr -f MBTILES target.mbtiles source.gpkg -dsco MAXZOOM=10

See Also
--------

-  `MBTiles specification <https://github.com/mapbox/mbtiles-spec>`__
-  `UTFGrid
   specification <https://github.com/mapbox/utfgrid-spec/blob/master/1.0/utfgrid.md>`__
-  :ref:`Mapbox Vector tiles driver <vector.mvt>`
