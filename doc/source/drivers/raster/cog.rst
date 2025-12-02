.. _raster.cog:

================================================================================
COG -- Cloud Optimized GeoTIFF generator
================================================================================

.. versionadded:: 3.1

.. shortname:: COG

.. built_in_by_default::

This driver supports the creation of Cloud Optimized GeoTIFF (COG)

It essentially relies upon the :ref:`raster.gtiff` driver with the
``COPY_SRC_OVERVIEWS=YES`` creation option, but automatically does the needed
preprocessing stages (reprojection if asked and creation of overviews on
imagery and/or mask) if not already
done, and also takes care of morphing the input dataset into the expected form
when using some compression types (for example a RGBA dataset will be transparently
converted to a RGB+mask dataset when selecting JPEG compression)

This driver is nominally used through the :cpp:func:`GDALDriver::CreateCopy` method,
that natively does not support random writing but requires a source dataset
to be provided as input. During the writing process, if overviews are generated,
they will be created in a temporary dataset, requiring storage capacity
of roughly one third of the final file size, before being transferred to the
output file.
By default temporary files are created in the same directory as the final file,
if the target file system supports random writing and if the :config:`CPL_TMPDIR`
configuration option is not set.

Starting with GDAL 3.13, the :cpp:func:`GDALDriver::Create` method is also implemented,
by using a temporary GeoTIFF dataset, which means that at least twice the
final size of the file of storage capacity is required. Note that as the
temporary dataset uses lossless compression, if the final COG file uses lossy
compression, more temporary storage may be needed.

Driver capabilities
-------------------

.. supports_create::

    .. versionadded:: 3.13

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Creation Options
----------------

|about-creation-options|

General creation options
************************

-  .. co:: BLOCKSIZE
      :choices: <integer>
      :default: 512

      Sets the tile width and height in pixels. Must be divisible by 16.

-  .. co:: INTERLEAVE
      :choices: BAND, PIXEL, TILE
      :since: 3.11

      Set the interleaving to use

      * ``PIXEL``: for each spatial block, one TIFF tile/strip gathering values for
        all bands is used . This matches the ``contiguous`` planar configuration in
        TIFF terminology.
        This is also known as a ``BIP (Band Interleaved Per Pixel)`` organization.
        Such method is the default, and may be slightly less
        efficient than BAND interleaving for some purposes, but some applications
        only support pixel interleaved TIFF files. On the other hand, image-based
        compression methods may perform better using PIXEL interleaving. For JPEG
        PHOTOMETRIC=YCbCr, pixel interleaving is required. It is also required for
        WebP compression.

        Prior to GDAL 3.11, INTERLEAVE=PIXEL was the only possible interleaving
        output by the COG driver.

        Assuming a pixel[band][y][x] indexed array, when using CreateCopy(),
        the pseudo code for the file disposition is:

        ::

          for y in 0 ... numberOfBlocksInHeight - 1:
              for x in 0 ... numberOfTilesInWidth - 1:
                  for j in 0 ... blockHeight - 1:
                      for i in 0 ... blockWidth -1:
                          start_new_strip_or_tile()
                          for band in 0 ... numberBands -1:
                              write(pixel[band][y*blockHeight+j][x*blockWidth+i])
                          end_new_strip_or_tile()
                      end_for
                  end_for
              end_for
          end_for


      * ``BAND``: for each spatial block, one TIFF tile/strip is used for each band.
        This matches the contiguous ``separate`` configuration in TIFF terminology.
        This is also known as a ``BSQ (Band SeQuential)`` organization.

        In addition to that, when using CreateCopy(), data for the first band is
        written first, followed by data for the second band, etc.
        The pseudo code for the file disposition is:

        ::

          for y in 0 ... numberOfBlocksInHeight - 1:
              for x in 0 ... numberOfTilesInWidth - 1:
                  start_new_strip_or_tile()
                  for band in 0 ... numberBands -1:
                      for j in 0 ... blockHeight - 1:
                          for i in 0 ... blockWidth -1:
                              write(pixel[band][y*blockHeight+j][x*blockWidth+i])
                          end_for
                      end_for
                  end_new_strip_or_tile()
              end_for
          end_for


      * ``TILE`` (added in 3.11): this is a sort of
        compromise between PIXEL and BAND, using the ``separate`` configuration,
        but where data for a same spatial block is written for all bands, before
        the data of the next spatial block.
        When the block height is 1, this is also known as a
        ``BIL (Band Interleaved per Line)`` organization.

        Such a layout may be useful for writing hyperspectral datasets (several
        hundred of bands), to get a compromise between efficient spatial query,
        and partial band selection.

        Assuming a pixel[band][y][x] indexed array, when using CreateCopy(),
        the pseudo code for the file disposition is:

        ::

          for y in 0 ... numberOfBlocksInHeight - 1:
              for x in 0 ... numberOfTilesInWidth - 1:
                  for band in 0 ... numberBands -1:
                      start_new_strip_or_tile()
                      for j in 0 ... blockHeight - 1:
                          for i in 0 ... blockWidth -1:
                              write(pixel[band][y*blockHeight+j][x*blockWidth+i])
                          end_for
                      end_for
                      end_new_strip_or_tile()
                  end_for
              end_for
          end_for


      Starting with GDAL 3.5, when copying from a source dataset with multiple bands
      which advertises a INTERLEAVE metadata item, if the INTERLEAVE creation option
      is not specified, the source dataset INTERLEAVE will be automatically taken
      into account, unless the COMPRESS creation option is specified.

