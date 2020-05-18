.. _raster.cosar:

================================================================================
COSAR -- TerraSAR-X Complex SAR Data Product
================================================================================

.. shortname:: COSAR

.. built_in_by_default::

This driver provides the capability to read TerraSAR-X complex data.
While most users will receive products in GeoTIFF format (representing
detected radiation reflected from the targets, or geocoded data),
ScanSAR products will be distributed in COSAR format.

Essentially, COSAR is an annotated binary matrix, with each sample held
in 4 bytes (16 bits real, 16 bits imaginary) stored with the most
significant byte first (Big Endian). Within a COSAR container there are
one or more "bursts" which represent individual ScanSAR bursts. Note
that if a Stripmap or Spotlight product is held in a COSAR container it
is stored in a single burst.

Support for ScanSAR data is currently under way, due to the difficulties
in fitting the ScanSAR "burst" identifiers into the GDAL model.

Driver capabilities
-------------------

.. supports_virtualio::

See Also
--------

-  DLR Document TX-GS-DD-3307 "Level 1b Product Format Specification."
