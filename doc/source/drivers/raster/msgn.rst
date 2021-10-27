.. _raster.msgn:

================================================================================
MSGN -- Meteosat Second Generation (MSG) Native Archive Format (.nat)
================================================================================

.. shortname:: MSGN

.. built_in_by_default::

GDAL supports reading only of MSG native files. These files may have
anything from 1 to 12 bands, all at 10-bit resolution.

Includes support for the 12th band (HRV - High Resolution Visible). This
is implemented as a subset, i.e., it is accessed by prefixing the
filename with the tag "HRV:".

Similarly, it is possible to obtain floating point radiance values in
stead of the usual 10-bit digital numbers (DNs). This subset is accessed
by prefixing the filename with the tag "RAD:".

Georeferencing is currently supported, but the results may not be
acceptable (accurate enough), depending on your requirements. The
current workaround is to implement the CGMS Geostationary projection
directly, using the code available from EUMETSAT.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
