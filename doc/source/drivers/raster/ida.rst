.. _raster.ida:

================================================================================
IDA -- Image Display and Analysis
================================================================================

.. shortname:: IDA

.. built_in_by_default::

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_IDA

GDAL supports reading and writing IDA images with some limitations. IDA
images are the image format of WinDisp 4. The files are always one band
only of 8bit data. IDA files often have the extension .img though that
is not required.

Projection and georeferencing information is read though some
projections (i.e. Meteosat, and Hammer-Aitoff) are not supported. When
writing IDA files the projection must have a false easting and false
northing of zero. The support coordinate systems in IDA are Geographic,
Lambert Conformal Conic, Lambert Azimuth Equal Area, Albers Equal-Area
Conic and Goodes Homolosine.

IDA files typically contain values scaled to 8bit via a slope and
offset. These are returned as the slope and offset values of the bands
and they must be used if the data is to be rescaled to original raw
values for analysis.

NOTE: Implemented as ``gdal/frmts/raw/idadataset.cpp``.

See Also:
`WinDisp <http://www.fao.org/giews/english/windisp/windisp.htm>`__

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