-  .. co:: COMPRESS
      :choices: NONE, LZW, JPEG, DEFLATE, ZSTD, WEBP, LERC, LERC_DEFLATE, LERC_ZSTD, LZMA
      :default: LZW

      Set the compression to use.
      Defaults to ``LZW`` starting with GDAL 3.4 (default in previous version is ``NONE``).

      * ``JPEG`` should generally only be used with
        Byte data (8 bit per channel). But if GDAL is built with internal libtiff and
        libjpeg, it is    possible to read and write TIFF files with 12bit JPEG compressed TIFF
        files (seen as UInt16 bands with NBITS=12).
        For the COG driver, JPEG compression for 3 or 4-band images automatically
        selects the PHOTOMETRIC=YCBCR colorspace with a 4:2:2 subsampling of the Y,Cb,Cr
        components with the default INTERLEAVE=PIXEL.
        For a input dataset (single-band or 3-band), plus an alpha band,
        the alpha band will be converted as a 1-bit DEFLATE compressed mask.

      * ``LZW``, ``DEFLATE`` and ``ZSTD`` compressions can be used with the PREDICTOR creation option.

      * ``ZSTD`` is available when using internal libtiff and if GDAL built against
        libzstd >=1.0, or if built against external libtiff with zstd support.

      * ``WEBP`` is available when using internal libtiff and if GDAL built against
        libwebp, or if built against external libtiff with WebP support.
        It can only be used with the default INTERLEAVE=PIXEL.

      * ``LERC`` is available when using internal libtiff.

      * ``LERC_ZSTD`` is available when ``LERC`` and ``ZSTD`` are available.

      * ``JXL`` is for JPEG-XL, and is only available when using internal libtiff and building GDAL against
        https://github.com/libjxl/libjxl . JXL compression may only be used on datasets with 4 bands or less.
        Option added in GDAL 3.4

-  .. co:: LEVEL
      :choices: <integer>

      DEFLATE/ZSTD/LERC_DEFLATE/LERC_ZSTD/LZMA compression level.
      A lower number will
      result in faster compression but less efficient compression rate.
      1 is the fastest.

      * For DEFLATE/LZMA, 9 is the slowest/higher compression rate
        (or 12 when using a libtiff with libdeflate support). The default is 6.
      * For ZSTD, 22 is the slowest/higher compression rate. The default is 9.

