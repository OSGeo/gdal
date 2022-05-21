.. _raster.jpegxl:

================================================================================
JPEGXL -- JPEG-XL File Format
================================================================================

.. versionadded:: 3.6

.. shortname:: JPEGXL

.. build_dependencies:: libjxl

The JPEG-XL format is supported for reading, and batch writing (CreateCopy()), but
not update in place.

The driver supports reading and writing:
- georeferencing: encoded as a GeoJP2 UUID box within a JUMBF box.
- XMP in the xml:XMP metadata domain
- EXIF in the EXIF metadata domain
- color profile in the COLOR_PROFILE metadata domain.

Reading or writing involves ingesting the whole uncompressed image in memory.
Compression is in particular very memory hungry with current libjxl implementation.
For large images (let's say width or height larger than 10,000 pixels),
using JPEGXL compression as a :ref:`raster.gtiff` codec is thus recommended.

The number of worker threads for multi-threaded compression and decompression
can be set with the :decl_configoption:`GDAL_NUM_THREADS` configuration option
to an integer value or ``ALL_CPUS`` (the later is the default).

.. note::
    Support for reading and writing XMP and EXIF, and writing georeferencing,
    requires a libjxl version build from its main branch, post 0.6.1 release.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Color Profile Metadata
----------------------

GDAL can deal with the following color profile
metadata in the COLOR_PROFILE domain:

-  SOURCE_ICC_PROFILE (Base64 encoded ICC profile embedded in file.)

Creation Options
----------------

With libjxl 0.6.1, only 1 (greyscale), 2 band (greyscale + alpha), 3 band (RGB)
or 4 band (RGBA) source images are supported. With later libjxl versions, any
number of bands can be written.
Supported data types are Byte, UInt16 and Float32.

When copying from a (regular) JPEG file, and not specifying lossy compression
options, its content is re-encoded in a lossless way, and with reconstruction
data that enables to recreate a JPEG file from the JPEGXL codestream.

The following creation options are available:

-  **LOSSLESS=YES/NO**: Whether JPEGXL compression should be lossless.
   Defaults to YES (unless DISTANCE or QUALITY are specified)

-  **EFFORT=[1-9]**: Level of effort.
   The higher, the smaller file and slower compression time. Default is 5.

-  **DISTANCE=[0.1-15]**: Distance level for lossy compression
   0=mathematically lossless, 1.0=visually lossless, usual range [0.5,3].
   Default is 1.0

-  **QUALITY=[-inf,100]**: Alternative setting to DISTANCE to specify lossy
   compression, roughly matching libjpeg quality setting in the [0,100] range.
   Default is 90.0

-  **NBITS=n**: Create a file with less than 8 bits per sample by
   passing a value from 1 to 7 for a Byte type, or a value from 9 to 15 for
   a UInt16 type.

-  **NUM_THREADS=number_of_threads/ALL_CPUS**: Set the number of worker threads
   for multi-threaded compression. Default is ALL_CPUS.
   If not set, can also be controlled with the
   :decl_configoption:`GDAL_NUM_THREADS` configuration option.

-  **SOURCE_ICC_PROFILE=value**: ICC profile encoded in Base64. Can also be
   set to empty string to avoid the ICC profile from the source dataset to be used.

-  **WRITE_EXIF_METADATA=YES/NO**: (libjxl > 0.6.1) Whether to write EXIF metadata from the
   EXIF metadata domain of the source dataset in a Exif box.
   Default is YES.

-  **WRITE_XMP=YES/NO**: (libjxl > 0.6.1) Whether to write XMP metadata from the
   xml:XMP metadata domain of the source dataset in a xml box.
   Default is YES.

-  **WRITE_GEOJP2=YES/NO**: (libjxl > 0.6.1) Whether to write georeferencing in a JUMBF UUID box
   using GeoJP2 encoding. Default is YES.

-  **COMPRESS_BOXES=YES/NO**: (libjxl > 0.6.1) Whether to to decompress Exif/XMP/GeoJP2 boxes
   using Brotli compression. Default is NO.

See Also
--------

-  `JPEG-XL home page <https://jpeg.org/jpegxl/>`__
-  `libjxl <https://github.com/libjxl/libjxl/>`__
