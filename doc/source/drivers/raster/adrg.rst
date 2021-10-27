.. _raster.adrg:

================================================================================
ADRG -- ADRG/ARC Digitized Raster Graphics (.gen/.thf)
================================================================================

.. shortname:: ADRG

.. built_in_by_default::

Supported by GDAL for read access. Creation is possible, but it must be
considered as experimental and a means of testing read access (although
files created by the driver can be read successfully on another GIS
software)

An ADRG dataset is made of several files. The file recognised by GDAL is
the General Information File (.GEN). GDAL will also need the image file
(.IMG), where the actual data is.

The Transmission Header File (.THF) can also be used as an input to
GDAL. If the THF references more than one image, GDAL will report the
images it is composed of as subdatasets. If the THF references just one
image, GDAL will open it directly.

Overviews, legends and insets are not used. Polar zones (ARC zone 9 and
18) are not supported (due to the lack of test data).

See also : the `ADRG specification
(MIL-A-89007) <http://earth-info.nga.mil/publications/specs/printed/89007/89007_ADRG.pdf>`__


Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