-  .. co:: MAX_Z_ERROR
      :choices: <threshold>
      :default: 0

      Set the maximum error threshold on values
      for LERC/LERC_DEFLATE/LERC_ZSTD compression. The default is 0
      (lossless).

-  .. co:: MAX_Z_ERROR_OVERVIEW
      :choices: <threshold>
      :since: 3.8

      Set the maximum error threshold on values
      for LERC/LERC_DEFLATE/LERC_ZSTD compression, on overviews.
      The default is the value of :co:`MAX_Z_ERROR`

-  .. co:: QUALITY
      :choices: <integer>
      :default: 75

      JPEG/WEBP quality setting. A value of 100 is best
      quality (least compression), and 1 is worst quality (best compression).
      For WEBP, QUALITY=100 automatically turns on lossless mode.

-  .. co:: JXL_LOSSLESS
      :choices: YES, NO
      :default: YES

      Set whether JPEG-XL compression should be lossless
      (YES) or lossy (NO). For lossy compression, the underlying data
      should be either gray, gray+alpha, rgb or rgb+alpha.

-  .. co:: JXL_EFFORT
      :choices: 1-9
      :default: 5

      Level of effort for JPEG-XL compression.
      The higher, the smaller file and slower compression time.

-  .. co:: JXL_DISTANCE
      :choices: 0.01-25
      :default: 1.0

      (Only applies when JXL_LOSSLESS=NO)
      Distance level for lossy JPEG-XL compression.
      It is specified in multiples of a just-noticeable difference
      (cf `butteraugli <https://github.com/google/butteraugli>`__ for the definition
      of the distance)
      That is, 0 is mathematically lossless, 1 should be visually lossless, and
      higher distances yield denser and denser files with lower and lower fidelity.
      The recommended range is [0.5,3].

-  .. co:: JXL_ALPHA_DISTANCE
      :choices: -1, 0, 0.01-25
      :default: -1
      :since: 3.7

      (Only applies when JXL_LOSSLESS=NO)
      (libjxl > 0.8.1)
      Distance level for alpha channel for lossy JPEG-XL compression.
      It is specified in multiples of a just-noticeable difference.
      (cf `butteraugli <https://github.com/google/butteraugli>`__ for the definition
      of the distance)
      That is, 0 is mathematically lossless, 1 should be visually lossless, and
      higher distances yield denser and denser files with lower and lower fidelity.
      For lossy compression, the recommended range is [0.5,3].
      The default value is the special value -1.0, which means to use the same
      distance value as non-alpha channel (ie :co:`JXL_DISTANCE`).

-  .. co:: NUM_THREADS
      :choices: <number_of_threads>, ALL_CPUS

      Enable multi-threaded compression by specifying the number of worker
      threads. Default is compression in the main thread. This also determines
      the number of threads used when reprojection is done with the :co:`TILING_SCHEME`
      or :co:`TARGET_SRS` creation options. (Overview generation is also multithreaded since
      GDAL 3.2)

-  .. co:: NBITS
      :choices: <integer>
      :since: 3.7

      Create a file with less than 8 bits per sample by
      passing a value from 1 to 7. The apparent pixel type should be Byte.
      Values of n=9...15 (UInt16 type) and n=17...31
      (UInt32 type) are also accepted. n=16 is accepted for
      Float32 type to generate half-precision floating point values.

-  .. co:: PREDICTOR
      :choices: YES, NO, STANDARD, FLOATING_POINT
      :default: NO

      Set the predictor for LZW,
      DEFLATE and ZSTD compression. If YES is specified, then
      standard predictor (Predictor=2) is used for integer data type,
      and floating-point predictor (Predictor=3) for floating point data type (in
      some circumstances, the standard predictor might perform better than the
      floating-point one on floating-point data). STANDARD or FLOATING_POINT can
      also be used to select the precise algorithm wished.

