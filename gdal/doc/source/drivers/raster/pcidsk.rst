.. _raster.pcidsk:

================================================================================
PCIDSK -- PCI Geomatics Database File
================================================================================

.. shortname:: PCIDSK

.. built_in_by_default::

PCIDSK database file used by PCI EASI/PACE software for image analysis.
It is supported for reading, and writing by GDAL. All pixel data types,
and data organizations (pixel interleaved, band interleaved, file
interleaved and tiled) should be supported. Currently LUT segments are
ignored, but PCT segments should be treated as associated with the
bands. Overall file, and band specific metadata should be correctly
associated with the image or bands.

Georeferencing is supported though there may be some limitations in
support of datums and ellipsoids. GCP segments are ignored. RPC segments
will be returned as GDAL style RPC metadata.

Internal overview (pyramid) images will also be correctly read though
newly requested overviews will be built externally as an .ovr file.

Vector segments are also supported by the driver.

Creation Options
----------------

Note that PCIDSK files are always produced pixel interleaved, even
though other organizations are supported for read.

-  **INTERLEAVING=PIXEL/BAND/FILE/TILED**: sets the interleaving for the
   file raster data.
-  **COMPRESSION=NONE/RLE/JPEG**: Sets the compression to use. Values
   other than NONE (the default) may only be used with TILED
   interleaving. If JPEG is select it may include a quality value
   between 1 and 100 - eg. COMPRESSION=JPEG40.
-  **TILESIZE=n**: When INTERLEAVING is TILED, the tilesize may be
   selected with this parameter - the default is 127 for 127x127.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

See Also:
---------

-  Implemented as ``gdal/frmts/pcidsk/pcidskdataset2.cpp``.
-  `PCIDSK SDK <https://web.archive.org/web/20130730111701/http://home.gdal.org/projects/pcidsk/index.html>`__
