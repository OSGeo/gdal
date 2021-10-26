.. _raster.aaigrid:

================================================================================
AAIGrid -- Arc/Info ASCII Grid
================================================================================

.. shortname:: AAIGrid

.. built_in_by_default::

Supported for read and write access, including reading of an affine
georeferencing transform and some projections. This format is the ASCII
interchange format for Arc/Info Grid, and takes the form of an ASCII
file, plus sometimes an associated .prj file. It is normally produced
with the Arc/Info ASCIIGRID command.

The projections support (read if a \*.prj file is available) is quite
limited. Additional sample .prj files may be sent to the maintainer,
warmerdam@pobox.com.

The NODATA value for the grid read is also preserved when available in
the same format as the band data.

By default, the datatype returned for AAIGRID datasets by GDAL is
autodetected, and set to Float32 for grid with floating point values or
Int32 otherwise. This is done by analysing the format of the NODATA
value and, if needed, the data of the grid. You can
explicitly specify the datatype by setting the AAIGRID_DATATYPE
configuration option (Int32, Float32 and Float64 values are supported
currently)

If pixels being written are not square (the width and height of a pixel
in georeferenced units differ) then DX and DY parameters will be output
instead of CELLSIZE. Such files can be used in Golden Surfer, but not
most other ascii grid reading programs. For force the X pixel size to be
used as CELLSIZE use the FORCE_CELLSIZE=YES creation option or resample
the input to have square pixels.

When writing floating-point values, the driver uses the "%.20g" format
pattern as a default. You can consult a `reference
manual <http://en.wikipedia.org/wiki/Printf>`__ for printf to have an
idea of the exact behavior of this ;-). You can alternatively specify
the number of decimal places with the DECIMAL_PRECISION creation option.
For example, DECIMAL_PRECISION=3 will output numbers with 3 decimal
places(using %lf format). Another option is
SIGNIFICANT_DIGITS=3, which will output 3 significant digits (using %g
format).

The :ref:`raster.aig` driver is also available for Arc/Info Binary Grid
format.

NOTE: Implemented as ``gdal/frmts/aaigrid/aaigriddataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::
