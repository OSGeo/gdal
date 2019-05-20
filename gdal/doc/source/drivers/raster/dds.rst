.. _raster.dds:

================================================================================
DDS -- DirectDraw Surface
================================================================================

.. shortname:: DDS

Supported for writing and creation. The DirectDraw Surface file format
(uses the filename extension DDS), from Microsoft, is a standard for
storing data compressed with the lossy S3 Texture Compression (S3TC)
algorithm. The DDS format and compression are provided by the crunch
library.

The driver supports the following texture formats: DXT1. DXT1A, DXT3
(default) and DXT5. You can set the texture format using the creation
option FORMAT.

The driver supports the following compression quality: SUPERFAST, FAST,
NORMAL (default), BETTER and UBER. You can set the compression quality
using the creation option QUALITY.

More information about `Crunch Lib <http://code.google.com/p/crunch/>`__

NOTE: Implemented as ``gdal/frmts/dds/ddsdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::
