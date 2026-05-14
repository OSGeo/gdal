.. _raster.pmtiles:

================================================================================
PMTiles
================================================================================

.. versionadded:: 3.14

.. shortname:: PMTiles

.. built_in_by_default::

This driver supports reading and writing `PMTiles <https://github.com/protomaps/PMTiles>`__
datasets containing raster tiles, encoded in PNG, JPEG or WEBP format.

PMTiles is a single-file archive format for tiled data. A PMTiles archive can
be hosted on a commodity storage platform such as S3, and enables low-cost,
zero-maintenance map applications that are "serverless" - free of a custom tile
backend or third party provider.

This driver is compatible with all GDAL
:ref:`network-based virtual file systems <network_based_file_systems>`

The SRS is always the `Pseudo-Mercator <https://en.wikipedia.org/wiki/Web_Mercator_projection>`__
(a.k.a Google Mercator) projection, EPSG:3857.

The supported datasets must contain a JSON metadata document
following the
`MBTiles specification <https://github.com/mapbox/mbtiles-spec/blob/master/1.3/spec.md#vector-tileset-metadata>`__.

The driver also supports vector layers. See :ref:`PMTiles vector driver <vector.pmtiles>`

The driver requires at runtime the availability of the :ref:`raster.gti` and
:ref:`vector.gpkg` drivers for read support, and :ref:`raster.mbtiles` for
creation support. For both read and creation, it also requires the :ref:`raster.png`, :ref:`raster.jpeg` or
:ref:`raster.webp` driver depending on the tile format of the dataset.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Opening options
---------------

|about-open-options|
The following open options are available:

-  .. oo:: ZOOM_LEVEL
     :choices: <integer>

     Zoom level to use. Must be between the minimum and maximum zoom levels
     reported in the metadata. Defaults to the maximum zoom level available in
     the dataset.

Creation issues
---------------

The driver supports conversion from another GDAL supported raster format into
PMTiles.

Tiles are generated with WebMercator (EPSG:3857) projection.

Internally the source dataset is converted into a temporary MBTiles dataset, hence
both drivers supports the same raster creation options.

The driver implements also a direct translation mode when using :program:`gdal raster convert`
with a MBTiles raster dataset as input and a PMTiles output dataset, without
any creation option: ``gdal raster convert in.mbtile out.pmtiles``. In that mode, existing
PNG/JPEG/WEBP tiles from the MBTiles files are used as such, contrary to the general writing
mode that may involve reprojection and resampling.

Creation options
----------------

|about-dataset-creation-options|
The following dataset creation options are supported:

-  .. co:: NAME

      Tileset name. Defaults to the basename of the
      output file/directory. Used to fill metadata records.

-  .. co:: DESCRIPTION

      A description of the tileset. Used to fill metadata records.

-  .. co:: TYPE
      :choices: overlay, baselayer

      Layer type. Used to fill metadata records.


-  .. co:: VERSION
     :default: 1.1

     The version of the tileset, as a dotted notation
     number, used to set the 'version' metadata item.

-  .. co:: BLOCKSIZE
     :choices: <integer>
     :default: 256
     :since: 2.3

     Block/tile size in width
     and height in pixels. Maximum supported size is 4096.

-  .. co:: TILE_FORMAT
     :choices: PNG, PNG8, JPEG, WEBP
     :default: PNG

     Format used to store tiles. See :ref:`mbtiles_tile_formats`.

-  .. co:: QUALITY
     :choices: 1-100
     :default: 75

     Quality setting for JPEG/WEBP compression.

-  .. co:: ZLEVEL
     :choices: 1-9
     :default: 6

     DEFLATE compression level for PNG tiles.

-  .. co:: DITHER
     :choices: YES, NO
     :default: NO

     Whether to use Floyd-Steinberg dithering (for
     :co:`TILE_FORMAT=PNG8`).

-  .. co:: ZOOM_LEVEL_STRATEGY
     :choices: AUTO, LOWER, UPPER
     :default: AUTO

     Strategy to determine
     zoom level. LOWER will select the zoom level immediately below the
     theoretical computed non-integral zoom level, leading to
     subsampling. On the contrary, UPPER will select the immediately
     above zoom level, leading to oversampling. Defaults to AUTO which
     selects the closest zoom level.

-  .. co:: RESAMPLING
     :choices: NEAREST, BILINEAR, CUBIC, CUBICSPLINE, LANCZOS, MODE, AVERAGE
     :default: BILINEAR

     Resampling algorithm.

-  .. co:: WRITE_BOUNDS
     :choices: YES, NO
     :default: YES

     Whether to write the bounds 'metadata' item.

-  .. co:: ELEVATION_TYPE
     :choices: '', 'terrain-rgb'
     :default: ''

     Type of elevation encoding, suitable for direct generation of
     Mapbox Terrain-RGB tilesets. See `Mapbox Terrain-RGB v1
     <https://docs.mapbox.com/data/tilesets/reference/mapbox-terrain-rgb-v1/>`__

/vsipmtiles/ virtual file system
--------------------------------

See :ref:`vsipmtiles`.

Examples
--------

.. example::
   :title: Simple translation of a GeoTIFF into PMTiles using default PNG tile format

   .. code-block:: bash

      gdal raster convert in.tif out.pmtiles

See Also
--------

-  :ref:`PMTiles vector driver <vector.pmtiles>`
-  `PMTiles specification <https://github.com/protomaps/PMTiles>`__
-  :ref:`MBTiles driver <raster.mbtiles>`
