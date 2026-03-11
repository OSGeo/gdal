.. _raster.mem:

================================================================================
MEM -- In Memory datasets
================================================================================

.. shortname:: MEM

.. built_in_by_default::

GDAL supports the ability to hold datasets in a temporary in-memory
format. This is primarily useful for temporary datasets in scripts or
internal to applications. It is not generally of any use to application
end-users.

This page documents its raster capabilities. Starting with GDAL 3.11, this
driver has been unified with the long-time existing Memory driver to offer
both raster and :ref:`vector <vector.mem>` capabilities.

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
-  SPATIALREFERENCE: (GDAL >= 3.7) Set the projection. The coordinate reference
   systems that can be passed are anything supported by the
   OGRSpatialReference.SetFromUserInput() as per '-a_srs' in
   :ref:`gdal_translate`. If the passed string includes comma or double-quote characters (typically WKT),
   it should be surrounded by double-quote characters and the double-quote characters inside it
   should be escaped with anti-slash.
   e.g ``SPATIALREFERENCE="GEOGCRS[\"WGS 84\",[... snip ...],ID[\"EPSG\",4326]]"``

.. warning::

    Starting with GDAL 3.10, opening a MEM dataset using the above syntax is no
    longer enabled by default for security reasons.
    If you want to allow it, define the ``GDAL_MEM_ENABLE_OPEN`` configuration
    option to ``YES``, or build GDAL with the ``GDAL_MEM_ENABLE_OPEN`` compilation
    definition.

    .. config:: GDAL_MEM_ENABLE_OPEN
       :choices: YES, NO
       :default: NO
       :since: 3.10

       Whether opening a MEM dataset with the ``MEM:::`` syntax is allowed.


Creation Options
----------------

|about-creation-options|
This driver supports the following creation option:

-  .. co:: INTERLEAVE
      :choices: BAND, PIXEL
      :default: BAND

      Set the interleaving to use

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