-  .. co:: BIGTIFF
      :choices: YES, NO, IF_NEEDED, IF_SAFER

      Control whether the created
      file is a BigTIFF or a classic TIFF.

      -  ``YES`` forces BigTIFF.
      -  ``NO`` forces classic TIFF.
      -  ``IF_NEEDED`` will only create a BigTIFF if it is clearly needed (in
         the uncompressed case, and image larger than 4GB. So no effect
         when using a compression).
      -  ``IF_SAFER`` will create BigTIFF if the resulting file \*might\*
         exceed 4GB. Note: this is only a heuristics that might not always
         work depending on compression ratios.

      BigTIFF is a TIFF variant which can contain more than 4GiB of data
      (size of classic TIFF is limited by that value). This option is
      available if GDAL is built with libtiff library version 4.0 or
      higher. The default is IF_NEEDED.

      When creating a new GeoTIFF with no compression, GDAL computes in
      advance the size of the resulting file. If that computed file size is
      over 4GiB, GDAL will automatically decide to create a BigTIFF file.
      However, when compression is used, it is not possible in advance to
      known the final size of the file, so classical TIFF will be chosen.
      In that case, the user must explicitly require the creation of a
      BigTIFF with BIGTIFF=YES if the final file is anticipated to be too
      big for classical TIFF format. If BigTIFF creation is not explicitly
      asked or guessed and the resulting file is too big for classical
      TIFF, libtiff will fail with an error message like
      "TIFFAppendToStrip:Maximum TIFF file size exceeded".

-  .. co:: RESAMPLING
      :choices: NEAREST, AVERAGE, BILINEAR, CUBIC, CUBICSPLINE, LANCZOS, MODE, RMS

      Resampling method used for overview generation or reprojection.
      For paletted images,
      NEAREST is used by default, otherwise it is CUBIC.

-  .. co:: OVERVIEW_RESAMPLING
      :choices: NEAREST, AVERAGE, BILINEAR, CUBIC, CUBICSPLINE, LANCZOS, MODE, RMS
      :since: 3.2

      Resampling method used for overview generation.
      For paletted images, NEAREST is used by default, otherwise it is CUBIC.
      This overrides, for overview generation, the value of :co:`RESAMPLING` if it specified.

-  .. co:: WARP_RESAMPLING
      :choices: NEAREST, AVERAGE, BILINEAR, CUBIC, CUBICSPLINE, LANCZOS, MODE, RMS, MIN, MAX, MED, Q1, Q3
      :since: 3.2

      Resampling method used for reprojection.
      For paletted images, NEAREST is used by default, otherwise it is CUBIC.
      This overrides, for reprojection, the value of :co:`RESAMPLING` if it specified.

- .. co:: OVERVIEWS
     :choices: AUTO, IGNORE_EXISTING, FORCE_USE_EXISTING, NONE
     :default: AUTO

     Describe the behavior
     regarding overview generation and use of source overviews.

     - ``AUTO`` (default): source overviews will be used if present.
       If not present, overviews will be automatically generated in the
       output file.

     - ``IGNORE_EXISTING``: potential existing overviews on the source dataset will
       be ignored and new overviews will be automatically generated.

     - ``FORCE_USE_EXISTING``: potential existing overviews on the source will
       be used.
       If there is no source overview, this is equivalent to specifying ``NONE``.

     - ``NONE``: potential source overviews will be ignored, and no overview will be
       generated.

       .. note::

           When using the gdal_translate utility, source overviews will not be
           available if general options (i.e. options which are not creation options,
           like subsetting, etc.) are used.

- .. co:: OVERVIEW_COUNT
     :choices: <integer>
     :since: 3.6

     Number of overview levels to generate. This can be used to increase or decrease
     the number of levels in the COG file (when GDAL computes overviews from the
     full resolution dataset, that is when there are no source overviews or the user
     specifies :co:`OVERVIEWS=IGNORE_EXISTING`), or decrease the number of levels copied
     from the source dataset (in :co:`OVERVIEWS=AUTO` or ``FORCE_USE_EXISTING`` modes when
     there are such overviews in the source dataset).

     If not specified, the driver will use all the overviews available in the source raster,
     in :co:`OVERVIEWS=AUTO` or ``FORCE_USE_EXISTING`` modes. In situations where GDAL generates
     overviews, the default number of overview levels is such that the dimensions of
     the smallest overview are smaller or equal to the :co:`BLOCKSIZE` value.

