.. _raster.gtiff:

================================================================================
GTiff -- GeoTIFF File Format
================================================================================

.. shortname:: GTiff

.. built_in_by_default::

Most forms of TIFF and GeoTIFF files are supported by GDAL for reading,
and somewhat less varieties can be written.

GDAL also supports reading and writing BigTIFF files (evolution of the TIFF format
to support files larger than 4 GB).

Currently band types of Byte, UInt16, Int16, UInt32, Int32, Float32,
Float64, CInt16, CInt32, CFloat32 and CFloat64 are supported for reading
and writing. Paletted images will return palette information associated
with the band. The compression formats listed below should be supported
for reading as well.

As well, one bit files, and some other unusual formulations of GeoTIFF
file, such as YCbCr color model files, are automatically translated into
RGBA (red, green, blue, alpha) form, and treated as four eight bit
bands.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Georeferencing
--------------

Most GeoTIFF projections should be supported, with the caveat that in
order to translate uncommon Projected, and Geographic coordinate systems
into OGC WKT it is necessary to have the PROJ proj.db database
available. It must be found at the location pointed to by the PROJ_LIB
environment variable, or at one of the locations set programmatically
via OSRSetPROJSearchPaths().

Georeferencing from GeoTIFF is supported in the form of one tiepoint and
pixel size, a transformation matrix, or a list of GCPs.

If no georeferencing information is available in the TIFF file itself,
GDAL will also check for, and use an ESRI :ref:`world
file <raster.wld>` with the extension .tfw, .tifw/.tiffw or
.wld, as well as a MapInfo .tab file.

By default, information is fetched in following order (first listed is
the most prioritary): PAM (Persistent Auxiliary metadata) .aux.xml
sidecar file, INTERNAL (GeoTIFF keys and tags), TABFILE (.tab),
WORLDFILE (.tfw, .tifw/.tiffw or .wld).

Starting with GDAL 2.2, the allowed sources and their priority order can
be changed with the GDAL_GEOREF_SOURCES configuration option (or
GEOREF_SOURCES open option) whose value is a comma-separated list of the
following keywords : PAM, INTERNAL, TABFILE, WORLDFILE, NONE. First
mentioned sources are the most prioritary over the next ones. A non
mentioned source will be ignored.

For example setting it to "WORLDFILE,PAM,INTERNAL" will make a
geotransformation matrix from a potential worldfile prioritary over PAM
or GeoTIFF.

GDAL can read and write the *RPCCoefficientTag* as described in the
`RPCs in GeoTIFF <http://geotiff.maptools.org/rpc_prop.html>`__ proposed
extension. The tag is written only for files created with the default
profile GDALGeoTIFF. For other profiles, a .RPB file is created. In GDAL
data model, the RPC coefficients are stored into the RPC metadata
domain. For more details, see the :ref:`rfc-22`. If .RPB or \_RPC.TXT
files are found, they will be used to read the RPCs, even if the
*RPCCoefficientTag* tag is set.

Internal nodata masks
---------------------

TIFF files can contain internal transparency masks. The GeoTIFF driver
recognizes an internal directory as being a transparency mask when the
FILETYPE_MASK bit value is set on the TIFFTAG_SUBFILETYPE tag. According
to the TIFF specification, such internal transparency masks contain 1
sample of 1-bit data. Although the TIFF specification allows for higher
resolutions for the internal transparency mask, the GeoTIFF driver only
supports internal transparency masks of the same dimensions as the main
image. Transparency masks of internal overviews are also supported.

When the GDAL_TIFF_INTERNAL_MASK configuration option is set to YES and
the GeoTIFF file is opened in update mode, the CreateMaskBand() method
on a TIFF dataset or rasterband will create an internal transparency
mask. Otherwise, the default behavior of nodata mask creation will be
used, that is to say the creation of a .msk file, as per :ref:`rfc-15`.

1-bit internal mask band are deflate compressed. When reading them back,
to make conversion between mask band and alpha band easier, mask bands
are exposed to the user as being promoted to full 8 bits (i.e. the value
for unmasked pixels is 255) unless the GDAL_TIFF_INTERNAL_MASK_TO_8BIT
configuration option is set to NO. This does not affect the way the mask
band is written (it is always 1-bit).

Overviews
---------

The GeoTIFF driver supports reading, creation and update of internal
overviews. Internal overviews can be created on GeoTIFF files opened in
update mode (with gdaladdo for instance). If the GeoTIFF file is opened
as read only, the creation of overviews will be done in an external .ovr
file. Overview are only updated on request with the BuildOverviews()
method.

The block size (tile width and height) used for overviews (internal or
external) can be specified by setting the GDAL_TIFF_OVR_BLOCKSIZE
environment variable to a power-of-two value between 64 and 4096. The
default is 128, or starting with GDAL 3.1 to use the same block size
as the full-resolution dataset if possible (i.e. block height and width
are equal, a power-of-two, and between 64 and 4096).

