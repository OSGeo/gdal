.. _raster.mem:

================================================================================
MEM -- In Memory Raster
================================================================================

.. shortname:: MEM

.. built_in_by_default::

GDAL supports the ability to hold rasters in a temporary in-memory
format. This is primarily useful for temporary datasets in scripts or
internal to applications. It is not generally of any use to application
end-users.

Memory datasets should support for most kinds of auxiliary information
including metadata, coordinate systems, georeferencing, GCPs, color
interpretation, nodata, color tables and all pixel data types.

Dataset Name Format
-------------------

It is possible to open an existing array in memory. To do so, construct
a dataset name with the following format:

::

     MEM:::option=value[,option=value...]

For example:

::

     MEM:::DATAPOINTER=342343408,PIXELS=100,LINES=100,BANDS=3,DATATYPE=Byte,
          PIXELOFFSET=3,LINEOFFSET=300,BANDOFFSET=1,
          GEOTRANSFORM=1.166396e+02/1.861068e-05/0.000000e+00/3.627969e+01/0.000000e+00/-1.861068e-05

or

::

     MEM:::DATAPOINTER=0x1467BEF0,PIXELS=100,LINES=100,BANDS=3,DATATYPE=Byte,
          PIXELOFFSET=3,LINEOFFSET=300,BANDOFFSET=1,
          GEOTRANSFORM=1.166396e+02/1.861068e-05/0.000000e+00/3.627969e+01/0.000000e+00/-1.861068e-05

-  DATAPOINTER: address of the first pixel of the first band. The
   address can be represented as a hexadecimal or decimal value.
   Hexadecimal values must be prefixed with '0x'. Some implementations
   (notably Windows) doesn't print hexadecimal pointer values with a
   leading '0x', so the prefix must be added. You can use
   CPLPrintPointer to create a string with format suitable for use as a
   DATAPOINTER.
-  PIXELS: Width of raster in pixels. (required)
-  LINES: Height of raster in lines. (required)
-  BANDS: Number of bands, defaults to 1. (optional)
-  DATATYPE: Name of the data type, as returned by GDALGetDataTypeName()
   (eg. Byte, Int16) Defaults to Byte. (optional)
-  PIXELOFFSET: Offset in bytes between the start of one pixel and the
   next on the same scanline. (optional)
-  LINEOFFSET: Offset in bytes between the start of one scanline and the
   next. (optional)
-  BANDOFFSET: Offset in bytes between the start of one bands data and
   the next.
-  GEOTRANSFORM: Set the affine transformation coefficients. 6 real
   numbers with '/' as separator (optional)

Creation Options
----------------

There are no supported creation options.

The MEM format is one of the few that supports the AddBand() method. The
AddBand() method supports DATAPOINTER, PIXELOFFSET and LINEOFFSET
options to reference an existing memory array.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

Multidimensional API support
----------------------------

.. versionadded:: 3.1

The MEM driver supports the :ref:`multidim_raster_data_model`.