- .. co:: OVERVIEW_COMPRESS
     :choices: AUTO, NONE, LZW, JPEG, DEFLATE, ZSTD, WEBP, LERC, LERC_DEFLATE, LERC_ZSTD, LZMA
     :default: AUTO

     Set the compression method (see ``COMPRESS``) to use when storing the overviews in the COG.

     By default (``AUTO``) the overviews will be created with the same compression method as the COG.

- .. co:: OVERVIEW_QUALITY
     :choices: <integer>

     JPEG/WEBP quality setting. A value of 100 is best
     quality (least compression), and 1 is worst quality (best compression).
     By default the overviews will be created with the same quality as the COG, unless
     the compression type is different then the default is 75.

- .. co:: OVERVIEW_PREDICTOR
     :choices: YES, NO, STANDARD, FLOATING_POINT

     Set the predictor for LZW,
     DEFLATE and ZSTD overview compression. By default the overviews will be created with the
     same predictor as the COG, unless the compression type of the overview is different,
     then the default is NO.

- .. co:: GEOTIFF_VERSION
     :choices: AUTO, 1.0,1.1
     :default: AUTO

     Select the version of
     the GeoTIFF standard used to encode georeferencing information. ``1.0``
     corresponds to the original
     `1995, GeoTIFF Revision 1.0, by Ritter & Ruth <http://geotiff.maptools.org/spec/geotiffhome.html>`_.
     ``1.1`` corresponds to the OGC standard 19-008, which is an evolution of 1.0,
     which clear ambiguities and fix inconsistencies mostly in the processing of
     the vertical part of a CRS.
     ``AUTO`` mode (default value) will generally select 1.0, unless the CRS to
     encode has a vertical component or is a 3D CRS, in which case 1.1 is used.

     .. note:: Write support for GeoTIFF 1.1 requires libgeotiff 1.6.0 or later.

- .. co:: SPARSE_OK
     :choices: TRUE, FALSE
     :default: FALSE
     :since: 3.2

     Should empty blocks be
     omitted on disk? When this option is set, any attempt of writing a
     block whose all pixels are 0 or the nodata value will cause it not to
     be written at all (unless there is a corresponding block already
     allocated in the file). Sparse files have 0 tile/strip offsets for
     blocks never written and save space; however, most non-GDAL packages
     cannot read such files.
     On the reading side, the presence of a omitted tile after a non-empty one
     may cause optimized readers to have to issue an extra GET request to the
     TileByteCounts array.

- .. co:: STATISTICS
     :choices: AUTO, YES, NO
     :default: AUTO
     :since: 3.8

     Whether band statistics should be included in the output file.
     In ``AUTO`` mode, they will be included only if available in the source
     dataset.
     If setting to ``YES``, they will always be included.
     If setting to ``NO``, they will be never included.

Reprojection related creation options
*************************************

- .. co:: TILING_SCHEME
     :choices: CUSTOM, GoogleMapsCompatible, ...
     :default: CUSTOM

     If set to a value different than CUSTOM, the definition of the specified tiling
     scheme will be used to reproject the dataset to its CRS, select the resolution
     corresponding to the closest zoom level and align on tile boundaries at this
     resolution (the actual resolution can be controlled with the :co:`ZOOM_LEVEL` or
     :co:`ZOOM_LEVEL_STRATEGY` options).

     The tile size indicated in the tiling scheme definition (generally
     256 pixels) will be used, unless the user has specified a value with the
     :co:`BLOCKSIZE` creation option, in which case the user specified one will be taken
     into account (that is if setting a higher value than 256, the original
     tiling scheme is modified to take into account the size of the HiDPi tiles).

     In non-CUSTOM mode, TARGET_SRS, RES and EXTENT options are ignored.
     Starting with GDAL 3.2, the value of :co:`TILING_SCHEME` can also be the filename
     of a JSON file according to the `OGC Two Dimensional Tile Matrix Set standard`_,
     a URL to such file, the radical of a definition file in the GDAL data directory
     (e.g. ``FOO`` for a file named ``tms_FOO.json``) or the inline JSON definition.
     The list of available tiling schemes can be obtained by looking at values of
     the TILING_SCHEME option reported by ``gdalinfo --format COG``.

     .. _`OGC Two Dimensional Tile Matrix Set standard`: http://docs.opengeospatial.org/is/17-083r2/17-083r2.html

