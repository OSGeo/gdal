.. _raster.nwtgrd:

================================================================================
NWT_GRD/NWT_GRC -- Northwood/Vertical Mapper File Format
================================================================================

.. shortname:: NWT_GRD

.. shortname:: NWT_GRC

.. built_in_by_default::

Support for reading & writing Northwood GRID raster formats. This format
is also known as Vertical Mapper Grid or MapInfo Grid and is commonly
used in MapInfo Pro software

Full read/write support of \*.grd (grid) files is available, read-only
support is available for classified grids (\*.grc).

For writing, Float32 is the only supported band type.

Driver capabilities (NWT_GRD)
-----------------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Driver capabilities (NWT_GRC)
-----------------------------

.. supports_georeferencing::

.. supports_virtualio::

Color Information
-----------------

The grid formats have color information embedded in the grid file
header. This describes how to scale data values to RGB values. When
opening in read mode, the driver will report 4 bands - R, G, B and the
data band. In reality there is 1 band and the RGB bands are 'virtual',
made from scaling data. For this reason, when opening in write mode only
1 band is reported and the RGB bands are unavailable.

Metadata
--------

GDAL Metadata items are stored in the PAM .aux.xml file

Northwood Grid itself does not natively support arbitrary metadata

Nodata values
-------------

In write mode, it is possible to designate any value as the nodata
value. These values are translated to the Vertical Mapper no data value
when writing. Therefore, in read mode the nodata value is always
reported as -1e37.

Creation Options
~~~~~~~~~~~~~~~~

-  **ZMIN=-2e37**: Set the minimum Z value. Data are scaled on disk to a
   16 bit integer and the Z value range is used to scale data. If not
   set, it may cause incorrect data to be written when using 'Create()'
   or a full recalculation of the source dataset statistics when using
   'CreateCopy'

-  **ZMAX=2e38**: Set the maximum Z value. Data are scaled on disk to a
   16 bit integer and the Z value range is used to scale data. If not
   set, it may cause incorrect data to be written when using 'Create()'
   or a full recalculation of the source dataset statistics when using
   'CreateCopy'

-  **BRIGHTNESS=50**: Set the brightness level. Only affects opening the
   file in MapInfo/Vertical Mapper

-  **CONTRAST=50**: Set the contrast level. Only affects opening the
   file in MapInfo/Vertical Mapper

-  **TRANSCOLOR=0**: Set a transparent color level. Only affects opening
   the file in MapInfo/Vertical Mapper

-  **TRANSLUCENCY=0**: Set the translucency level. Only affects opening
   the file in MapInfo/Vertical Mapper
