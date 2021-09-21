.. _raster.intergraphraster:

================================================================================
INGR -- Intergraph Raster Format
================================================================================

.. shortname:: INGR

.. built_in_by_default::

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_INGR

This format is supported for read and writes access.

The Intergraph Raster File Format was the native file format used by
Intergraph software applications to store raster data. It is
manifested in several internal data formats.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Reading INGR Files
------------------

Those are the data formats that the INGR driver supports for reading:

-  2 - Byte Integer
-  3 - Word Integer
-  4 - Integers 32 bit
-  5 - Floating Point 32 bit
-  6 - Floating Point 64 bit
-  9 - Run Length Encoded
-  10 - Run Length Encoded Color
-  24 - CCITT Group 4
-  27 - Adaptive RGB
-  28 - Uncompressed 24 bit
-  29 - Adaptive Gray Scale
-  30 - JPEG GRAY
-  31 - JPEG RGB
-  32 - JPEG CMYK
-  65 - Tiled
-  67 - Continuous Tone 

The format "65 - Tiled" is not a format; it is just an indication that
the file is tiled. In this case the tile header contains the real data
format code that could be any of the above formats. The INGR driver can
read tiled and untiled instance of any of the supported data formats.

Writing INGR Files
------------------

Those are the data formats that the INGR driver supports for writing:

-  2 - Byte Integer
-  3 - Word Integers
-  4 - Integers 32Bit
-  5 - Floating Point 32Bit
-  6 - Floating Point 64Bit

Type 9 RLE bitonal compression is used when outputting ".rle" file.
Other file types are uncompressed.

Note that writing in that format is not encouraged.

File Extension
--------------

The following is a partial listing of INGR file extensions:

+-----------------------------------+-----------------------------------+
| .cot                              | 8-bit grayscale or color table    |
|                                   | data                              |
+-----------------------------------+-----------------------------------+
| .ctc                              | 8-bit grayscale using             |
|                                   | PackBits-type compression         |
|                                   | (uncommon)                        |
+-----------------------------------+-----------------------------------+
| .rgb                              | 24-bit color and grayscale        |
|                                   | (uncompressed and PackBits-type   |
|                                   | compression)                      |
+-----------------------------------+-----------------------------------+
| .ctb                              | 8-bit color table data            |
|                                   | (uncompressed or run-length       |
|                                   | encoded)                          |
+-----------------------------------+-----------------------------------+
| .grd                              | 8, 16 and 32 bit elevation data   |
+-----------------------------------+-----------------------------------+
| .crl                              | 8 or 16 bit, run-length           |
|                                   | compressed grayscale or color     |
|                                   | table data                        |
+-----------------------------------+-----------------------------------+
| .tpe                              | 8 or 16 bit, run-length           |
|                                   | compressed grayscale or color     |
|                                   | table data                        |
+-----------------------------------+-----------------------------------+
| .lsr                              | 8 or 16 bit, run-length           |
|                                   | compressed grayscale or color     |
|                                   | table data                        |
+-----------------------------------+-----------------------------------+
| .rle                              | 1-bit run-length compressed data  |
|                                   | (16-bit runs)                     |
+-----------------------------------+-----------------------------------+
| .cit                              | CCITT G3 or G4 1-bit data         |
+-----------------------------------+-----------------------------------+
| .g3                               | CCITT G3 1-bit data               |
+-----------------------------------+-----------------------------------+
| .g4                               | CCITT G4 1-bit data               |
+-----------------------------------+-----------------------------------+
| .tg4                              | CCITT G4 1-bit data (tiled)       |
+-----------------------------------+-----------------------------------+
| .cmp                              | JPEG grayscale, RGB, or CMYK      |
+-----------------------------------+-----------------------------------+
| .jpg                              | JPEG grayscale, RGB, or CMYK      |
+-----------------------------------+-----------------------------------+

.. container::

   Source: \ http://www.oreilly.com/www/centers/gff/formats/ingr/index.htm

|
| The INGR driver does not require any especial file extension in order
  to identify or create an INGR file.

Georeference
------------

The INGR driver does not support reading or writing georeference
information. The reason for that is because there is no universal way of
storing georeferencing in INGR files. It could have georeference stored
in a accompanying .dgn file or in application specific data storage
inside the file itself.

Metadata
--------

The following creation option and bandset metadata is available.

-  RESOLUTION: This is the DPI (dots per inch). Microns not supported.

See Also
--------

For more information:

-  Implemented as ``gdal/frmts/ingr/intergraphraster.cpp``.
-  `www.intergraph.com <http://www.intergraph.com>`__
-  http://www.oreilly.com/www/centers/gff/formats/ingr/index.htm
-  File specification:
   ftp://ftp.intergraph.com/pub/bbs/scan/note/rffrgps.zip/