- .. co:: ZOOM_LEVEL
     :choices: <integer>
     :since: 3.5

     Zoom level number (starting at 0 for
     coarsest zoom level). Only used for :co:`TILING_SCHEME` different from CUSTOM.
     If this option is specified, :co:`ZOOM_LEVEL_STRATEGY` is ignored.

- .. co:: ZOOM_LEVEL_STRATEGY
     :choices: AUTO, LOWER, UPPER
     :default: AUTO
     :since: 3.2

     Strategy to determine
     zoom level. Only used for :co:`TILING_SCHEME` different from CUSTOM.
     LOWER will select the zoom level immediately below the
     theoretical computed non-integral zoom level, leading to subsampling.
     On the contrary, UPPER will select the immediately above zoom level,
     leading to oversampling. Defaults to AUTO which selects the closest
     zoom level.

- .. co:: TARGET_SRS

     to force reprojection of the input dataset to another
     SRS. The string can be a WKT string, a EPSG:XXXX code or a PROJ string.

- .. co:: RES

     Set the resolution of the target raster, in the units of
     :co:`TARGET_SRS`. Only taken into account if :co:`TARGET_SRS` is specified.

- .. co:: EXTENT
     :choices: <minx\,miny\,maxx\,maxy>

     Set the extent of the target raster, in the
     units of :co:`TARGET_SRS`. Only taken into account if :co:`TARGET_SRS` is specified.

- .. co:: ALIGNED_LEVELS
     :choices: <integer>

     Number of resolution levels for which GeoTIFF tile and
     tiles defined in the tiling scheme match. When specifying this option, padding tiles will be
     added to the left and top sides of the target raster, when needed, so that
     a GeoTIFF tile matches with a tile of the tiling scheme.
     Only taken into account if :co:`TILING_SCHEME` is different from CUSTOM.
     Effect of this option is only visible when setting it at 2 or more, since the
     full resolution level is by default aligned with the tiling scheme.
     For a tiling scheme whose consecutive zoom level resolutions differ by a
     factor of 2, care must be taken in setting this value to a high number of
     levels, as up to 2^(ALIGNED_LEVELS-1) tiles can be added in each dimension.
     The driver enforces a hard limit of 10.

- .. co:: ADD_ALPHA
     :choices: YES, NO
     :default: YES

     Whether an alpha band is added in case of reprojection.

Update
------

Updating a COG file generally breaks part of the optimizations, but still
produces a valid GeoTIFF file. Starting with GDAL 3.8, to avoid undesired loss
of the COG characteristics, opening such a file in update mode will be rejected,
unless the IGNORE_COG_LAYOUT_BREAK open option is also explicitly set to YES.

Note that a subset of operations are possible when opening a COG file in
read-only mode, like metadata edition (including statistics storage), that will
be stored in a auxiliary .aux.xml side-car file.

File format details
-------------------

High level
**********

A Cloud optimized GeoTIFF has the following characteristics:

- TIFF or BigTIFF file
- Tiled (512 pixels by default) for imagery, mask and overviews
- Overviews until the maximum dimension of the smallest overview level is
  lower than 512 pixels.
- Compressed or not
- Pixel interleaving for multi-band dataset
- Optimized layout of TIFF sections to minimize the number of GET requests
  needed by a reader doing random read access.

