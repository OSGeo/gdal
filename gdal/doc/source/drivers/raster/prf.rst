.. _raster.prf:

================================================================================
PHOTOMOD Raster File
================================================================================

.. shortname:: PRF

.. built_in_by_default::

PRF or MegaTIFF is an internal format of PHOTOMOD software for storing
large images.

This format was developed to store images larger than 4 GB. As a basis
for storing data used TIFF or JPEG2000 format. Raster is split into
fragments (tiles) such that each fragment does not exceeded a predefined
size (e.g., less than 1 GB). An overview file also added to process
raster data on a small scales.

PRF files has two variations: 'prf' for imagery data and 'x-dem' for
elevation data. Files can be georeferenced, but projection information
can be stored only in external files (\*.prj).

Image format has the following structure:

-  the header XML file 'image_name.prf'/'image_name.x-dem'
-  folder 'image_name' with raster subtiles
-  files \*.tif/*.jp2/*.demtif inside folder 'image_name', containing
   raster fragments and the overview image

The driver support the data type among Byte, UInt16, UInt32, Float32 or
Float64.

Driver capabilities
-------------------

.. supports_virtualio::

See Also
--------

-  `Racurs company home page <http://www.racurs.ru>`__
-  `PHOTOMOD Lite home page <http://www.racurs.ru/index.php?page=453>`__
