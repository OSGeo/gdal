.. _raster.isis2:

================================================================================
ISIS2 -- USGS Astrogeology ISIS Cube (Version 2)
================================================================================

.. shortname:: ISIS2

.. built_in_by_default::

ISIS2 is a format used by the USGS Planetary Cartography group to store
and distribute planetary imagery data. GDAL provides read
access to ISIS2 formatted imagery data.

ISIS2 files often have the extension .cub, sometimes with an associated
.lbl (label) file. When a .lbl file exists it should be used as the
dataset name rather than the .cub file.

In addition to support for most ISIS2 imagery configurations, this
driver also reads georeferencing and coordinate system information as
well as selected other header metadata.

Implementation of this driver was supported by the United States
Geological Survey.

ISIS2 is part of a family of related formats including PDS and ISIS3.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  Implemented as :source_file:`frmts/pds/isis2dataset.cpp`.
-  :ref:`raster.pds` driver
-  :ref:`raster.isis3` driver
