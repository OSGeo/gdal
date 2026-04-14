.. _raster.esric:

================================================================================
ESRIC -- Esri Compact Cache
================================================================================

.. shortname:: ESRIC

.. built_in_by_default::

Read and write Esri Compact Cache V2 as a single raster.

.. versionadded:: 3.13

   Write support (CreateCopy) for .tpkx output.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Esri Compact Cache V2
---------------------

A reader for `Esri Compact Cache V2 <https://github.com/Esri/raster-tiles-compactcache>`__.
The cache is stored in multiple files located in a specific folder
structure. From the point of view of this driver the raster is
represented by the file named conf.xml, which resides in the root
folder of the cache.  The exact content of this XML file is not fully
documented by Esri, and is subject to change. This driver uses only
a few of the XML fields, as necessary to read the raster.

Esri Tile Package (.tpkx)
-------------------------

Starting from GDAL 3.8, the driver supports reading the `Esri
Tile Package Specification <https://github.com/Esri/tile-package-spec>`__.
A tile package is a compressed file with ".tpkx" extension.
It has a simplified structure, containing image tiles stored in
Compact Cache V2 storage format and tiling scheme and other
metadata stored in a JSON file.

.. note::

    Tile packages with ".tpk" extension, use compact storage V1
    format for cache tiles. The spec for this package type is not
    available and it is not supported by GDAL.

Starting from GDAL 3.13, the driver also supports writing .tpkx files
using the CreateCopy() API (e.g. with :ref:`gdal_translate`). See
`Creation issues`_ below for details.

Usage examples
--------------

Reading
~~~~~~~

If the /path/Layers contains an Esri Compact Cache in V2 format in
the normal Web Mercator tile grid, this command will copy the level 2
content into a TIF file.

``gdal_translate -outsize 1024 1024 path/Layers/conf.xml output.tif``

To convert a .tpkx file to a GeoTIFF:

``gdal_translate -outsize 1024 1024 /path/to/my.tpkx output.tif``

Writing
~~~~~~~

.. versionadded:: 3.13

To create a .tpkx tile package from a GeoTIFF in EPSG:3857 with PNG tiles:

``gdal_translate input.tif output.tpkx -of ESRIC``

To create a .tpkx with JPEG tiles, custom LOD range, and quality setting:

``gdal_translate input.tif output.tpkx -of ESRIC -co TILE_FORMAT=JPEG -co QUALITY=85 -co MIN_LOD=0 -co MAX_LOD=5``

To create a .tpkx with package metadata:

``gdal_translate input.tif output.tpkx -of ESRIC -co SUMMARY="My tile cache" -co TAGS="tag1,tag2"``

Features and Limitations
------------------------

-  Only V2 Compact cache is supported.  This format is identified by
   the value **esriMapCacheStorageModeCompactV2** in the
   **CacheInfo.TileCacheInfo.StorageFormat** element of the conf.xml
   file. Other legacy Esri cache formats are not supported.

-  Only caches with tiles of 256 by 256 pixels are supported. These
   are stored as the values of the
   **CacheInfo.TileCacheInfo.TileRows** and
   **CacheInfo.TileCacheInfo.TileCols** elements.

-  Tiles in the cache can be in different formats, including JPEG
   and PNG. The most common are 8 bit color or grayscale JPEG and
   color PNG with or without opacity. These caches will have the
   value **JPEG**, **PNG** or **MIXED** in the
   **CacheInfo.TileImageInfo.CacheTileFormat** element.
   Other tile formats, such as **PNG8** are not supported.
   The raster will be promoted to RGB or RGBA by this driver, even
   if the source is grayscale.

-  Only caches with 128 by 128 tiles in a bundle file are supported.
   This value, the linear number of tiles in a bundle, is
   contained in the **CacheInfo.CacheStorageInfo.PacketSize**
   element.

-  The spatial reference is obtained from the
   **CacheInfo.TileCacheInfo.SpatialReference.WKT** field. If this
   field is not present, the default value of EPSG:4326 is assumed.

-  As defined in the standard conf.xml file, the cache tile grid
   has an origin location, stored in the
   **CacheInfo.TileCacheInfo.TileOrigin** element, which has X
   and Y components. The cache size is not explicitly defined.
   The ESRIC format driver will assume a symmetric area around the
   0,0 coordinates, in the reference system coordinates. This is
   true for the standard Web Mercator and GCS reference systems.
   If this assumption is not valid, the conf.xml file can be
   modified, adding a non-standard
   **CacheInfo.TileCacheInfo.TileEnd** element with the
   appropriate values for the X and Y components, similar to the
   **TileOrigin**. Note that the grid origin and size is different
   from the raster data extent. For example, it is common to
   generate a cache in the standard Web Mercator tile grid, which is
   defined for the whole Earth, even if data is only available for a
   small region of the globe.

-  A bundled cache has multiple resolution levels, encoded in a
   series of **CacheInfo.TileCacheInfo.LODInfos.LODInfo** nodes.
   The **LODInfo** XML nodes are identified by the **LevelID**
   value, which have to be successive, starting with 0 for the level
   with the largest value for the **Resolution** and increasing
   towards the **LODInfo** node with smallest **Resolution** value.
   The level convention is similar to other Level, Row and Column tile
   addressing schemes used in WMTS for example. While common, it is
   not a requirement to have the resolutions of two successive
   levels differ by a factor of two. Each cache level will be read
   as an overview, at the appropriate resolution.
   The resolution values are in spatial reference distance
   units per pixel.