Low level
*********

A COG file is organized as the following (if using libtiff >= 4.0.11 or GDAL
internal libtiff. For other versions, the layout will be different and some of
the optimizations will not be available).

- TIFF/BigTIFF header/signature and pointer to first IFD (Image File Directory)
- "ghost area" with COG optimizations (see `Header ghost area`_)
- IFD of the full resolution image, followed by TIFF tags values, excluding the
  TileOffsets and TileByteCounts arrays.
- IFD of the mask of the full resolution image, if present, followed by TIFF
  tags values, excluding the TileOffsets and TileByteCounts arrays.
- IFD of the first (largest in dimensions) overview level, if present
- ...
- IFD of the last (smallest) overview level, if present
- IFD of the first (largest in dimensions) overview level of the mask, if present
- ...
- IFD of the last (smallest) overview level of the mask, if present
- TileOffsets and TileByteCounts arrays of the above IFDs
- tile data of the smallest overview, if present (with each tile followed by the
  corresponding tile of mask data, if present),
  with :ref:`leader and trailer bytes <cog.tile_data_leader_trailer>`
- ...
- tile data of the largest overview, if present (interleaved with mask data if present)
- tile data of the full resolution image, if present (interleaved with corresponding  mask data if present)

Header ghost area
*****************

To describe the specific layout of COG files, a
description of the features used is located at the beginning of the file, so that
optimized readers (like GDAL) can use them and take shortcuts. Those features
are described as ASCII strings "hidden" just after the 8 first bytes of a
ClassicTIFF (or after the 16 first ones for a BigTIFF). That is the first IFD
starts just after those strings. It is completely valid to have *ghost*
areas like this in a TIFF file, and readers will normally skip over them. So
for a COG file with a transparency mask, those strings will be:

::

    GDAL_STRUCTURAL_METADATA_SIZE=000174 bytes
    LAYOUT=IFDS_BEFORE_DATA
    BLOCK_ORDER=ROW_MAJOR
    BLOCK_LEADER=SIZE_AS_UINT4
    BLOCK_TRAILER=LAST_4_BYTES_REPEATED
    KNOWN_INCOMPATIBLE_EDITION=NO
    MASK_INTERLEAVED_WITH_IMAGERY=YES

.. note::

    - A newline character `\\n` is used to separate those strings.
    - A space character is inserted after the newline following `KNOWN_INCOMPATIBLE_EDITION=NO`
    - For a COG without mask, the `MASK_INTERLEAVED_WITH_IMAGERY` item will not be present of course.

The ghost area starts with ``GDAL_STRUCTURAL_METADATA_SIZE=XXXXXX bytes\n`` (of
a fixed size of 43 bytes) where XXXXXX is a 6-digit number indicating the remaining
size of the section (that is starting after the linefeed character of this starting
line).

- ``LAYOUT=IFDS_BEFORE_DATA``: the IFDs are located at the beginning of the file.
  GDAL will also makes sure that the tile index arrays are written
  just after the IFDs and before the imagery, so that a first range request of
  16 KB will always get all the IFDs

- ``BLOCK_ORDER=ROW_MAJOR``: (strile is a contraction of 'strip or tile') the
  data for tiles is written in increasing tile id order. Future enhancements
  could possibly implement other layouts.

- ``BLOCK_LEADER=SIZE_AS_UINT4``: each tile data is preceded by 4 bytes, in a
  *ghost* area as well, indicating the real tile size (in little endian order).
  See `Tile data leader and trailer`_ for more details.

- ``BLOCK_TRAILER=LAST_4_BYTES_REPEATED``: just after the tile data, the last 4
  bytes of the tile data are repeated. See `Tile data leader and trailer`_ for more details.

- ``KNOWN_INCOMPATIBLE_EDITION=NO``: when a COG is generated this is always
  written. If GDAL is then used to modify the COG file, as most of the changes
  done on an existing COG file, will break the optimized structure, GDAL will
  change this metadata item to KNOWN_INCOMPATIBLE_EDITION=YES, and issue a
  warning on writing, and when reopening such file, so that users know they have
  *broken* their COG file

