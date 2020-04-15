.. _raster.esat:

================================================================================
ESAT -- Envisat Image Product
================================================================================

.. shortname:: ESAT

.. built_in_by_default::

GDAL supports the Envisat product format for read access. All sample
types are supported. Files with two matching measurement datasets (MDS)
are represented as having two bands. Currently all ASAR Level 1 and
above products, and some MERIS and AATSR products are supported.

The control points of the GEOLOCATION GRID ADS dataset are read if
available, generally giving a good coverage of the dataset. The GCPs are
in WGS84.

Virtually all key/value pairs from the MPH and SPH (primary and
secondary headers) are copied through as dataset level metadata.

ASAR and MERIS parameters contained in the ADS and GADS records
(excluded geolocation ones) can be retrieved as key/value pairs using
the "RECORDS" metadata domain.

NOTE: Implemented as ``gdal/frmts/envisat/envisatdataset.cpp``.

See Also: `Envisat Data
Products <http://envisat.esa.int/dataproducts/>`__ at ESA.

Driver capabilities
-------------------

.. supports_virtualio::
