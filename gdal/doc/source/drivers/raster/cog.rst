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

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Creation Options
----------------

General creation options
************************

-  **BLOCKSIZE=n**: Sets the tile width and height in pixels. Defaults to 512.

-  **COMPRESS=[NONE/LZW/JPEG/DEFLATE/ZSTD/WEBP/LERC/LERC_DEFLATE/LERC_ZSTD]**: Set the compression to use.
   Defaults to ``NONE``.

   * ``JPEG`` should generally only be used with
     Byte data (8 bit per channel). But if GDAL is built with internal libtiff and
     libjpeg, it is    possible to read and write TIFF files with 12bit JPEG compressed TIFF
     files (seen as UInt16 bands with NBITS=12). See the `"8 and 12 bit
     JPEG in TIFF" <http://trac.osgeo.org/gdal/wiki/TIFF12BitJPEG>`__ wiki
     page for more details.
     For the COG driver, JPEG compression for 3 or 4-band images automatically
     selects the PHOTOMETRIC=YCBCR colorspace with a 4:2:2 subsampling of the Y,Cb,Cr
     components.

   * ``LZW``, ``DEFLATE`` and ``ZSTD`` compressions can be used with the PREDICTOR creation option.

   * ``ZSTD`` is available when using internal libtiff and if GDAL built against
     libzstd >=1.0, or if built against external libtiff with zstd support.

   * ``LERC`` is available when using internal libtiff.

   * ``LERC_ZSTD`` is available when ``LERC`` and ``ZSTD`` are available.

-  **LEVEL=integer_value**: DEFLATE/ZSTD/LERC_DEFLATE/LERC_ZSTD compression level.
   A lower number will
   result in faster compression but less efficient compression rate.
   1 is the fastest.

   * For DEFLATE, 9 is the slowest/higher compression rate
     (or 12 when using a libtiff with libdeflate support). The default is 6.
   * For ZSTD, 22 is the slowest/higher compression rate. The default is 9.

-  **MAX_Z_ERROR=threshold**: Set the maximum error threshold on values
   for LERC/LERC_DEFLATE/LERC_ZSTD compression. The default is 0
   (lossless).

-  **QUALITY=integer_value**: JPEG/WEBP quality setting. A value of 100 is best
   quality (least compression), and 1 is worst quality (best compression).
   The default is 75. For WEBP, QUALITY=100 automatically turns on lossless mode.

-  **NUM_THREADS=number_of_threads/ALL_CPUS**: Enable
   multi-threaded compression by specifying the number of worker
   threads. Default is compression in the main thread. This also determines
   the number of threads used when reprojection is done with the TILING_SCHEME
   or TARGET_SRS creation options. (Overview generation is also multithreaded since
   GDAL 3.2)

-  **PREDICTOR=[YES/NO/STANDARD/FLOATING_POINT]**: Set the predictor for LZW,
   DEFLATE and ZSTD compression. The default is NO. If YES is specified, then
   standard predictor (Predictor=2) is used for integer data type,
   and floating-point predictor (Predictor=3) for floating point data type (in
   some circumstances, the standard predictor might perform better than the
   floating-point one on floating-point data). STANDARD or FLOATING_POINT can
   also be used to select the precise algorithm wished.

-  **BIGTIFF=YES/NO/IF_NEEDED/IF_SAFER**: Control whether the created
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

-  **RESAMPLING=[NEAREST/AVERAGE/BILINEAR/CUBIC/CUBICSPLINE/LANCZOS]**:
   Resampling method used for overview generation or reprojection.
   For paletted images,
   NEAREST is used by default, otherwise it is CUBIC.

-  **OVERVIEW_RESAMPLING=[NEAREST/AVERAGE/BILINEAR/CUBIC/CUBICSPLINE/LANCZOS]**:
   (since GDAL 3.2)
   Resampling method used for overview generation.
   For paletted images, NEAREST is used by default, otherwise it is CUBIC.
   This overrides, for overview generation, the value of ``RESAMPLING`` if it specified.

