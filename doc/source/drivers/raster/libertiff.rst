.. _raster.libertiff:

================================================================================
LIBERTIFF -- GeoTIFF File Format
================================================================================

.. versionadded:: 3.11

.. shortname:: LIBERTIFF

.. built_in_by_default::

This driver is a natively thread-safe alternative to the default
:ref:`raster.gtiff` driver. Note that the driver is read-only.

The driver is registered after the GTiff one. Consequently one must explicitly
specify ``LIBERTIFF`` in the allowed drivers of the :cpp:func:`GDALOpenEx`, or
with the ``-if`` option of command line utilities, to use it.

The driver supports the following compression methods: LZW, Deflate, PackBits,
LZMA, ZSTD, LERC, JPEG, WEBP and JPEGXL.
The driver supports only BitsPerSample values of 1, 8, 16, 32, 64.

The driver does *not* read any side-car file: ``.aux.xml``, ``.ovr``, ``.msk``,
``.imd``, etc.

The driver mostly by-passes the GDAL raster block cache, and only caches (per-thread)
the last tile or strip it has read. Read patterns must be adapted accordingly,
to avoid repeated data acquisition from storage and decompression.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

|about-open-options|
This driver supports the following open options:

.. oo:: NUM_THREADS
   :choices: <number_of_threads>, ALL_CPUS

   This option also enables multi-threaded decoding
   when RasterIO() requests intersect several tiles/strips.
   The :config:`GDAL_NUM_THREADS` configuration option can also
   be used as an alternative to setting the open option.