- ``MASK_INTERLEAVED_WITH_IMAGERY=YES``: indicates that mask data immediately
  follows imagery data. So when reading data at offset=TileOffset[i] - 4 and
  size=TileOffset[i+1]-TileOffset[i]+4, you'll get a buffer with:

   * leader with imagery tile size (4 bytes)
   * imagery data (starting at TileOffsets[i] and of size TileByteCounts[i])
   * trailer of imagery (4 bytes)
   * leader with mask tilesize (4 bytes)
   * mask data (starting at mask.TileOffsets[i] and of size
     mask.TileByteCounts[i], but none of them actually need to be read)
   * trailer of mask data (4 bytes)

   This is only written if INTERLEAVE=PIXEL.

- ``INTERLEAVE=BAND`` or ``INTERLEAVE=TILE``: (GDAL >= 3.11)
  Reflects the value of the INTERLEAVE creation option.
  Omission implies INTERLEAVE=PIXEL.


.. note::

    The content of the header ghost area can be retrieved by getting the
    ``GDAL_STRUCTURAL_METADATA`` metadata item of the ``TIFF`` metadata domain
    on the dataset object (with GetMetadataItem())

.. _cog.tile_data_leader_trailer:

Tile data leader and trailer
****************************

Each tile data is immediately preceded by a leader, consisting of a unsigned 4-byte integer,
in little endian order, giving the number of bytes of *payload* of the tile data
that follows it. This leader is *ghost* in the sense that the
TileOffsets[] array does not point to it, but points to the real payload. Hence
the offset of the leader is TileOffsets[i]-4.

For INTERLEAVE=PIXEL or INTERLEAVE=TILE, an optimized reader seeing the
``BLOCK_LEADER=SIZE_AS_UINT4`` metadata item will thus look for TileOffset[i]
and TileOffset[i+1] to deduce it must fetch the data starting at
offset=TileOffset[i] - 4 and of size=TileOffset[i+1]-TileOffset[i]+4. It then
checks the 4 first bytes to see if the size in this leader marker is
consistent with TileOffset[i+1]-TileOffset[i]. When there is no mask, they
should normally be equal (modulo the size taken by BLOCK_LEADER and
BLOCK_TRAILER). In the case where there is a mask and
MASK_INTERLEAVED_WITH_IMAGERY=YES, then the tile size indicated in the leader
will be < TileOffset[i+1]-TileOffset[i] since the data for the mask will
follow the imagery data (see MASK_INTERLEAVED_WITH_IMAGERY=YES)

For INTERLEAVE=BAND, the above paragraph applies but the successor of tile i
is not tile i+1, but tile i+nTilesPerBand.

Each tile data is immediately followed by a trailer, consisting of the repetition
of the last 4 bytes of the payload of the tile data. The size of this trailer is
*not* included in the TileByteCounts[] array. The purpose of this trailer is forces
readers to be able to check if TIFF writers, not aware of those optimizations,
have modified the  TIFF file in a way that breaks the optimizations. If an optimized reader
detects an inconsistency, it can then fallbacks to the regular/slower method of using
TileOffsets[i] + TileByteCounts[i].

Examples
--------

::

    gdalwarp src1.tif src2.tif out.tif -of COG

::

    gdal_translate world.tif world_webmerc_cog.tif -of COG -co TILING_SCHEME=GoogleMapsCompatible -co COMPRESS=JPEG

See Also
--------

- :ref:`raster.gtiff` driver
- If your source dataset is an internally tiled geotiff with the desired georeferencing and compression,
  using `cogger <https://github.com/airbusgeo/cogger>`__ (possibly along with gdaladdo to create overviews) will
  be much faster than the COG driver.


.. below is an allow-list for spelling checker.

.. spelling:word-list::
      nTilesPerBand