Overviews and nodata masks
--------------------------

The following configurations can be encountered depending if overviews
and nodata masks are internal or not.

There are two well supported configurations:

-  Internal overviews, internal nodata mask: If a GeoTIFF file has a
   internal transparency mask (and the GDAL_TIFF_INTERNAL_MASK
   environment variable is not set to NO) and the GeoTIFF file is opened
   in update mode, BuildOverviews() will automatically create overviews
   for the internal transparency mask.
-  External overviews, external nodata mask: When opened in read-only
   mode, BuildOverviews() will automatically create overviews for the
   external transparency mask (in a .msk.ovr file)

For the two other mixed configurations, behavior is less obvious:

-  Internal overviews, external nodata mask: when running
   BuildOverviews() in update mode on the .tif file, only the overviews
   of the main bands will be generated. Overviews of the external .msk
   file must be explicitly generated by running BuildOverviews() on the
   .msk.
-  External overviews, internal nodata mask: when running
   BuildOverviews() in read-only mode on the .tif file, only the
   overviews of the main bands will be generated. Generating the
   overviews of the internal nodata mask is not currently supported by
   the driver.

Practical note: for a command line point of view, BuildOverview() in
update mode means "gdaladdo the.tiff" (without -ro). Whereas
BuildOverviews() in read-only mode means "gdaladdo -ro the.tiff".

Metadata
--------

GDAL can deal with the following baseline TIFF tags as dataset-level
metadata :

-  TIFFTAG_DOCUMENTNAME
-  TIFFTAG_IMAGEDESCRIPTION
-  TIFFTAG_SOFTWARE
-  TIFFTAG_DATETIME
-  TIFFTAG_ARTIST
-  TIFFTAG_HOSTCOMPUTER
-  TIFFTAG_COPYRIGHT
-  TIFFTAG_XRESOLUTION
-  TIFFTAG_YRESOLUTION
-  TIFFTAG_RESOLUTIONUNIT
-  TIFFTAG_MINSAMPLEVALUE (read only)
-  TIFFTAG_MAXSAMPLEVALUE (read only)
-  `GEO_METADATA <https://www.awaresystems.be/imaging/tiff/tifftags/geo_metadata.html>`__:
   This tag may be used for embedding XML-encoded instance documents
   prepared using 19139-based schema (GeoTIFF DGIWG) (GDAL >= 2.3)
-  `TIFF_RSID <https://www.awaresystems.be/imaging/tiff/tifftags/tiff_rsid.html>`__:
   This tag specifies a File Universal Unique Identifier, or RSID,
   according to DMF definition (GeoTIFF DGIWG) (GDAL >= 2.3)

The name of the metadata item to use is one of the above names
("TIFFTAG_DOCUMENTNAME", ...). On creation, those tags can for example
be set with

::

   gdal_translate in.tif out.tif -mo {TAGNAME}=VALUE

Other non standard metadata items can be stored in a TIFF file created
with the profile GDALGeoTIFF (the default, see below in the Creation
issues section). Those metadata items are grouped together into a XML
string stored in the non standard TIFFTAG_GDAL_METADATA ASCII tag (code
42112). When BASELINE or GeoTIFF profile are used, those non standard
metadata items are stored into a PAM .aux.xml file.

The value of GDALMD_AREA_OR_POINT ("AREA_OR_POINT") metadata item is
stored in the GeoTIFF key RasterPixelIsPoint for GDALGeoTIFF or GeoTIFF
profiles.

XMP metadata can be extracted from the file,
and will be reported as XML raw content in the xml:XMP metadata domain.

EXIF metadata can be extracted from the file,
and will be reported in the EXIF metadata domain.

Color Profile Metadata
----------------------

GDAL can deal with the following color profile
metadata in the COLOR_PROFILE domain:

-  SOURCE_ICC_PROFILE (Base64 encoded ICC profile embedded in file. If
   available, other tags are ignored.)
-  SOURCE_PRIMARIES_RED (xyY in "x,y,1" format for red primary.)
-  SOURCE_PRIMARIES_GREEN (xyY in "x,y,1" format for green primary)
-  SOURCE_PRIMARIES_BLUE (xyY in "x,y,1" format for blue primary)
-  SOURCE_WHITEPOINT (xyY in "x,y,1" format for whitepoint)
-  TIFFTAG_TRANSFERFUNCTION_RED (Red table of TIFFTAG_TRANSFERFUNCTION)
-  TIFFTAG_TRANSFERFUNCTION_GREEN (Green table of
   TIFFTAG_TRANSFERFUNCTION)
-  TIFFTAG_TRANSFERFUNCTION_BLUE (Blue table of
   TIFFTAG_TRANSFERFUNCTION)
