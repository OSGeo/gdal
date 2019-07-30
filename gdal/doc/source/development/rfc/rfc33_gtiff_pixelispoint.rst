.. _rfc-33:

================================================================================
RFC 33: GTiff - Fixing PixelIsPoint Interpretation
================================================================================

Authors: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

This document proposes changes in the GDAL GTiff (GeoTIFF) driver's
interpretation of PixelIsPoint when constructing the geotransform and
interpreting control points. An RFC is used due to the fundamental role
of GeoTIFF in GDAL and the GDAL user community and the risk for
significant backward compatibility problems with this adjustment.

Rationale
---------

The GeoTIFF specification includes a data item, GTRasterTypeGeoKey,
which may be set to either RasterPixelIsArea (the default), or
RasterPixelIsPoint. RasterPixelIsArea defines that a pixel represents an
area in the real world, while RasterPixelIsPoint defines a pixel to
represent a point in the real world. Often this is useful to distinguish
the behavior of optical sensors that average light values over an area
vs. raster data which is point oriented like an elevation sample at a
point.

Traditionally GDAL has treated this flag as having no relevance to the
georeferencing of the image despite disputes from a variety of other
software developers and data producers. This was based on the authors
interpretation of something said once by the GeoTIFF author. However, a
recent review of section [`section
2.5.2.2 <http://www.remotesensing.org/geotiff/spec/geotiff2.5.html#2.5.2.2>`__]
of the GeoTIFF specificaiton has made it clear that GDAL behavior is
incorrect and that PixelIsPoint georeferencing needs to be offset by a
half a pixel when transformed to the GDAL georeferencing model. This
issue is documented in the following tickets including #3837, #3838,
....

This RFC attempts to manage this transition with a minimum of disruption
for the users of GDAL/OGR.

Planned Changes
---------------

Interpretation of the raster space from the GeoTIFF tie points will be
offset by half a pixel in the PixelIsPoint case in
gdal/frmts/gtiff/geotiff.cpp. This will impact the formation of the
geotransform and the formation of GCPs when there are multiple tie
points. geotransmatrix conversion to geotransform will also be affected.

Conversely if writing files with PixelIsPoint (as driven by the
"AREA_OR_POINT" metadata item being set to "POINT") the written raster
space coordinates would be offset by half a pixel.

In trunk the above behavior may be disabled by setting the
GTIFF_POINT_GEO_IGNORE configuration option to TRUE (it will default to
FALSE).

In GDAL 1.7 and 1.6 branch the same changes will be applied, except the
GTIFF_POINT_GEO_IGNORE configuration option will default to TRUE.

Compatibility Issues
--------------------

This change will alter the apparent georeferencing of all GeoTIFF files
with PixelIsPoint set. It is not clear how large a proportion of GeoTIFF
files this will apply to, but it is significant. This isn't too bad for
files coming from non-GDAL sources as most other produces have made the
correct interpretation of PixelIsPoint for years. However,
unfortunately, files produced in the past by GDAL with PixelIsPoint will
now be interpreted differently and the values will be off by half a
pixel.

In practice it was not particularly convenient or well documented how to
produce PixelIsPoint GeoTIFF files with GDAL, so these files should be
fairly rare. Thee easiest way to produce them was by copying from
another PixelIsPoint GeoTIFF file in which the error on write just undid
the error when reading the source GeoTIFF file.

Reporting Extents
-----------------

Folks have at various points in the past requested that we report the
extents differently for files with an AREA_OR_POINT value of POINT, much
as listgeo does for GeoTIFF files that have a PixelIsPoint
interpretation. I do *not* plan to do this, and for the purpose of GDAL
the GCPs, RPCs and GeoTransform will always be based on an area
interpretation of pixels. The AREA_OR_POINT will *only* be used to
control setting of the PixelIsPoint value in GeoTIFF files, and as
metadata about the physical interpretation of pixels.

World Files
-----------

These changes will have no impact on how world files are treated or
written. They are always based on the assumption of a area based pixel,
but with the origin at the center of the top left pixel. This is
effectively the same as the values for PixelIsPoint, but is not in any
way tried to this metadata.

Test Suite
----------

The 1.6 and 1.7 branch test suites will not be altered.

The trunk branch test suite will be altered to check for the updated
values and will be extended with a test to confirm that setting the
config option GTIFF_POINT_GEO_IGNORE to TRUE suppresses the altered
behavior.

Documentation
-------------

The situation will be noted in the 1.8.0, 1.7.4 and 1.6.4 release notes
as well as in the GeoTIFF driver documentation in trunk.

The GeoTIFF web site GeoTIFF FAQ will be updated to clarify the
interpretation of PixelIsPoint and note that up to GDAL 1.8 it was
improperly interpreted by GDAL.

Implementation
--------------

All code implementation will be by Frank Warmerdam in the next few
weeks.

Applied in trunk (r21158), 1.7 (r21159), 1.6 (r21160) and 1.6-esri
(r21161).
