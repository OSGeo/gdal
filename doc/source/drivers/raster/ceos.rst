.. _raster.ceos:

================================================================================
CEOS -- CEOS Image
================================================================================

.. shortname:: CEOS

.. built_in_by_default::

This is a simple, read-only reader for ceos image files. To use, select
the main imagery file. This driver reads only the image data, and does
not capture any metadata, or georeferencing.

This driver is known to work with CEOS data produced by Spot Image, but
will have problems with many other data sources. In particular, it will
only work with eight bit unsigned data.

See the separate :ref:`raster.sar_ceos` driver for access to SAR CEOS
data products.

NOTE: Implemented as :source_file:`frmts/ceos/ceosdataset.cpp`.

Driver capabilities
-------------------

.. supports_virtualio::