-  TIFFTAG_TRANSFERRANGE_BLACK (Min range of TIFFTAG_TRANSFERRANGE)
-  TIFFTAG_TRANSFERRANGE_WHITE (Max range of TIFFTAG_TRANSFERRANGE)

Note that these metadata properties can only be used on the original raw
pixel data. If automatic conversion to RGB has been done, the color
profile information cannot be used.

All these metadata tags can be overridden and/or used as creation
options.

Nodata value
------------

GDAL stores band nodata value in the non standard TIFFTAG_GDAL_NODATA
ASCII tag (code 42113) for files created with the default profile
GDALGeoTIFF. Note that all bands must use the same nodata value. When
BASELINE or GeoTIFF profile are used, the nodata value is stored into a
PAM .aux.xml file.

Sparse files
------------

GDAL makes a special interpretation of a TIFF tile or strip whose offset
and byte count are set to 0, that is to say a tile or strip that has no
corresponding allocated physical storage. On reading, such tiles or
strips are considered to be implicitly set to 0 or to the nodata value
when it is defined. On writing, it is possible to enable generating such
files through the Create() interface by setting the SPARSE_OK creation
option to YES. Then, blocks that are never written through the
IWriteBlock()/IRasterIO() interfaces will have their offset and byte
count set to 0. This is particularly useful to save disk space and time
when the file must be initialized empty before being passed to a further
processing stage that will fill it. To avoid ambiguities with another
sparse mechanism discussed in the next paragraphs, we will call such
files with implicit tiles/strips "TIFF sparse files". They will be
likely **not** interoperable with TIFF readers that are not GDAL based
and would consider such files with implicit tiles/strips as defective.

Starting with GDAL 2.2, this mechanism is extended to the CreateCopy()
and Open() interfaces (for update mode) as well. If the SPARSE_OK
creation option (or the SPARSE_OK open option for Open()) is set to YES,
even an attempt to write a all 0/nodata block will be detected so that
the tile/strip is not allocated (if it was already allocated, then its
content will be replaced by the 0/nodata content).

Starting with GDAL 2.2, in the case where SPARSE_OK is **not** defined
(or set to its default value FALSE), for uncompressed files whose nodata
value is not set, or set to 0, in Create() and CreateCopy() mode, the
driver will delay the allocation of 0-blocks until file closing, so as
to be able to write them at the very end of the file, and in a way
compatible of the filesystem sparse file mechanisms (to be distinguished
from the TIFF sparse file extension discussed earlier). That is that all
the empty blocks will be seen as properly allocated from the TIFF point
of view (corresponding strips/tiles will have valid offsets and byte
counts), but will have no corresponding physical storage. Provided that
the filesystem supports such sparse files, which is the case for most
Linux popular filesystems (ext2/3/4, xfs, btfs, ...) or NTFS on Windows.
If the file system does not support sparse files, physical storage will
be allocated and filled with zeros.

Raw mode
--------

For some TIFF formulations that have "odd" photometric color spaces,
on-the-fly decoding as RGBA is done. This might not be desirable in some
use cases. This behavior can be disabled by prefixing the filename with
GTIFF_RAW:

For example to translate a CMYK file to another one :

::

   gdal_translate GTIFF_RAW:in.tif out.tif -co PHOTOMETRIC=CMYK

Open options
------------

-  **NUM_THREADS=number_of_threads/ALL_CPUS**: (From GDAL 2.1) Enable
   multi-threaded compression by specifying the number of worker
   threads. Worth it for slow compression algorithms such as DEFLATE or
   LZMA. Default is compression in the main thread.

-  **GEOREF_SOURCES=string**: (GDAL > 2.2) Define which georeferencing
   sources are allowed and their priority order. See
   `Georeferencing <#georeferencing>`__ paragraph.

