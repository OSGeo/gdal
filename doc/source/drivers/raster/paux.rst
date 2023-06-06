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

Creation Options:

-  .. co:: INTERLEAVE
      :choices: PIXEL, LINE, BAND
      :default: BAND

      Establish output interleaving.
      Starting with GDAL 3.5, when copying from a source dataset with multiple bands
      which advertises a INTERLEAVE metadata item, if the INTERLEAVE creation option
      is not specified, the source dataset INTERLEAVE will be automatically taken
      into account.

NOTE: Implemented as :source_file:`frmts/raw/pauxdataset.cpp`.

See Also: `PCI's .aux Format
Description <http://www.pcigeomatics.com/cgi-bin/pcihlp/GDB%7CSupported+File+Formats%7CRaw+Binary+Image+Format+(RAW)%7CRaw+.aux+Format>`__

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_virtualio::
