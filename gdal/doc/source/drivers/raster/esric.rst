.. _raster.esric:

================================================================================
ESRIC -- Esri Compact Cache
================================================================================

.. shortname:: ESRIC

Read Esri Compact Cache V2 as a single raster

Driver capabilities
-------------------

.. supports_virtualio::

.. supports_georeferencing::

Esri Compact Cache V2
---------------------

The Esri Compact Cache V2 is a tile cache storage format, with the tile storage structure described in 
https://github.com/Esri/raster-tiles-compactcache

The raster is stored in a specific folder structure, from the point of view of this driver the raster is represented by the file named conf.xml, which resides in the root folder of the cache.  The exact content of this XML file is not fully documented by Esri, and is subject to change.  This driver only uses a few of the XML fields, as necessary to read the raster.

Usage examples
______________

If the /path/Layers contains an Esri Compact Cache in V2 format in the normal Web Mercator tile grid, this command will copy the level 2 content into a TIF file.

``gdal_translate -outsize 1024 1024 path/Layers/conf.xml output.tif``

Features and Limitations
________________________

- Only V2 Compact cache is supported.  This format is identified by the value **esriMapCacheStorageModeCompactV2** in the **CacheInfo.TileCacheInfo.StorageFormat** element of the conf.xml file.  Other legacy Esri cache formats are not supported.

- Only caches with tiles of 256 by 256 pixels are supported. These are the values of the **CacheInfo.TileCacheInfo.TileRows** and **CacheInfo.TileCacheInfo.TileCols** elements.

- Tiles in the cache can be in different formats, including JPEG and PNG. The most common are 8 bit color or grayscale JPEG and color PNG with or without opacity. These caches will have the value **JPEG**, **PNG** or **MIXED** in the **CacheInfo.TileImageInfo.CacheTileFormat** element.  Other tile formats, such as **PNG8** are not supported at this time.  The raster will be promoted to RGB or RGBA by this driver, even if the source is grayscale.

- Only caches with 128 by 128 tiles in a bundle file are supported.  The linear number of tiles in a bundle is the value contained in the **CacheInfo.CacheStorageInfo.PacketSize** element.

- The spatial reference is obtained from the **CacheInfo.TileCacheInfo.SpatialReference.WKT** field. If this field is not present, the default value of EPSG:4326 is assumed.

- As defined in the standard conf.xml file, the cache tile grid has an origin, stored in the **CacheInfo.TileCacheInfo.TileOrigin** element, which has an X and Y component.  However, the cache size is not explicilty defined.  The driver will assume a symetric area around the 0,0 coordinates, in any reference system. This is correct for the standard Web Mercator and GCS reference systems.  If this assumption is not valid, the conf.xml file needs to have a non-standard **CacheInfo.TileCacheInfo.TileEnd** element with the appropriate values for the X and Y components, similar to the **TileOrigin**.  Note that the grid origin and size is different from the raster data extent. For example, it is common to generate a cache in the standard Web Mercator tile grid, which is defined for the whole Earth, even if data is only available for a small region of the globe.

- A bundled cache can have multiple resolution levels, encoded in a series of **CacheInfo.TileCacheInfo.LODInfos.LODInfo** elements. The **LODInfo** XML nodes are identified by the **LevelID** value, which should be successive, starting with 0 for the level with the largest value for the **Resolution** and increasing towards the smallest resolution value.  The **Resolution** values are in spatial reference units per pixel. This is similar to other Level/Row/Column tile addressing schemes used elsewhere.  While common, it is not a requirement to have the resolutions of two successive levels differ by a factor of two.  Each cache level will be read as an overview by GDAL, at the appropriate resolution.

- Many caches are built level by level from different sources, similar to many web map tile protocols supported by the GDAL WMS driver.  This means that the content of a specific level might be from a different source than another, or be missing altogether.  This driver will return opaque black when reading areas which do not have tiles in the cache.

- A cache might be very large, exceeding the maximum size supported by GDAL, which is INT32_MAX, in either dimension. The ESRIC driver will refuse to open such caches.  Eliminating the **LODInfo** nodes with the highest **LevelID** from the conf.xml file until the computed raster size drops below INT32_MAX will allow this driver to read the cache, ignoring the high resolution levels.

