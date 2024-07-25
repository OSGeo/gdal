.. _raster.exr:

================================================================================
EXR -- Extended Dynamic Range Image File Format
================================================================================

.. versionadded:: 3.1

.. shortname:: EXR

.. build_dependencies:: libopenexr

OpenEXR is a high dynamic range raster file format. The driver supports reading
and writing images in that format.

Georeferencing is written as a WKT CRS string and a 3x3 geotransform matrix in
EXR header metadata.

"Deep images" are not supported.

Creation Options
----------------

|about-creation-options|
The following creation options are supported:

-  .. co:: COMPRESS
      :choices: NONE, RLE, ZIPS, ZIP, PIZ, PXR24, B44, B44A, DWAA, DWAB
      :default: ZIP

      Compression method.
      Details on the format `Wikipedia page <https://en.wikipedia.org/wiki/OpenEXR#Compression_methods>`_

-  .. co:: PIXEL_TYPE
      :choices: HALF, FLOAT, UINT

      Pixel type used for encoding.

      - ``HALF`` corresponds to a IEEE-754 16-bit floating point value.
      - ``FLOAT`` corresponds to a IEEE-754 32-bit floating point value.
      - ``UINT`` corresponds to a 32-bit unsigned integer value.

      If not specified, the following GDAL data types will be mapped as following:

      - ``Byte`` ==> HALF
      - ``Int16`` ==> HALF (potentially lossy)
      - ``UInt16`` ==> HALF (potentially lossy)
      - ``Int32`` ==> FLOAT (potentially lossy)
      - ``UInt32`` ==> UINT
      - ``Float32`` ==> FLOAT
      - ``Float64`` ==> FLOAT (generally lossy)

-  .. co:: TILED
      :choices: YES, NO
      :default: NO

      By default tiled files will be created, unless this option
      is set to NO. In Create() mode, setting TILED=NO is not possible.

-  .. co:: BLOCKXSIZE
      :choices: <integer>
      :default: 256

      Sets tile width.

-  .. co:: BLOCKYSIZE
      :choices: <integer>
      :default: 256

      Sets tile height.

-  .. co:: OVERVIEWS
      :choices: YES, NO
      :default: NO

      Whether to create overviews. Only compatible with CreateCopy() mode.

-  .. co:: OVERVIEW_RESAMPLING
      :choices: NEAR, AVERAGE, CUBIC, ...
      :default: CUBIC

      Resampling method to use for overview creation.

-  .. co:: PREVIEW
      :choices: YES, NO
      :default: NO

      Whether to create a preview. Only
      compatible with CreateCopy() mode, and with RGB(A) data of type Byte.

-  .. co:: AUTO_RESCALE
      :choices: YES, NO

      Whether to rescale Byte RGB(A) values from 0-255 to
      the 0-1 range usually used in EXR ecosystem.

-  .. co:: DWA_COMPRESSION_LEVEL
      :choices: <integer>

      DWA compression level. The higher, the more
      compressed the image will be (and the more artifacts). Defaults to 45
      for OpenEXR 2.4

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

    With the caveat, that it is only for tiled data, and each tile must be
    written at most once, and written tiles cannot be read back before dataset
    closing.

.. supports_georeferencing::

.. supports_virtualio::