-  **WARP_RESAMPLING=[NEAREST/AVERAGE/BILINEAR/CUBIC/CUBICSPLINE/LANCZOS]**:
   (since GDAL 3.2)
   Resampling method used for reprojection.
   For paletted images, NEAREST is used by default, otherwise it is CUBIC.
   This overrides, for reprojection, the value of ``RESAMPLING`` if it specified.

- **OVERVIEWS=[AUTO/IGNORE_EXISTING/FORCE_USE_EXISTING/NONE]**: Describe the behavior
  regarding overview generation and use of source overviews.

  - ``AUTO`` (default): source overviews will be
    used if present (even if the dimension of the smallest level is not < 512 pixels),
    and, if not present, overviews will be automatically generated in the
    output file.

  - ``IGNORE_EXISTING``: potential existing overviews on the source dataset will
    be ignored and new overviews will be automatically generated.

  - ``FORCE_USE_EXISTING``: potential existing overviews on the source will
    be used (even if the dimension of the smallest level is not < 512 pixels).
    If there is no source overview, this is equivalent to specifying ``NONE``.

  - ``NONE``: potential source overviews will be ignored, and no overview will be
    generated.

    .. note::

        When using the gdal_translate utility, source overviews will not be
        available if general options (i.e. options which are not creation options,
        like subsetting, etc.) are used.

- **OVERVIEW_COMPRESS=[AUTO/NONE/LZW/JPEG/DEFLATE/ZSTD/WEBP/LERC/LERC_DEFLATE/LERC_ZSTD]**:
  Set the compression method (see ``COMPRESS``) to use when storing the overviews in the COG.
  
  By default (``AUTO``) the overviews will be created with the same compression method as the COG.

- **OVERVIEW_QUALITY=integer_value**: JPEG/WEBP quality setting. A value of 100 is best
  quality (least compression), and 1 is worst quality (best compression).
  By default the overviews will be created with the same quality as the COG, unless
  the compression type is different then the default is 75.

- **OVERVIEW_PREDICTOR=[YES/NO/STANDARD/FLOATING_POINT]**: Set the predictor for LZW,
  DEFLATE and ZSTD overview compression. By default the overviews will be created with the
  same predictor as the COG, unless the compression type of the overview is different,
  then the default is NO.

- **GEOTIFF_VERSION=[AUTO/1.0/1.1]**: Select the version of
  the GeoTIFF standard used to encode georeferencing information. ``1.0``
  corresponds to the original
  `1995, GeoTIFF Revision 1.0, by Ritter & Ruth <http://geotiff.maptools.org/spec/geotiffhome.html>`_.
  ``1.1`` corresponds to the OGC standard 19-008, which is an evolution of 1.0,
  which clear ambiguities and fix inconsistencies mostly in the processing of
  the vertical part of a CRS.
  ``AUTO`` mode (default value) will generally select 1.0, unless the CRS to
  encode has a vertical component or is a 3D CRS, in which case 1.1 is used.

  .. note:: Write support for GeoTIFF 1.1 requires libgeotiff 1.6.0 or later.

