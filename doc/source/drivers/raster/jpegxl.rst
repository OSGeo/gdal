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
can be set with the :config:`GDAL_NUM_THREADS` configuration option
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

Open Options
------------

|about-open-options|
The following open options are available:

-  .. oo:: APPLY_ORIENTATION
      :choices: YES, NO
      :default: NO
      :since: 3.7

      Whether to use EXIF_Orientation
      metadata item to rotate/flip the image to apply scene orientation.
      Defaults to NO (that is the image will be returned in sensor orientation).

Creation Options
----------------

With libjxl 0.6.1, only 1 (greyscale), 2 band (greyscale + alpha), 3 band (RGB)
or 4 band (RGBA) source images are supported. With later libjxl versions, any
number of bands can be written.
Supported data types are Byte, UInt16 and Float32.

When copying from a (regular) JPEG file, and not specifying lossy compression
options, its content is re-encoded in a lossless way, and with reconstruction
data that enables to recreate a JPEG file from the JPEGXL codestream.

|about-creation-options|
The following creation options are available:

-  .. co:: LOSSLESS
      :choices: YES, NO

      Whether JPEGXL compression should be lossless.
      Defaults to YES (unless DISTANCE or QUALITY are specified)

-  .. co:: LOSSLESS_COPY
      :choices: AUTO, YES, NO
      :default: AUTO
      :since: 3.7

      Whether conversion should be lossless.
      In AUTO or YES mode, if LOSSLESS=YES and the source dataset uses JPEG
      compression, lossless recoding of it to JPEGXL is done, and a JPEG
      reconstruction box is added so that reverse conversion to JPEG is possible.
      If set to NO, or in AUTO mode if the source dataset does not use JPEG
      compression, the regular conversion code path is taken, resulting in a
      lossless or lossy copy depending on the LOSSLESS setting.

-  .. co:: EFFORT
      :choices: 1-9
      :default: 5

      Level of effort.
      The higher, the smaller file and slower compression time.

-  .. co:: DISTANCE
      :choices: 0.1-15
      :default: 1.0

      Distance level for lossy JPEG-XL compression.
      It is specified in multiples of a just-noticeable difference.
      (cf `butteraugli <https://github.com/google/butteraugli>`__ for the definition
      of the distance)
      That is, 0 is mathematically lossless, 1 should be visually lossless, and
      higher distances yield denser and denser files with lower and lower fidelity.
      The recommended range is [0.5,3].

-  .. co:: ALPHA_DISTANCE
      :choices: -1, 0, 0.1-15
      :default: -1.0
      :since: 3.7

      (libjxl > 0.8.1)
      Distance level for alpha channel for lossy JPEG-XL compression.
      It is specified in multiples of a just-noticeable difference.
      (cf `butteraugli <https://github.com/google/butteraugli>`__ for the definition
      of the distance)
      That is, 0 is mathematically lossless, 1 should be visually lossless, and
      higher distances yield denser and denser files with lower and lower fidelity.
      For lossy compression, the recommended range is [0.5,3].
      The default value is the special value -1.0, which means to use the same
      distance value as non-alpha channel (ie DISTANCE).

-  .. co:: QUALITY
      :choices: [-inf\,100]
      :default: 90.0

      Alternative setting to :co:`DISTANCE` to specify lossy
      compression, roughly matching libjpeg quality setting in the [0,100] range.

-  .. co:: NBITS
      :choices: <integer>

      Create a file with less than 8 bits per sample by
      passing a value from 1 to 7 for a Byte type, or a value from 9 to 15 for
      a UInt16 type.

-  .. co:: NUM_THREADS
      :choices: <number_of_threads>, ALL_CPUS
      :default: ALL_CPUS

      Set the number of worker threads
      for multi-threaded compression.
      If not set, can also be controlled with the
      :config:`GDAL_NUM_THREADS` configuration option.

-  .. co:: SOURCE_ICC_PROFILE

      ICC profile encoded in Base64. Can also be
      set to empty string to avoid the ICC profile from the source dataset to be used.

-  .. co:: WRITE_EXIF_METADATA
      :choices: YES, NO
      :default: YES

      (libjxl > 0.6.1) Whether to write EXIF metadata from the
      EXIF metadata domain of the source dataset in a Exif box.

-  .. co:: WRITE_XMP
      :choices: YES, NO
      :default: YES

      (libjxl > 0.6.1) Whether to write XMP metadata from the
      xml:XMP metadata domain of the source dataset in a xml box.

-  .. co:: WRITE_GEOJP2
      :choices: YES, NO
      :default: YES

      (libjxl > 0.6.1) Whether to write georeferencing in a JUMBF UUID box
      using GeoJP2 encoding.

-  .. co:: COMPRESS_BOXES
      :choices: YES, NO
      :default: NO

      (libjxl > 0.6.1) Whether to to decompress Exif/XMP/GeoJP2 boxes
      using Brotli compression.

See Also
--------

-  `JPEG-XL home page <https://jpeg.org/jpegxl/>`__
-  `libjxl <https://github.com/libjxl/libjxl/>`__
