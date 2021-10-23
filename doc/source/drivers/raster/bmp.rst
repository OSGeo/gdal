.. _raster.bmp:

================================================================================
BMP -- Microsoft Windows Device Independent Bitmap
================================================================================

.. shortname:: BMP

.. built_in_by_default::

MS Windows Device Independent Bitmaps supported by the Windows kernel
and mostly used for storing system decoration images. Due to the nature
of the BMP format it has several restrictions and could not be used for
general image storing. In particular, you can create only 1-bit
monochrome, 8-bit pseudocoloured and 24-bit RGB images only. Even
grayscale images must be saved in pseudocolour form.

This driver supports reading almost any type of the BMP files and could
write ones which should be supported on any Windows system. Only single-
or three- band files could be saved in BMP file. Input values will be
resampled to 8 bit.

If an ESRI world file exists with the .bpw, .bmpw or .wld extension, it
will be read and used to establish the geotransform for the image.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Options
----------------

-  **WORLDFILE=YES**: Force the generation of an associated ESRI world
   file (with the extension .wld).

See Also
--------

-  Implemented as ``gdal/frmts/bmp/bmpdataset.cpp``.
-  `Wikipedia BMP file
   format <https://en.wikipedia.org/wiki/BMP_file_format>`__