- **SPARSE_OK=TRUE/FALSE** ((GDAL >= 3.2): Should empty blocks be
  omitted on disk? When this option is set, any attempt of writing a
  block whose all pixels are 0 or the nodata value will cause it not to
  be written at all (unless there is a corresponding block already
  allocated in the file). Sparse files have 0 tile/strip offsets for
  blocks never written and save space; however, most non-GDAL packages
  cannot read such files.
  On the reading side, the presence of a omitted tile after a non-empty one
  may cause optimized readers to have to issue an extra GET request to the
  TileByteCounts array.
  The default is FALSE.

Reprojection related creation options
*************************************

- **TILING_SCHEME=CUSTOM/GoogleMapsCompatible/other**: Default value: CUSTOM.
  If set to a value different than CUSTOM, the definition of the specified tiling
  scheme will be used to reproject the dataset to its CRS, select the resolution
  corresponding to the closest zoom level and align on tile boundaries at this
  resolution. The tile size indicated in the tiling scheme definition (generally
  256 pixels) will be used, unless the user has specified a value with the
  BLOCKSIZE creation option, in which case the user specified one will be taken
  into account (that is if setting a higher value than 256, the original
  tiling scheme is modified to take into account the size of the HiDPi tiles).
  In non-CUSTOM mode, TARGET_SRS, RES and EXTENT options are ignored.
  Starting with GDAL 3.2, the value of TILING_SCHEME can also be the filename
  of a JSON file according to the `OGC Two Dimensional Tile Matrix Set standard`_,
  a URL to such file, the radical of a definition file in the GDAL data directory
  (e.g. ``FOO`` for a file named ``tms_FOO.json``) or the inline JSON definition.

.. _`OGC Two Dimensional Tile Matrix Set standard`: http://docs.opengeospatial.org/is/17-083r2/17-083r2.html

- **ZOOM_LEVEL_STRATEGY**\ =AUTO/LOWER/UPPER. (GDAL >= 3.2) Strategy to determine
  zoom level. Only used for TILING_SCHEME different from CUSTOM.
  LOWER will select the zoom level immediately below the
  theoretical computed non-integral zoom level, leading to subsampling.
  On the contrary, UPPER will select the immediately above zoom level,
  leading to oversampling. Defaults to AUTO which selects the closest
  zoom level.

- **TARGET_SRS=string**: to force reprojection of the input dataset to another
  SRS. The string can be a WKT string, a EPSG:XXXX code or a PROJ string.

- **RES=value**: Set the resolution of the target raster, in the units of
  TARGET_SRS. Only taken into account if TARGET_SRS is specified.

- **EXTENT=minx,miny,maxx,maxy**: Set the extent of the target raster, in the
  units of TARGET_SRS. Only taken into account if TARGET_SRS is specified.

- **ALIGNED_LEVELS=INT**: Number of overview levels for which GeoTIFF tile and
  tiles defined in the tiling scheme match. When specifying this option, padding tiles will be
  added to the left and top sides of the target raster, when needed, so that
  a GeoTIFF tile matches with a tile of the tiling scheme.
  Only taken into account if TILING_SCHEME is different from CUSTOM.
  For a tiling scheme whose consecutive zoom level resolutions differ by a
  factor of 2, care must be taken in setting this value to a high number of
  levels, as up to 2^(ALIGNED_LEVELS-1) tiles can be added in each dimension.
  The driver enforces a hard limit of 10.

- **ADD_ALPHA=YES/NO**: Whether an alpha band is added in case of reprojection.
  Defaults to YES.


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

.. note::

    The content of the header ghost area can be retrieved by getting the
    ``GDAL_STRUCTURAL_METADATA`` metadata item of the ``TIFF`` metadata domain
    on the datasett object (with GetMetadataItem())

.. _cog.tile_data_leader_trailer:

Tile data leader and trailer
****************************

Each tile data is immediately preceded by a leader, consisting of a unsigned 4-byte integer,
in little endian order, giving the number of bytes of *payload* of the tile data
that follows it. This leader is *ghost* in the sense that the
TileOffsets[] array does not point to it, but points to the real payload. Hence
the offset of the leader is TileOffsets[i]-4.

An optimized reader seeing the ``BLOCK_LEADER=SIZE_AS_UINT4`` metadata item will thus look for TileOffset[i]
and TileOffset[i+1] to deduce it must fetch the data starting at
offset=TileOffset[i] - 4 and of size=TileOffset[i+1]-TileOffset[i]+4. It then
checks the 4 first bytes to see if the size in this leader marker is
consistent with TileOffset[i+1]-TileOffset[i]. When there is no mask, they
should normally be equal (modulo the size taken by BLOCK_LEADER and
BLOCK_TRAILER). In the case where there is a mask and
MASK_INTERLEAVED_WITH_IMAGERY=YES, then the tile size indicated in the leader
will be < TileOffset[i+1]-TileOffset[i] since the data for the mask will
follow the imagery data (see MASK_INTERLEAVED_WITH_IMAGERY=YES)

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
-  `How to generate and read cloud optimized GeoTIFF
   files <https://trac.osgeo.org/gdal/wiki/CloudOptimizedGeoTIFF>`__ (before GDAL 3.1)
