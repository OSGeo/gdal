.. _raster.esric:

================================================================================
ESRIC -- Esri Compact Cache
================================================================================

.. shortname:: ESRIC

.. built_in_by_default::

Read Esri Compact Cache V2 as a single raster

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Esri Compact Cache V2
---------------------

A reader for `Esri
Compact
Cache
V2 <https://github.com/Esri/raster-tiles-compactcache>`__.
The cache is stored in multiple files located in a specific folder
structure. From the point of view of this driver the raster is
represented by the file named conf.xml, which resides in the root
folder of the cache.  The exact content of this XML file is not fully
documented by Esri, and is subject to change. This driver uses only
a few of the XML fields, as necessary to read the raster.

Usage examples
______________

If the /path/Layers contains an Esri Compact Cache in V2 format in
the normal Web Mercator tile grid, this command will copy the level 2
content into a TIF file.

``gdal_translate -outsize 1024 1024 path/Layers/conf.xml output.tif``

Features and Limitations
________________________

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

-  A cache can exceede the maximum size supported by GDAL, which
   is INT32_MAX, in either dimension. This driver will generate
   an error when opening such caches. Removing the
   **LODInfo** nodes with the highest **LevelID** from the conf.xml
   file until the raster size drops below INT32_MAX is a possible
   workaround, but the highest resolution levels will not be read.

See Also
--------
-  Implemented as ``gdal/frmts/esric/esric_dataset.cpp``.
