.. _raster.webp:

================================================================================
WEBP - WEBP
================================================================================

.. shortname:: WEBP

.. build_dependencies:: libwebp

GDAL can read and write WebP images through
the WebP library.

WebP is a new image format that provides lossy compression for
photographic images. A WebP file consists of VP8 image data, and a
container based on RIFF.

The driver rely on the Open Source WebP library (BSD licensed). The WebP
library (at least in its version 0.1) only offers compression and
decompression of whole images, so RAM might be a limitation when dealing
with big images (which are limited to 16383x16383 pixels).

The WEBP driver supports 3 bands (RGB) images. It also supports 4 bands (RGBA)

The WEBP driver can be used as the internal format used by the
:ref:`raster.rasterlite` driver.

XMP metadata can be extracted from the file,
and will be stored as XML raw content in the xml:XMP metadata domain.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::

Creation options
----------------

Various creation options exists, among them :

-  **QUALITY=n**: By default the quality flag is set to 75, but this
   option can be used to select other values. Values must be in the
   range 1-100. Low values result in higher compression ratios, but
   poorer image quality.

-  **LOSSLESS=True/False** By
   default, lossy compression is used. If set to True, lossless
   compression will be used.

See Also
--------

-  `WebP home page <https://developers.google.com/speed/webp/>`__
