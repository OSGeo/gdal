.. _raster.kro:

================================================================================
KRO -- KOLOR Raw format
================================================================================

.. shortname:: KRO

.. built_in_by_default::

Supported for read access, update and creation. This format is a binary
raw format, that supports data of several depths ( 8 bit, unsigned
integer 16 bit and floating point 32 bit) and with several band number
(3 or 4 typically, for RGB and RGBA). There is no file size limit,
except the limitation of the file system.

`Specification of the
format <http://www.autopano.net/wiki-en/Format_KRO>`__

NOTE: Implemented as ``gdal/frmts/raw/krodataset.cpp``.


Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_virtualio::
