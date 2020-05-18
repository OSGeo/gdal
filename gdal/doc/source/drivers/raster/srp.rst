.. _raster.srp:

================================================================================
SRP -- Standard Product Format (ASRP/USRP) (.gen)
================================================================================

.. shortname:: SRP

.. built_in_by_default::

The ASRP and USRP raster products (as defined by DGIWG) are variations
on a common standard product format and are supported for reading by
GDAL. ASRP and USRP datasets are made of several files - typically a
.GEN, .IMG, .SOU and .QAL file with a common basename. The .IMG file
should be selected to access the dataset.

ASRP (in a geographic coordinate system) and USRP (in a UTM/UPS
coordinate system) products are single band images with a palette and
georeferencing.

the Transmission Header File (.THF) can also be
used as an input to GDAL. If the THF references more than one image,
GDAL will report the images it is composed of as subdatasets. If the THF
references just one image, GDAL will open it directly.

NOTE: Implemented as ``gdal/frmts/adrg/srpdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
