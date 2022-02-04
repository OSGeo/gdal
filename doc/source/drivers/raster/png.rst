.. _raster.png:

================================================================================
PNG -- Portable Network Graphics
================================================================================

.. shortname:: PNG

.. built_in_by_default:: internal libpng provided

GDAL includes support for reading, and creating .png files. Greyscale,
pseudo-colored, Paletted, RGB and RGBA PNG files are supported as well
as precisions of eight and sixteen bits per sample.

The GDAL PNG Driver is built using the libpng library. Also note that
the GeoTIFF driver supports tiled TIFF with DEFLATE compressed tiles,
which is the same compression algorithm that PNG at its core uses.

PNG files are linearly compressed, so random reading of large PNG files
can be very inefficient (resulting in many restarts of decompression
from the start of the file). The maximum dimension of a PNG file that
can be created by GDAL is set to 1,000,000x1,000,000 pixels by libpng.

Text chunks are translated into metadata, typically with multiple lines
per item. :ref:`raster.wld` with the extensions of .pgw, .pngw or
.wld will be read. Single transparency values in greyscale files will be
recognised as a nodata value in GDAL. Transparent index in paletted
images are preserved when the color table is read.

PNG files can be created with a type of PNG, using the CreateCopy()
method, requiring a prototype to read from. Writing includes support for
the various image types, and will preserve transparency/nodata values.
Georeferencing .wld files are written if option WORLDFILE is set. All
pixel types other than 16bit unsigned will be written as eight bit.

XMP metadata can be extracted from the file,
and will be stored as XML raw content in the xml:XMP metadata domain.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Color Profile Metadata
----------------------

GDAL can deal with the following color profile
metadata in the COLOR_PROFILE domain:

-  SOURCE_ICC_PROFILE (Base64 encoded ICC profile embedded in file. If
   available, other tags are ignored.)
-  SOURCE_ICC_PROFILE_NAME : ICC profile name. sRGB is recognized as a
   special value.
-  SOURCE_PRIMARIES_RED (xyY in "x,y,1" format for red primary.)
-  SOURCE_PRIMARIES_GREEN (xyY in "x,y,1" format for green primary)
-  SOURCE_PRIMARIES_BLUE (xyY in "x,y,1" format for blue primary)
-  SOURCE_WHITEPOINT (xyY in "x,y,1" format for whitepoint)
-  PNG_GAMMA

Note that these metadata properties can only be used on the original raw
pixel data. If automatic conversion to RGB has been done, the color
profile information cannot be used.

All these metadata tags can be used as creation options.

Creation Options:

-  **WORLDFILE=YES**: Force the generation of an associated ESRI world
   file (with the extension .wld). See `World File <#WLD>`__ section for
   details.
-  **ZLEVEL=n**: Set the amount of time to spend on compression. The
   default is 6. A value of 1 is fast but does no compression, and a
   value of 9 is slow but does the best compression.
-  **TITLE=value**: Title, written in a TEXT or iTXt chunk
-  **DESCRIPTION=value**: Description, written in a TEXT or iTXt chunk
-  **COPYRIGHT=value**: Copyright, written in a TEXT or iTXt chunk
-  **COMMENT=value**: Comment, written in a TEXT or iTXt chunk
-  **WRITE_METADATA_AS_TEXT=YES/NO**: Whether to write source dataset
   metadata in TEXT chunks
-  **NBITS=1/2/4**: Force number of output bits (GDAL >= 2.1)

NOTE: Implemented as ``gdal/frmts/png/pngdataset.cpp``.

PNG support is implemented based on the libpng reference library. More
information is available at http://www.libpng.org/pub/png.
