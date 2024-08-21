.. _raster.avif:

================================================================================
AVIF -- AV1 Image File Format
================================================================================

.. versionadded:: 3.10

.. shortname:: AVIF

.. build_dependencies:: libavif

AV1 Image File Format (AVIF) is an open, royalty-free image file format
specification for storing images or image sequences compressed with AV1 in the
HEIF container format.

It supports 8-bit, 10-bit and 12-bit images, single-bland, single-band and
alpha channel, RGB and RGBA.

Files containing animations (several images) will be exposed as GDAL subdatasets.

Compression and decompression is done on entire images, so the driver can not
handle arbitrary sizes images.

Note: read-only support for AVIF files is also available through the
:ref:`raster.heif` driver if the AVIF driver is not available, and if libheif
has been compiled with an AV1 compatible decoder.

Driver capabilities
-------------------

.. supports_virtualio

.. supports_createcopy

Color Profile Metadata
----------------------

GDAL can deal with the following color profile
metadata in the COLOR_PROFILE domain:

-  SOURCE_ICC_PROFILE (Base64 encoded ICC profile embedded in file.)

Creation options
----------------

|about-creation-options|
The following creation options are supported:

-  .. co:: CODEC
      :choices: AUTO, AOM, RAV1E, SVT
      :default: AUTO

      Compression library to use. Choices available depend on how libavif has
      been built.

-  .. co:: QUALITY
      :choices: [0-100]
      :default: 60

      Quality for non-alpha channels. 0 is the lowest quality, 100 is for
      lossless encoding. Default is 60.

-  .. co:: QUALITY_ALPHA
      :choices: [0-100]
      :default: 100

      Quality for alpha channel. 0 is the lowest quality, 100 is for
      lossless encoding. Default is 100/lossless.

-  .. co:: SPEED
      :choices: [0-10]
      :default: 6

      Speed of encoding. 0=slowest. 10=fastest.

-  .. co:: NUM_THREADS
      :choices: <integer>|ALL_CPUS
      :default: ALL_CPUS

      Number of worker threads for compression.

-  .. co:: SOURCE_ICC_PROFILE

      ICC profile encoded in Base64. Can also be
      set to empty string to avoid the ICC profile from the source dataset to be used.

-  .. co:: WRITE_EXIF_METADATA
      :choices: YES, NO
      :default: YES

      Whether to write EXIF metadata present in source file.

-  .. co:: WRITE_XMP
      :choices: YES, NO
      :default: YES

      Whether to write XMP metadata present in source file.

-  .. co:: NBITS
      :choices: 8, 10, 12

      Bit depth.

-  .. co:: YUV_SUBSAMPLING
      :choices: 444, 422, 420
      :default: 444

      Type of `chroma subsampling <https://en.wikipedia.org/wiki/Chroma_subsampling>`
      to apply to YUV channels for RGB or RGBA images (it is ignored for single
      band of single band + alpha images)
      4:4:4 corresponds to full horizontal and vertical resolution for chrominance
      channels.
      4:2:2 corresponds to half horizontal and full vertical resolution.
      4:2:0 corresponds to half horizontal and half vertical resolution.
      Only 4:4:4 can be used for lossless encoding.


See Also
--------

- `libavif <https://github.com/AOMediaCodec/libavif>`__
