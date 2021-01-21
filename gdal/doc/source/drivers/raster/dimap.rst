.. _raster.dimap:

================================================================================
DIMAP -- Spot DIMAP
================================================================================

.. shortname:: DIMAP

.. built_in_by_default::

This is a read-only read for Spot DIMAP described images. To use, select
the METADATA.DIM (DIMAP 1), VOL_PHR.XML (DIMAP 2) or VOL_PNEO.XML (DIMAP 2 VHR-2020)
file in a product directory, or the product directory itself.

The imagery is in a distinct imagery file, often a TIFF file, but the
DIMAP dataset handles accessing that file, and attaches geolocation and
other metadata to the dataset from the metadata xml file.

The content of the <Spectral_Band_Info> node is
reported as metadata at the level of the raster band. Note that the
content of the Spectral_Band_Info of the first band is still reported as
metadata of the dataset, but this should be considered as a deprecated
way of getting this information.

For multi-component products, each component will be exposed in a subdataset.

NOTE: Implemented as ``gdal/frmts/dimap/dimapdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
