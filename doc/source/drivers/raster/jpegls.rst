.. _raster.jpegls:

================================================================================
JPEGLS
================================================================================

.. shortname:: JPEGLS

.. build_dependencies:: CharLS library

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_JPEGLS

This driver is an implementation of a JPEG-LS reader/writer based on the
Open Source CharLS library (BSD style license).

The driver can read and write lossless or near-lossless images. Note
that it is not aimed at dealing with too big images (unless enough
virtual memory is available), since the whole image must be
compressed/decompressed in a single operation.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::

Creation Options
----------------

-  **INTERLEAVE=PIXEL/LINE/BAND** : Data interleaving in compressed
   stream. Default to BAND.

-  **LOSS_FACTOR=error_threshold** : 0 (the default) means loss-less
   compression. Any higher value will be the maximum bound for the
   error.

See Also:
---------

-  Implemented as ``gdal/frmts/jpegls/jpeglsdataset.cpp``.

-  `Homepage of the CharLS
   library <https://github.com/team-charls/charls>`__
