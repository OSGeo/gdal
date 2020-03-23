.. _raster.rmf:

================================================================================
RMF -- Raster Matrix Format
================================================================================

.. shortname:: RMF

.. built_in_by_default::

RMF is a simple tiled raster format used in the GIS "Integration" and
"Panorama" GIS. The format itself has very poor capabilities.

There are two flavors of RMF called MTW and RSW. MTW supports 16-bit
integer and 32/64-bit floating point data in a single channel and aimed
to store DEM data. RSW is a general purpose raster. It supports single
channel colormapped or three channel RGB images. Only 8-bit data can be
stored in RSW. Simple georeferencing can be provided for both image
types.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Metadata
--------

-  **ELEVATION_MINIMUM**: Minimum elevation value (MTW only).
-  **ELEVATION_MAXIMUM**: Maximum elevation value (MTW only).
-  **ELEVATION_UNITS**: Name of the units for raster values (MTW only).
   Can be "m" (meters), "cm" (centimeters), "dm" (decimeters), "mm"
   (millimeters).
-  **ELEVATION_TYPE**: Could be either 0 (absolute elevation) or 1
   (total elevation). MTW only.

Open Options
------------
- **RMF_SET_VERTCS**: set to ON, the layers spatial reference
   will include vertical coordinate system description if exist.
   This feature can be enabled via config option with same name.

Creation Options
----------------

-  **MTW=ON**: Force the generation of MTW matrix (RSW will be created
   by default).
-  **BLOCKXSIZE=n**: Sets tile width, defaults to 256.
-  **BLOCKYSIZE=n**: Set tile height. Tile height defaults to 256.
-  **RMFHUGE=NO/YES/IF_SAFER**: Creation of huge RMF file (Supported by
   GIS Panorama since v11). Defaults to NO.
-  **COMPRESS=NONE/LZW/JPEG/RMF_DEM**: (From GDAL 2.4) Compression type.
   Defaults to NONE. Note: JPEG compression supported only with RGB
   (3-band) Byte datasets. RMF_DEM compression supported only with Int32
   one channel MTW datasets.
-  **JPEG_QUALITY**: (From GDAL 2.4) JPEG quality 1-100. Defaults to 75.
-  **NUM_THREADS=number_of_threads/ALL_CPUS**: (From GDAL 2.4) Enable
   multi-threaded compression by specifying the number of worker
   threads. Default is compression in the main thread.

See Also:
---------

-  Implemented as ``gdal/frmts/rmf/rmfdataset.cpp``.
-  `"Panorama" GIS homepage <http://www.gisinfo.ru/index_en.htm>`__