-  Many caches are built level by level from different sources,
   similar to many web map tile protocols supported by the GDAL WMS
   driver. This means that the content of a specific level might be
   from a different source than an adjacent one, or content for a
   specific level can be missing altogether. This driver will return
   opaque black when reading areas which do not have tiles in the
   cache at a given resolution level, even if data does exist at
   other levels at the same location.

-  Starting from GDAL 3.8, the driver can automatically expand
   the paletted images to RGBA. The same cache may contain tiles with
   different color representations.

-  A cache can exceed the maximum size supported by GDAL, which
   is INT32_MAX, in either dimension. By default, the driver will
   return an error when opening such caches unless the following open 
   option is specified: ``IGNORE_OVERSIZED_LODS=YES``.

Creation issues
---------------

.. versionadded:: 3.13

The CreateCopy() API can be used to create Esri Tile Package (.tpkx) files.
Only .tpkx output is supported; writing directly to Compact Cache V2
exploded cache directory structures (conf.xml) is not supported.

The source dataset must be in EPSG:3857 (Web Mercator) or EPSG:4326
(WGS 84 Geographic). The tiling scheme used is
GoogleMapsCompatible for EPSG:3857 sources and WorldCRS84Quad for
EPSG:4326 sources.

The source must have 1 to 4 bands.
Paletted 1-band sources are automatically expanded to RGB (for JPEG
tile format) or RGBA (for PNG tile format).

Tile formats
~~~~~~~~~~~~

Tiles are encoded as JPEG or PNG. When using JPEG, tiles have 3 bands
(RGB) and the alpha channel is not preserved. Transparent areas are
filled with the source nodata value, or with the value 253 if no nodata
is defined. When using PNG, tiles have 4 bands (RGBA).

LOD levels
~~~~~~~~~~

LOD (Level of Detail) levels range from 0 (coarsest) to 23 (finest).
If :co:`MAX_LOD` exceeds the finest level of detail that the source
resolution can meaningfully populate, a warning is issued and tiles
beyond that level will be upsampled.

Bilinear resampling is used when warping source data to the target
tiling grid. When the source resolution exactly matches the target LOD
resolution, nearest neighbor resampling is used instead.

Source dataset overviews, when available, are used for efficient
multi-LOD creation. For each LOD, the coarsest source overview whose
resolution does not exceed the target LOD resolution is selected.

Output format
~~~~~~~~~~~~~

Tiles are stored in Compact Cache V2 bundle files (128 x 128 tiles per
bundle). The .tpkx file is an uncompressed ZIP archive containing the
bundle files under a ``tile/`` directory, along with ``root.json``
and ``iteminfo.json`` metadata.

Creation options
----------------

|about-creation-options|
The following creation options are available:

-  .. co:: TILE_FORMAT
      :choices: JPEG, PNG
      :default: PNG
      :since: 3.13

      Format used to encode tiles. Default is PNG.

-  .. co:: QUALITY
      :choices: 1-100
      :default: 75
      :since: 3.13

      JPEG compression quality. Only used when :co:`TILE_FORMAT` is JPEG.

-  .. co:: MIN_LOD
      :choices: 0-23
      :default: 0
      :since: 3.13

      Minimum level of detail to generate.

-  .. co:: MAX_LOD
      :choices: 0-23
      :default: 1
      :since: 3.13

      Maximum level of detail to generate. A warning is emitted if this
      exceeds the finest LOD supported by the source resolution.

-  .. co:: SUMMARY
      :since: 3.13

      Package summary stored in the ``iteminfo.json`` package metadata.

-  .. co:: TAGS
      :since: 3.13

      Comma-separated user tags stored in the ``iteminfo.json`` package metadata.

Open options
------------

|about-open-options|
The following open options are available:

-  .. oo:: EXTENT_SOURCE
      :choices: FULL_EXTENT, INITIAL_EXTENT, TILING_SCHEME
      :default: FULL_EXTENT
      :since: 3.9.1

      Which source should be used for the extent of Esri Tile Package (.tpkx) datasets.
      By default, or if specifying ``FULL_EXTENT``, the "fullExtent" element of
      the root.json file will be selected, which normally covers all tiles with data.
      ``INITIAL_EXTENT`` can be specified to use the default initial extent described
      in the "initialExtent" element of the root.json file.
      ``TILING_SCHEME`` will return the extent of the whole tiling scheme, which
      is typically world coverage.

-  .. oo:: IGNORE_OVERSIZED_LODS
      :choices: YES, NO
      :default: NO
      :since: 3.13

      Whether to ignore LODs whose computed raster size exceeds the maximum size
      supported by GDAL (INT32_MAX). By default, an error will be returned when attempting
      to open such datasets. When set to ``YES``, such LODs are ignored.

See Also
--------
-  Implemented as :source_file:`frmts/esric/esric_dataset.cpp`.
