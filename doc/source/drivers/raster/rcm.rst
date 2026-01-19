.. _raster.rcm:

================================================================================
RCM -- RADARSAT Constellation Mission Product
================================================================================

.. versionadded:: 3.11

.. shortname:: RCM

.. built_in_by_default::

This driver will read RADARSAT Constellation Mission polarimetric products.

The RADARSAT Constellation Mission XML products are distributed with a primary
XML file called product.xml, and a set of supporting XML data files with the
actual imagery stored in TIFF files.
The RCM driver will be used if the product.xml or the containing directory is
selected, and it can treat all the imagery as one consistent dataset.

The RCM driver also reads geolocation tiepoints from the product.xml file and
represents them as GCPs on the dataset.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Data Calibration
----------------

If you wish to have GDAL apply a particular calibration LUT to the data
when you open it, you have to open the appropriate subdatasets.
The following subdatasets exist within the SUBDATASET domain for RCM products:

- uncalibrated: open with ``RCM_CALIB:UNCALIB:`` prepended to filename
- beta\ :sub:`0`: open with ``RCM_CALIB:BETA0:`` prepended to filename
- sigma\ :sub:`0`: open with ``RCM_CALIB:SIGMA0:`` prepended to filename
- Gamma: open with  ``RCM_CALIB:GAMMA:`` prepended to filename

Note that geocoded (GCC/GCD) products do not have this functionality available.
Also be aware that the LUTs must be in the product directory where specified in
the product.xml, otherwise loading the product with the calibration LUT applied
will fail.

One caveat worth noting is that the RCM driver will supply the calibrated data
as GDT_Float32 or GDT_CFloat32 depending on the type of calibration selected.
The uncalibrated data is provided as GDT_Int16/GDT_UInt8/GDT_CInt16, also
depending on the type of product selected.

See Also
--------

- RADARSAT Constellation Mission Product Specification RCM-SP-52-9092
