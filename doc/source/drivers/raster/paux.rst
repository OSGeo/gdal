.. _raster.paux:

================================================================================
PAux -- PCI .aux Labelled Raw Format
================================================================================

.. shortname:: PAux

.. built_in_by_default::

GDAL includes a partial implementation of the PCI .aux labelled raw
raster file for read, write and creation. To open a PCI labelled file,
select the raw data file itself. The .aux file (which must have a common
base name) will be checked for automatically.

The format type for creating new files is ``PAux``. All PCI data types
(8U, 16U, 16S, and 32R) are supported. Currently georeferencing,
projections, and other metadata is ignored.

See Also: `PCI's .aux Format
Description <http://www.pcigeomatics.com/cgi-bin/pcihlp/GDB%7CSupported+File+Formats%7CRaw+Binary+Image+Format+(RAW)%7CRaw+.aux+Format>`__

Driver capabilities
-------------------

.. supports_virtualio::
