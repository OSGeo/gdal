.. _raster.cals:

================================================================================
CALS -- CALS Type 1
================================================================================

.. shortname:: CALS

.. versionadded:: 2.1

.. built_in_by_default::

CALS Type 1 rasters are untiled back and white rasters made of a
2048-byte text header in the MIL-STD-1840 standard, followed by a single
datastream compressed with CCITT/ITU-T Recommendation 6, aka Group 6,
aka CCITT FAX 4. CALS Type 1 rasters are one of the 4 types of formats
described by MIL-PRF-28002C (this standard is now deprecated). Other
types are not handled by this driver.

This driver supports reading and creation of CALS Type 1 rasters. Update
of existing rasters is not supported.

A CALS dataset is exposed by the driver as a single-band 1-bit raster
with a 2-entry color table. The first entry (0) is white
(RGB=255,255,255) and the second entry (1) is black (RGB=0,0,0).

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::

Metadata
--------

The following metadata items might be exposed by the driver (or read
from the input dataset to generate a cor

-  **PIXEL_PATH**: First value of the "rorient" header field, measured
   in degrees counterclockwise from the positive horizontal axis (east).
   The pel path value represents the number of degrees the image would
   have to be rotated counterclockwise in order to display the image
   with proper viewing orientation. The permissible values for the pixel
   path direction shall be "0", "90", "180", or "270". 0 is the typical
   value. If PIXEL_PATH=0 and LINE_PROGRESSION=270, neither are
   reported.
-  **LINE_PROGRESSION**: Second value of the "rorient" header field. The
   line progression direction is measured in degrees counterclockwise
   from the pel path direction. The permissible values for the line
   progression direction shall be "90" or "270". 270 is the typical
   value. If PIXEL_PATH=0 and LINE_PROGRESSION=270, neither are
   reported.
-  **TIFFTAG_XRESOLUTION**: Scan X resolution in dot per inch, from the
   "rdensty" header field. TIFFTAG_XRESOLUTION and TIFFTAG_YRESOLUTION
   are always equal in CALS.
-  **TIFFTAG_YRESOLUTION**: Scan Y resolution in dot per inch, from the
   "rdensty" header field. TIFFTAG_XRESOLUTION and TIFFTAG_YRESOLUTION
   are always equal in CALS.

Creation issues
---------------

Only single band 1-bit rasters are valid input to create a new CALS
file. If the input raster has no color table, 0 is assumed to be black
and 1 to be white. If the input raster has a (2 entries) color table,
the value for the black and white color will be determined from the
color table.

See Also
--------

-  `MIL-PRF-28002C <http://everyspec.com/MIL-PRF/MIL-PRF-010000-29999/MIL-PRF-28002C_4830/>`__
-  `MIL-STD-1840C <http://everyspec.com/MIL-STD/MIL-STD-1800-1999/MIL-STD-1840C_4779/>`__
