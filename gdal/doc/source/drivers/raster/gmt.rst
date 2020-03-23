.. _raster.gmt:

================================================================================
GMT -- GMT Compatible netCDF
================================================================================

.. shortname:: GMT

.. build_dependencies:: libnetcdf

GDAL has limited support for reading and writing netCDF *grid* files.
NetCDF files that are not recognised as grids (they lack variables
called dimension, and z) will be silently ignored by this driver. This
driver is primarily intended to provide a mechanism for grid interchange
with the `GMT <http://gmt.soest.hawaii.edu/>`__ package. The netCDF
driver should be used for more general netCDF datasets.

The units information in the file will be ignored, but x_range, and
y_range information will be read to get georeferenced extents of the
raster. All netCDF data types should be supported for reading.

Newly created files (with a type of ``GMT``) will always have units of
"meters" for x, y and z but the x_range, y_range and z_range should be
correct. Note that netCDF does not have an unsigned byte data type, so
8bit rasters will generally need to be converted to Int16 for export to
GMT.

NetCDF support in GDAL is optional, and not compiled in by default.

NOTE: Implemented as ``gdal/frmts/netcdf/gmtdataset.cpp``.

See Also: `Unidata NetCDF
Page <http://www.unidata.ucar.edu/software/netcdf/>`__

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::
