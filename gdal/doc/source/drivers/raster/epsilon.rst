.. _raster.epsilon:

================================================================================
Epsilon - Wavelet compressed images
================================================================================

.. shortname:: EPSILON

.. build_dependencies:: epsilon 0.9.1

GDAL can read and write wavelet-compressed
images through the Epsilon library. epsilon 0.9.1 is required.

The driver rely on the Open Source EPSILON library (dual LGPL/GPL
licence v3). In its current state, the driver will only be able to read
images with regular internal tiling.

The EPSILON driver only supports 1 band (grayscale) and 3 bands (RGB)
images

This is mainly intended to be used by the :ref:`raster.rasterlite` driver.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::

Creation options
----------------

-  **TARGET** Target size reduction as a percentage of the original
   (0-100). Defaults to 96

-  **FILTER**. See EPSILON documentation or 'gdalinfo --format EPSILON'
   for full list of filter IDs. Defaults to 'daub97lift'

-  **BLOCKXSIZE**\ =n: Sets tile width, defaults to 256. Power of 2
   between 32 and 1024

-  **BLOCKYSIZE**\ =n: Sets tile height, defaults to 256. Power of 2
   between 32 and 1024

-  **MODE**\ =[NORMAL/OTLPF] : OTLPF is some kind of hack to reduce
   boundary artifacts when image is broken into several tiles. Due to
   mathematical constrains this method can be applied to biorthogonal
   filters only. Defaults to OTLPF

-  **RGB_RESAMPLE**\ =[YES/NO] : Whether RGB buffer must be resampled to
   4:2:0. Defaults to YES

See Also
--------

-  `EPSILON home
   page <http://sourceforge.net/projects/epsilon-project>`__
