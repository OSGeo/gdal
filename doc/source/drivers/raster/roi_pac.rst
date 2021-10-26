.. _raster.roi_pac:

================================================================================
ROI_PAC -- ROI_PAC
================================================================================

.. shortname:: ROI_PAC

.. built_in_by_default::

Driver for the image formats used in the JPL's ROI_PAC project
(https://aws.roipac.org/). All image type are supported excepted .raw
images.

Metadata are stored in the ROI_PAC domain.

Georeferencing is supported, but expect problems when using the UTM
projection, as ROI_PAC format do not store any hemisphere field.

When creating files, you have to be able to specify the right data type
corresponding to the file type (slc, int, etc), else the driver will
output an error.

NOTE: Implemented as ``gdal/frmts/raw/roipacdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
