.. _raster.blx:

================================================================================
BLX -- Magellan BLX Topo File Format
================================================================================

.. shortname:: BLX

.. built_in_by_default::

BLX is the format for storing topographic data in Magellan GPS units.
This driver supports both reading and writing. In addition the 4
overview levels inherent in the BLX format can be used with the driver.

The BLX format is tile based, for the moment the tile size is fixed to
128x128 size. Furthermore the dimensions must be a multiple of the tile
size.

The data type is fixed to Int16 and the value for undefined values is
fixed to -32768. In the BLX format undefined values are only really
supported on tile level. For undefined pixels in non-empty tiles see the
FILLUNDEF/FILLUNDEFVAL options.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Georeferencing
--------------

The BLX projection is fixed to WGS84 and georeferencing from BLX is
supported in the form of one tiepoint and pixelsize.

Creation Issues
---------------

Creation Options:

-  **ZSCALE=1**: Set the desired quantization increment for write
   access. A higher value will result in better compression and lower
   vertical resolution.
-  **BIGENDIAN=YES**: If BIGENDIAN is defined, the output file will be
   in XLB format (big endian blx).
-  **FILLUNDEF=YES**: If FILLUNDEF is yes the value of FILLUNDEFVAL will
   be used instead of -32768 for non-empty tiles. This is needed since
   the BLX format only support undefined values for full tiles, not
   individual pixels.
-  **FILLUNDEFVAL=0**: See FILLUNDEF
