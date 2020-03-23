.. _raster.isce:

================================================================================
ISCE -- ISCE
================================================================================

.. shortname:: ISCE

.. built_in_by_default::

Driver for the image formats used in the JPL's Interferometric synthetic
aperture radar Scientific Computing Environment (ISCE). Only images with
data types mappable to GDAL data types are supported.

Image properties are stored under the ISCE metadata domain, but there is
currently no support to access underlying components elements and their
properties. Likewise, ISCE domain metadata will be saved as properties
in the image XML file.

Georeferencing is not yet implemented.

The ACCESS_MODE property is not currently honored.

The only creation option currently is SCHEME, which value (BIL, BIP,
BSQ) determine the interleaving (default is BIP).

NOTE: Implemented as ``gdal/frmts/raw/iscedataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
