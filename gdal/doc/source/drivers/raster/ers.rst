.. _raster.ers:

ERS -- ERMapper .ERS
====================

GDAL supports reading and writing raster files with .ers header files,
with some limitations. The .ers ascii format is used by ERMapper for
labeling raw data files, as well as for providing extended metadata, and
georeferencing for some other file formats. The .ERS format, or variants
are also used to hold descriptions of ERMapper algorithms, but these are
not supported by GDAL.

Starting with GDAL 1.9.0, the PROJ, DATUM and UNITS values found in the
ERS header are reported in the ERS metadata domain.

Creation Issues
---------------

Creation Options:

-  **PIXELTYPE=value**:.By setting this to SIGNEDBYTE, a new Byte file
   can be forced to be written as signed byte
-  **PROJ=name**: (GDAL >= 1.9.0) Name of the ERS projection string to
   use. Common examples are NUTM11, or GEODETIC. If defined, this will
   override the value computed by SetProjection() or SetGCPs().
-  **DATUM=name**: (GDAL >= 1.9.0) Name of the ERS datum string to use.
   Common examples are WGS84 or NAD83. If defined, this will override
   the value computed by SetProjection() or SetGCPs().
-  **UNITS=name**: (GDAL >= 1.9.0) Name of the ERS projection units to
   use : METERS (default) or FEET (us-foot). If defined, this will
   override the value computed by SetProjection() or SetGCPs().

See Also:
---------

-  Implemented as ``gdal/frmts/ers/ersdataset.cpp``.