-  **SPARSE_OK=TRUE/FALSE** ((GDAL > 2.2): Should empty blocks be
   omitted on disk? When this option is set, any attempt of writing a
   block whose all pixels are 0 or the nodata value will cause it not to
   be written at all (unless there is a corresponding block already
   allocated in the file). Sparse files have 0 tile/strip offsets for
   blocks never written and save space; however, most non-GDAL packages
   cannot read such files. The default is FALSE.

Creation Issues
---------------

GeoTIFF files can be created with any GDAL defined band type, including
the complex types. Created files may have any number of bands. Files of
type Byte with exactly 3 bands will be given a photometric
interpretation of RGB, files of type Byte with exactly four bands will
have a photometric interpretation of RGBA, while all other combinations
will have a photometric interpretation of MIN_IS_BLACK. Starting with
GDAL 2.2, non-standard (regarding to the intrinsics TIFF capabilities)
band color interpretation, such as BGR ordering, will be handled in
creation and reading, by storing them in the GDAL internal metadata TIFF
tag.

The TIFF format only supports R,G,B components for palettes / color
tables. Thus on writing the alpha information will be silently
discarded.

You may want to read hints to `generate and read cloud optimized GeoTIFF
files <https://trac.osgeo.org/gdal/wiki/CloudOptimizedGeoTIFF>`__

Creation Options
~~~~~~~~~~~~~~~~

-  **TFW=YES**: Force the generation of an associated ESRI world file
   (.tfw).See the :ref:`World Files <raster.wld>` page for details.

-  **RPB=YES**: Force the generation of an associated .RPB file to
   describe RPC (Rational Polynomial Coefficients), if RPC information
   is available. If not specified, this file is automatically generated
   if there's RPC information and that the PROFILE is not the default
   GDALGeoTIFF.

-  **RPCTXT=YES**: Force the generation of an associated
   \_RPC.TXT file to describe RPC (Rational Polynomial Coefficients), if
   RPC information is available.

-  **INTERLEAVE=[BAND,PIXEL]**: By default TIFF files with pixel
   interleaving (PLANARCONFIG_CONTIG in TIFF terminology) are created.
   These are slightly less efficient than BAND interleaving for some
   purposes, but some applications only support pixel interleaved TIFF
   files.

-  **TILED=YES**: By default striped TIFF files are created. This
   option can be used to force creation of tiled TIFF files.

-  **BLOCKXSIZE=n**: Sets tile width, defaults to 256.

-  **BLOCKYSIZE=n**: Set tile or strip height. Tile height defaults to
   256, strip height defaults to a value such that one strip is 8K or
   less.

-  **NBITS=n**: Create a file with less than 8 bits per sample by
   passing a value from 1 to 7. The apparent pixel type should be Byte.
   Values of n=9...15 (UInt16 type) and n=17...31
   (UInt32 type) are also accepted. From GDAL 2.2, n=16 is accepted for
   Float32 type to generate half-precision floating point values.

-  **COMPRESS=[JPEG/LZW/PACKBITS/DEFLATE/CCITTRLE/CCITTFAX3/CCITTFAX4/LZMA/ZSTD/LERC/LERC_DEFLATE/LERC_ZSTD/WEBP/NONE]**:
   Set the compression to use.

   * ``JPEG`` should generally only be used with Byte data (8 bit per channel).
     Better compression for RGB images can be obtained by using the PHOTOMETRIC=YCBCR
     colorspace with a 4:2:2 subsampling of the Y,Cb,Cr components.

     Starting with GDAL 3.4, if GDAL is built with its internal libtiff,
     read and write support for JPEG-in-TIFF compressed images with 12-bit sample
     is enabled by default (if JPEG support is also enabled), using GDAL internal libjpeg
     (based on IJG libjpeg-6b, with additional changes for 12-bit sample support).
     Support for JPEG with 12-bit sample is independent of whether
     8-bit JPEG support is enabled through internal IJG libjpeg-6b or external libjpeg
     (like libjpeg-turbo)

   * ``CCITTFAX3``, ``CCITTFAX4`` or ``CCITRLE`` compression should only be used with 1bit (NBITS=1) data

   * ``LZW``, ``DEFLATE`` and ``ZSTD`` compressions can be used with the PREDICTOR creation option.

   * ``ZSTD`` is available since GDAL 2.3 when using internal libtiff and if GDAL
     built against libzstd >=1.0, or if built against external libtiff with zstd support.

   * ``LERC`` and ``LERC_DEFLATE`` are available only when using internal libtiff for GDAL < 3.3.0.
     Since GDAL 3.3.0, LERC compression is also available when building GDAL
     against external libtiff >= 4.3.0, built itself against https://github.com/esri/lerc

   * ``LERC_ZSTD`` is available when ``LERC`` and ``ZSTD`` are available.

   * ``NONE`` is the default.

-  **NUM_THREADS=number_of_threads/ALL_CPUS**: (From GDAL 2.1) Enable
   multi-threaded compression by specifying the number of worker
   threads. Worth for slow compressions such as DEFLATE or LZMA. Will be
   ignored for JPEG. Default is compression in the main thread.

-  **PREDICTOR=[1/2/3]**: Set the predictor for LZW, DEFLATE and ZSTD
   compression. The default is 1 (no predictor), 2 is horizontal
   differencing and 3 is floating point prediction.

-  **DISCARD_LSB=nbits or nbits_band1,nbits_band2,...nbits_bandN**:
   Set the number of least-significant bits to clear,
   possibly different per band. Lossy compression scheme to be best used
   with PREDICTOR=2 and LZW/DEFLATE/ZSTD compression.

-  **SPARSE_OK=TRUE/FALSE**: Should newly created
   files (through Create() interface) be allowed to be sparse? Sparse
   files have 0 tile/strip offsets for blocks never written and save
   space; however, most non-GDAL packages cannot read such files.
   Starting with GDAL 2.2, SPARSE_OK=TRUE is also supported through the
   CreateCopy() interface. Starting with GDAL 2.2, even an attempt to
   write a block whose all pixels are 0 or the nodata value will cause
   it not to be written at all (unless there is a corresponding block
   already allocated in the file). The default is FALSE.

-  **JPEG_QUALITY=[1-100]**: Set the JPEG quality when using JPEG
   compression. A value of 100 is best quality (least compression), and
   1 is worst quality (best compression). The default is 75.

-  **JPEGTABLESMODE=0/1/2/3**: Configure how and where
   JPEG quantization and Huffman tables are written in the TIFF
   JpegTables tag and strip/tile. Default to 1.

   -  0: JpegTables is not written. Each strip/tile contains its own
      quantization tables and use optimized Huffman coding.
   -  1: JpegTables is written with only the quantization tables. Each
      strip/tile refers to those quantized tables and use optimized
      Huffman coding. This is generally the optimal choice for smallest
      file size, and consequently is the default.
   -  2: JpegTables is written with only the default Huffman tables.
      Each strip/tile refers to those Huffman tables (thus no optimized
      Huffman coding) and contains its own quantization tables
      (identical). This option has no anticipated practical value.
   -  3: JpegTables is written with the quantization and default Huffman
      tables. Each strip/tile refers to those tables (thus no optimized
      Huffman coding). This option could perhaps with some data be more
      efficient than 1, but this should only occur in rare
      circumstances.

-  **ZLEVEL=[1-9] or [1-12]**: Set the level of compression when using DEFLATE
   compression (or LERC_DEFLATE). A value of 9 (resp. 12) is best/slowest when
   using zlib (resp. libdeflate), and 1 is least/fastest compression. The default is 6.

-  **ZSTD_LEVEL=[1-22]**: Set the level of compression when using ZSTD
   compression (or LERC_ZSTD). A value of 22 is best (very slow), and 1
   is least compression. The default is 9.

-  **MAX_Z_ERROR=threshold**: Set the maximum error threshold on values
   for LERC/LERC_DEFLATE/LERC_ZSTD compression. The default is 0
   (lossless).

-  **WEBP_LEVEL=[1-100]**: Set the WEBP quality level when using WEBP
   compression. A value of 100 is best quality (least compression), and
   1 is worst quality (best compression). The default is 75.

-  **WEBP_LOSSLESS=True/False**: (GDAL >= 2.4.0 and libwebp >= 0.1.4):
   By default, lossy compression is used. If set to True, lossless
   compression will be used. There is a significant time penalty for each
   tile/strip with lossless WebP compression, so you may want to increase the
   BLOCKYSIZE value for strip layout.

-  **PHOTOMETRIC=[MINISBLACK/MINISWHITE/RGB/CMYK/YCBCR/CIELAB/ICCLAB/ITULAB]**:
   Set the photometric interpretation tag. Default is MINISBLACK, but if
   the input image has 3 or 4 bands of Byte type, then RGB will be
   selected. You can override default photometric using this option.

-  **ALPHA=[YES/NON-PREMULTIPLIED/PREMULTIPLIED/UNSPECIFIED]**: The
   first "extrasample" is marked as being alpha if there are any extra
   samples. This is necessary if you want to produce a greyscale TIFF
   file with an alpha band (for instance). YES is an alias
   for NON-PREMULTIPLIED alpha.

-  **PROFILE=[GDALGeoTIFF/GeoTIFF/BASELINE]**: Control what non-baseline
   tags are emitted by GDAL.

   -  With ``GDALGeoTIFF`` (the default) various GDAL custom tags may be
      written.
   -  With ``GeoTIFF`` only GeoTIFF tags will be added to the baseline.
   -  With ``BASELINE`` no GDAL or GeoTIFF tags will be written.
      BASELINE is occasionally useful when writing files to be read by
      applications intolerant of unrecognized tags.

-  **BIGTIFF=YES/NO/IF_NEEDED/IF_SAFER**: Control whether the created
   file is a BigTIFF or a classic TIFF.

   -  YES forces BigTIFF.
   -  NO forces classic TIFF.
   -  IF_NEEDED will only create a BigTIFF if it is clearly needed (in
      the uncompressed case, and image larger than 4GB. So no effect
      when using a compression).
   -  IF_SAFER will create BigTIFF if the resulting file \*might\*
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

-  **PIXELTYPE=[DEFAULT/SIGNEDBYTE]**: By setting this to SIGNEDBYTE, a
   new Byte file can be forced to be written as signed byte.

-  **COPY_SRC_OVERVIEWS=[YES/NO]**: (CreateCopy() only)
   By setting this to YES (default is NO), the potential existing
   overviews of the source dataset will be copied to the target dataset
   without being recomputed. This option is typically used to generate
   Cloud Optimized Geotiff (starting with GDAL 3.1, the :ref:`raster.cog` driver
   can be used as a convenient shortcut). If overviews of mask band also exist,
   provided that the GDAL_TIFF_INTERNAL_MASK configuration option is set
   to YES, they will also be copied. Note that this creation option will
   have `no effect <http://trac.osgeo.org/gdal/ticket/3917>`__ if
   general options (i.e. options which are not creation options) of
   gdal_translate are used. Creation options related to compression are
   also applied to the output overviews.

-  **GEOTIFF_KEYS_FLAVOR=[STANDARD/ESRI_PE]**: (GDAL >= 2.1.0) Determine
   which "flavor" of GeoTIFF keys must be used to write the SRS
   information. The STANDARD way (default choice) will use the general
   accepted formulations of GeoTIFF keys, including extensions of the
   values accepted for ProjectedCSTypeGeoKey to new EPSG codes. The
   ESRI_PE flavor will write formulations that are (more) compatible of
   ArcGIS. At the time of writing, the ESRI_PE choice has mostly an
   effect when writing the EPSG:3857 (Web Mercator) SRS. For other SRS,
   the standard way will be used, with the addition of a ESRI_PE WKT
   string as the value of PCSCitationGeoKey.

-  **GEOTIFF_VERSION=[AUTO/1.0/1.1]**: (GDAL >= 3.1.0) Select the version of
   the GeoTIFF standard used to encode georeferencing information. ``1.0``
   corresponds to the original
   `1995, GeoTIFF Revision 1.0, by Ritter & Ruth <http://geotiff.maptools.org/spec/geotiffhome.html>`_.
   ``1.1`` corresponds to the OGC standard 19-008, which is an evolution of 1.0,
   which clear ambiguities and fix inconsistencies mostly in the processing of
   the vertical part of a CRS.
   ``AUTO`` mode (default value) will generally select 1.0, unless the CRS to
   encode has a vertical component or is a 3D CRS, in which case 1.1 is used.

   .. note:: Write support for GeoTIFF 1.1 requires libgeotiff 1.6.0 or later.

Subdatasets
~~~~~~~~~~~

Multi-page TIFF files are exposed as subdatasets. On opening, a
subdataset name is GTIFF_DIR:{index}:filename.tif, where {index} starts
at 1.

Starting with GDAL 3.0, subdataset creation is possible by using the
APPEND_SUBDATASET=YES creation option. The filename passed to Create() /
CreateCopy() should be the regular filename (not with GTIFF_DIR: syntax.
Creating overviews on a multi-page TIFF is not supported.

Starting with GDAL 3.2, read-only access to subdataset overviews and masks
is possible provided that they are referenced by their parent IFD through
the `TIFFTAG_SUBIFD <https://www.awaresystems.be/imaging/tiff/tifftags/subifds.html>`__ tag.

About JPEG compression of RGB images
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When translating a RGB image to JPEG-In-TIFF, using PHOTOMETRIC=YCBCR
can make the size of the image typically 2 to 3 times smaller than the
default photometric value (RGB). When using PHOTOMETRIC=YCBCR, the
INTERLEAVE option must be kept to its default value (PIXEL), otherwise
libtiff will fail to compress the data.

Note also that the dimensions of the tiles or strips must be a multiple
of 8 for PHOTOMETRIC=RGB or 16 for PHOTOMETRIC=YCBCR

Lossless conversion of JPEG into JPEG-in-TIFF
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The conversion of a JPEG file (but *not* a JPEG-in-TIFF file) to a JPEG-in-TIFF
file without decompression and compression cycles, and thus without any additional
quality loss, can be done with gdal_translate (or the CreateCopy() API),
if all the following conditions are met:

- the source dataset is a JPEG file (or a VRT with a JPEG as a single SimpleSource)
- the target dataset is a JPEG-in-TIFF file
- no explicity target JPEG quality is specified
- no change in colorspace is specified
- no sub-windowing is requested
- and more generally, no change that alters pixel values

The generation of a tiled JPEG-in-TIFF from the original JPEG image is possible.
Explicit assigment of target SRS and bounds are also possible.

So, the following commands will use the lossless copy method :

::

    gdal_translate in.jpg out.tif -co COMPRESS=JPEG

    gdal_translate in.jpg out.tif -co COMPRESS=JPEG -co TILED=YES

    gdal_translate in.jpg out.tif -co COMPRESS=JPEG -a_srs EPSG:4326 -a_ullr -180 90 180 -90


whereas the following commands will *not* (and thus cause JPEG decompression and
compression):

::

    gdal_translate in.jpg out.tif -co COMPRESS=JPEG -co JPEG_QUALITY=60

    gdal_translate in.jpg out.tif -srcwin 0 0 500 500 -co COMPRESS=JPEG


Streaming operations
~~~~~~~~~~~~~~~~~~~~

The GeoTIFF driver can support reading or
writing TIFF files (with some restrictions detailed below) in a
streaming compatible way.

When reading a file from /vsistdin/, a named pipe (on Unix), or if
forcing streamed reading by setting the TIFF_READ_STREAMING
configuration option to YES, the GeoTIFF driver will assume that the
TIFF Image File Directory (IFD) is at the beginning of the file, i.e. at
offset 8 for a classical TIFF file or at offset 16 for a BigTIFF file.
The values of the tags of array type must be contained at the beginning
of file, after the end of the IFD and before the first image strip/tile.
The reader must read the strips/tiles in the order they are written in
the file. For a pixel interleaved file (PlanarConfiguration=Contig), the
recommended order for a writer, and thus for a reader, is from top to
bottom for a strip-organized file or from top to bottom, which a chunk
of a block height, and left to right for a tile-organized file. For a
band organized file (PlanarConfiguration=Separate), the above order is
recommended with the content of the first band, then the content of the
second band, etc... Technically this order corresponds to increasing
offsets in the TileOffsets/StripOffsets tag. This is the order that the
GDAL raster copy routine will assume.

If the order is not the one described above, the UNORDERED_BLOCKS=YES
dataset metadata item will be set in the TIFF metadata domain. Each
block offset can be determined by querying the
"BLOCK_OFFSET_[xblock]_[yblock]" band metadata items in the TIFF
metadata domain (where xblock, yblock is the coordinate of the block),
and a reader could use that information to determine the appropriate
reading order for image blocks.

The files that are streamed into the GeoTIFF driver may be compressed,
even if the GeoTIFF driver cannot produce such files in streamable
output mode (regular creation of TIFF files will produce such compatible
files for streamed reading).

When writing a file to /vsistdout/, a named pipe (on Unix), or when
definiting the STREAMABLE_OUTPUT=YES creation option, the CreateCopy()
method of the GeoTIFF driver will generate a file with the above defined
constraints (related to position of IFD and block order), and this is
only supported for a uncompressed file. The Create() method also
supports creating streamable compatible files, but the writer must be
careful to set the projection, geotransform or metadata before writing
image blocks (so that the IFD is written at the beginning of the file).
And when writing image blocks, the order of blocks must be the one of
the above paragraph, otherwise errors will be reported.

Some examples :

::

   gdal_translate in.tif /vsistdout/ -co TILED=YES | gzip | gunzip | gdal_translate /vsistdin/ out.tif -co TILED=YES -co COMPRESS=DEFLATE

or

::

   mkfifo my_fifo
   gdalwarp in.tif my_fifo -t_srs EPSG:3857
   gdal_translate my_fifo out.png -of PNG

Note: not all utilities are compatible with such input or output
streaming operations, and even those which may deal with such files may
not manage to deal with them in all circumstances, for example if the
reading driver driven by the output file is not compatible with the
block order of the streamed input.

Configuration options
~~~~~~~~~~~~~~~~~~~~~

This paragraph lists the configuration options that can be set to alter
the default behavior of the GTiff driver.

-  :decl_configoption:`GTIFF_IGNORE_READ_ERRORS` : Can be set to TRUE to
   avoid turning libtiff errors into GDAL errors. Can help reading
   partially corrupted TIFF files
-  :decl_configoption:`ESRI_XML_PAM` : Can be set to TRUE to force metadata in the xml:ESRI
   domain to be written to PAM.
-  :decl_configoption:`COMPRESS_OVERVIEW` :  See `Creation Options COMPRESS <#creation-options>`__ section.
   Set the compression type to use for overviews
-  :decl_configoption:`PHOTOMETRIC_OVERVIEW` :  YCBCR
   Set the photometric color space for overview creation
-  :decl_configoption:`PREDICTOR_OVERVIEW` : Integer 1,2 or 3.
   Set the predictor to use for overviews with LZW, DEFLATE and ZSTD compression
-  :decl_configoption:`JPEG_QUALITY_OVERVIEW` : Integer between 0 and 100. Default value : 75.
   Quality of JPEG compressed overviews, either internal or external.
-  :decl_configoption:`WEBP_LEVEL_OVERVIEW` : Integer between 1 and 100. Default value : 75.
   WEBP quality level of overviews, either internal or external.
-  :decl_configoption:`GDAL_TIFF_INTERNAL_MASK` : See `Internal nodata
   masks <#internal_mask>`__ section. Default value : FALSE.
-  :decl_configoption:`GDAL_TIFF_INTERNAL_MASK_TO_8BIT` : See `Internal nodata
   masks <#internal_mask>`__ section. Default value : TRUE
-  :decl_configoption:`USE_RRD` : Can be set to TRUE to force external overviews in the RRD
   format. Default value : FALSE
-  :decl_configoption:`TIFF_USE_OVR` : Can be set to TRUE to force external overviews in the
   GeoTIFF (.ovr) format. Default value : FALSE
-  :decl_configoption:`GTIFF_POINT_GEO_IGNORE` : Can be set to TRUE to revert back to the
   behavior of ancient GDAL versions regarding how PixelIsPoint is interpreted
   w.r.t geotransform. See :ref:`rfc-33` for more details. Default value : FALSE
-  :decl_configoption:`GTIFF_REPORT_COMPD_CS` : Can be set to TRUE to avoid
   stripping the vertical CRS of compound CRS when reading the SRS of a
   file. Does not affect the writing side. Default value : FALSE for GeoTIFF 1.0
   files, or TRUE (starting with GDAL 3.1) for GeoTIFF 1.1 files.
-  :decl_configoption:`GDAL_ENABLE_TIFF_SPLIT` : Can be set to FALSE to avoid
   all-in-one-strip files being presented as having. Default value :
   TRUE
-  :decl_configoption:`GDAL_TIFF_OVR_BLOCKSIZE` : See `Overviews <#overviews>`__ section.
-  :decl_configoption:`GTIFF_LINEAR_UNITS` : Can be set to BROKEN to read GeoTIFF files that
   have false easting/northing improperly set in meters when it ought to
   be in coordinate system linear units. (`Ticket
   #3901 <http://trac.osgeo.org/gdal/ticket/3901>`__).
-  :decl_configoption:`TAB_APPROX_GEOTRANSFORM` =YES/NO: To decide if an
   approximate geotransform is acceptable when reading a .tab file.
   Default value: NO
-  :decl_configoption:`GTIFF_DIRECT_IO` =YES/NO: Can be set to YES to use
   specialized RasterIO() implementations when reading un-compressed
   TIFF files (un-tiled only in GDAL 2.0, both un-tiled and tiled in
   GDAL 2.1) to avoid using the block cache. Setting it to YES even when
   the optimized cases do not apply should be safe (generic
   implementation will be used). Default value:NO
-  :decl_configoption:`GTIFF_VIRTUAL_MEM_IO` =YES/NO/IF_ENOUGH_RAM: Can be set
   to YES to use specialized RasterIO() implementations when reading
   un-compressed TIFF files to avoid using the block cache. This
   implementation relies on memory-mapped file I/O, and is currently
   only supported on Linux (64-bit build strongly recommended) or,
   starting with GDAL 2.1, on other POSIX-like systems. Setting it to
   YES even when the optimized cases do not apply should be safe
   (generic implementation will be used), but if the file exceeds RAM,
   disk swapping might occur if the whole file is read. Setting it to
   IF_ENOUGH_RAM will first check if the uncompressed file size is no
   bigger than the physical memory. Default value:NO. If both
   GTIFF_VIRTUAL_MEM_IO and GTIFF_DIRECT_IO are enabled, the former is
   used in priority, and if not possible, the later is tried.
-  :decl_configoption:`GDAL_GEOREF_SOURCES` =comma-separated list with one or several of PAM,
   INTERNAL, TABFILE or WORLDFILE. (GDAL >= 2.2). See
   `Georeferencing <#georeferencing>`__ paragraph.
-  :decl_configoption:`GDAL_NUM_THREADS` =number_of_threads/ALL_CPUS: (GDAL >= 2.1) Enable
   multi-threaded compression by specifying the number of worker
   threads. Worth it for slow compression algorithms such as DEFLATE or
   LZMA. Will be ignored for JPEG. Default is compression in the main
   thread. Note: this configuration option also apply to other parts to
   GDAL (warping, gridding, ...).
-  :decl_configoption:`GTIFF_WRITE_TOWGS84` =AUTO/YES/NO: (GDAL >= 3.0.3). When set to AUTO, a
   GeogTOWGS84GeoKey geokey will be written with TOWGS84 3 or 7-parameter
   Helmert transformation, if the CRS has no EPSG code attached to it, or if
   the TOWGS84 transformation attached to the CRS doesn't match the one imported
   from the EPSG code.
   If set to YES, then the TOWGS84 transformation attached to the CRS will be
   always written. If set to NO, then the transformation will not be written in
   any situation.

See Also
--------

-  `GeoTIFF Information Page <https://trac.osgeo.org/geotiff>`__
-  `libtiff Page <http://www.simplesystems.org/libtiff/>`__
-  `Details on BigTIFF file
   format <http://www.awaresystems.be/imaging/tiff/bigtiff.html>`__
- :ref:`raster.cog` driver

.. toctree::
   :maxdepth: 1
   :hidden:

   wld
