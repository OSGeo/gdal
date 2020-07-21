.. _raster.bsb:

================================================================================
BSB -- Maptech/NOAA BSB Nautical Chart Format
================================================================================

.. shortname:: BSB

.. built_in_by_default::

BSB Nautical Chart format is supported for read access, including
reading the colour table and the reference points (as GCPs). Note that
the .BSB files cannot be selected directly. Instead select the .KAP
files. Versions 1.1, 2.0 and 3.0 have been tested successfully.

This driver should also support GEO/NOS format as supplied by Softchart.
These files normally have the extension .nos with associated .geo files
containing georeferencing ... the .geo files are currently ignored.

This driver is based on work by Mike Higgins. See the
frmts/bsb/bsb_read.c files for details on patents affecting BSB format.

It is possible to select an alternate color
palette via the BSB_PALETTE configuration option. The default value is
RGB. Other common values that can be found are : DAY, DSK, NGT, NGR,
GRY, PRC, PRG...

NOTE: Implemented as ``gdal/frmts/bsb/bsbdataset.cpp``.


Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Metadata
--------

The following metadata items may be reported:

- **BSB_KNP**: content of the KNP/ header field, giving information on the
  coordinate reference system.

- **BSB_KNQ**: content of the KNQ/ header field, giving information on the
  coordinate reference system.

- **BSB_CUTLINE**: (starting with GDAL 3.1). When PLY/ header is present,
  Well-Known text representation of a polygon with coordinates in longitude,
  latitude order, representing the cutline of the chart.
