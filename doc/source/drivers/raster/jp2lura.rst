.. _raster.jp2lura:

================================================================================
JP2Lura -- JPEG2000 driver based on Lurawave library
================================================================================

.. shortname:: JP2LURA

.. versionadded:: 2.2

.. build_dependencies:: Lurawave library

This driver is an implementation of a JPEG2000 reader/writer based on
Lurawave library.

The driver uses the VSI Virtual File API, so it can read JPEG2000
compressed NITF files.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Georeferencing
--------------

Georeferencing information can come from different sources : internal
(GeoJP2 or GMLJP2 boxes), worldfile .j2w/.wld sidecar files, or PAM
(Persistent Auxiliary metadata) .aux.xml sidecar files. By default,
information is fetched in following order (first listed is the most
prioritary): PAM, GeoJP2, GMLJP2, WORLDFILE.

The allowed sources and their priority order can be changed with the
GDAL_GEOREF_SOURCES configuration option (or GEOREF_SOURCES open option)
whose value is a comma-separated list of the following keywords : PAM,
GEOJP2, GMLJP2, INTERNAL (shortcut for GEOJP2,GMLJP2), WORLDFILE, NONE.
First mentioned sources are the most prioritary over the next ones. A
non mentioned source will be ignored.

For example setting it to "WORLDFILE,PAM,INTERNAL" will make a
geotransformation matrix from a potential worldfile prioritary over PAM
or internal JP2 boxes. Setting it to "PAM,WORLDFILE,GEOJP2" will use the
mentioned sources and ignore GMLJP2 boxes.

License number
--------------

The LURA_LICENSE_NUM_1 and LURA_LICENSE_NUM_2 configuration options /
environment variables must be set with the 2 numbers that compose a
license number.

Option Options
--------------

The following open option is available:

-  **GEOREF_SOURCES=string**: Define which georeferencing sources are
   allowed and their priority order. See
   `Georeferencing <#georeferencing>`__ paragraph.

Creation Options
----------------

-  **CODEC=JP2/Codestream** : JP2 will add JP2 boxes around the
   codestream data. The value is determined automatically from the file
   extension. If it is neither JP2 nor Codestream, JP2 codec is used.

-  **GMLJP2=YES/NO**: Indicates whether a GML box conforming to the OGC
   GML in JPEG2000 specification should be included in the file. Unless
   GMLJP2V2_DEF is used, the version of the GMLJP2 box will be version
   1. Defaults to YES.
-  **GMLJP2V2_DEF=filename**: Indicates whether a GML box conforming to
   the `OGC GML in JPEG2000, version
   2.0.1 <http://docs.opengeospatial.org/is/08-085r5/08-085r5.html>`__
   specification should be included in the file. *filename* must point
   to a file with a JSon content that defines how the GMLJP2 v2 box
   should be built. See :ref:`GMLJP2v2 definition file
   section <gmjp2v2def>` in documentation of
   the JP2OpenJPEG driver for the syntax of the JSon configuration file.
   It is also possible to
   directly pass the JSon content inlined as a string. If filename is
   just set to YES, a minimal instance will be built.
-  **GeoJP2=YES/NO**: Indicates whether a UUID/GeoTIFF box conforming to
   the GeoJP2 (GeoTIFF in JPEG2000) specification should be included in
   the file. Defaults to NO.
-  **SPLIT_IEEE754=YES/NO**: Whether encoding of Float32 bands as 3
   bands with values decomposed according to IEEE-754 structure: first
   band (1 bit, signed) with sign bit, second band (8 bits, unsigned)
   with exponent value and third band (23 bits, unsigned) with mantissa
   value. Default to NO. This is a non-standard extension to encode
   floating point values. By default, the sign bit and exponent will be
   encoded with the reversible wavelet (even with REVERSIBLE=NO), and
   the mantissa with the irreversible one. If specifying REVERSIBLE=YES,
   all 3 components will be encoded with the reversible wavelet.
-  **NBITS=int_value** : Bits (precision) for sub-byte files (1-7),
   sub-uint16 (9-15), sub-uint32 (17-28).
-  **QUALITY_STYLE=PSNR/XXSmall/XSmall/Small/Medium/Large/XLarge/XXLarge**:
   This property tag is used to set the quality mode to be used during
   lossy compression. For normal images and situations (1:1 pixel
   display, ~50 cm viewing distance) we recommend Small or PSNR. For
   quality measurement only PSNR should be used. Default is PSNR.
-  **SPEED_MODE=Fast/Accurate**: This property tag is used to set the
   speed mode to be used during lossy compression. The following modes
   are defined. Default is Fast
-  **RATE=int_value.** When specifying this value, the target compressed
   file size will be the uncompressed file size divided by RATE. In
   general the achieved rate will be exactly the requested size or a few
   bytes lower. Will force use of irreversible wavelet. Default value: 0
   (maximum quality).
-  **QUALITY=1 to 100** Compression to a particular quality is possible
   only when using the 9-7 filter with the standard expounded
   quantization and no regions of interest. A compression quality may be
   specified between 1 (low) and 100 (high). The size of the resulting
   JPEG2000 file will depend of the image content. Only used for
   irreversible compression. The compression quality cannot be used
   together the property RATE. Default value: 0 (maximum quality). When
   using this option together with SPLIT_IEEE754=YES, the sign bit and
   exponent bands will have to be switched to irreversible encoding,
   which can lead to huge loss in the reconstructed floating-point
   value.
-  **PRECISION=int_value** For improved efficiency, the library
   automatically, depending on the image depth, uses either 16 or 32 bit
   representation for wavelet coefficients. The precision property can
   be set to force the library to always use 32 bit representations. The
   use of 32 bit values may slightly improve image quality and the
   expense of speed and memory requirements. Default value: 0
   (automatically select appropriate precision).
-  **REVERSIBLE=YES/NO** : YES means use of reversible 5x3 integer-only
   filter, NO use of the irreversible DWT 9-7. Defaults to NO.

-  **LEVELS=int_value** (0-16) : The number of wavelet transformation
   levels can be set using this property. Valid values are in the range
   0 (no wavelet analysis) to 16 (very fine analysis). The memory
   requirements and compression time increases with the number of
   transformation levels. A reasonable number of transformation levels
   is in the 4-6 range. Default is 5.

-  **QUANTIZATION_STYLE=DERIVED/EXPOUNDED** : This property may only be
   set when the irreversible filter (9_7) is used. The quantization
   steps can either be derived from a bases quantization step, DERIVED,
   or calculated for each image sub-band, EXPOUNDED. The EXPOUNDED style
   is recommended when using the irreversible filter. Default is
   EXPOUNDED.

-  **TILEXSIZE=int_value** : Tile width. An image can be split into
   smaller tiles, with each tile independently compressed. The basic
   tile size and the offset to the first tile on the virtual compression
   reference grid can be set using these properties. The first tile must
   contain the first image pixel. The tiling of an image is recommended
   only for very large images. Default values: (0) One Tile containing
   the complete image. If the image dimension exceeds 15000x15000, it
   will be tiled with tiles of dimension 1024x1024.

-  **TILEYSIZE=int_value** : Tile height. An image can be split into
   smaller tiles, with each tile independently compressed. The basic
   tile size and the offset to the first tile on the virtual compression
   reference grid can be set using these properties. The first tile must
   contain the first image pixel. The tiling of an image is recommended
   only for very large images. Default values: (0) One Tile containing
   the complete image. If the image dimension exceeds 15000x15000, it
   will be tiled with tiles of dimension 1024x1024.

-  **TLM=YES/NO**: (TiLe Marker) The efficiency of decoding regions in a
   tiled image may be improved by " the usage of a tile length marker.
   Tile length markers contain the " position of each tile in a JPEG2000
   codestream, enabling faster access " to tiled data. Default is NO.

-  **PROGRESSION=LRCP/RLCP/RPCL/PCRL/CPRL** : The organization of the
   coded data in the file can be set by this property tag. The following
   progression orders are defined: LRCP = Quality progressive, LCP =
   Resolution then quality progressive, RPCL = Resolution then position
   progressive, PCRL = Position progressive, CPRL = Color/channel
   progressive. The setting LRCP (quality) is most useful when used with
   several layers. The PCRL (position) should be used with precincts.
   Defaults to LRCP.

-  **JPX=YES/NO**: Whether to advertise JPX features, and add a Reader
   requirement box, when a GMLJP2 box is written (for GMLJP2 v2, the
   branding will also be "jpx "). Defaults to YES. This option should
   not be used unless compatibility problems with a reader occur.

-  **CODEBLOCK_WIDTH=int_value**: Codeblock width: power of two value
   between 4 and 1024. Defaults to 64. Note that CODEBLOCK_WIDTH \*
   CODEBLOCK_HEIGHT must not be greater than 4096. For PROFILE_1
   compatibility, CODEBLOCK_WIDTH must not be greater than 64.

-  **CODEBLOCK_HEIGHT=int_value**: Codeblock height: power of two value
   between 4 and 1024. Defaults to 64. Note that CODEBLOCK_WIDTH \*
   CODEBLOCK_HEIGHT must not be greater than 4096. For PROFILE_1
   compatibility, CODEBLOCK_HEIGHT must not be greater than 64.

-  **ERROR_RESILIENCE=YES/NO**: This option improves error resilient in
   JPEG2000 streams or for special codecs (e.g. hardware coder) for a
   faster compression/ decompression. This option will increase the file
   size slightly when generating a code stream with the same image
   quality. Default is NO.

-  **WRITE_METADATA=YES/NO**: Whether metadata should be written, in a
   dedicated JP2 'xml ' box. Defaults to NO. The content of the 'xml '
   box will be like:

   ::

      <GDALMultiDomainMetadata>
        <Metadata>
          <MDI key="foo">bar</MDI>
        </Metadata>
        <Metadata domain='aux_domain'>
          <MDI key="foo">bar</MDI>
        </Metadata>
        <Metadata domain='a_xml_domain' format='xml'>
          <arbitrary_xml_content>
          </arbitrary_xml_content>
        </Metadata>
      </GDALMultiDomainMetadata>

   If there are metadata domain whose name starts with "xml:BOX\_", they
   will be written each as separate JP2 'xml ' box.

   If there is a metadata domain whose name is "xml:XMP", its content
   will be written as a JP2 'uuid' XMP box.

-  **MAIN_MD_DOMAIN_ONLY=YES/NO**: (Only if WRITE_METADATA=YES) Whether
   only metadata from the main domain should be written. Defaults to NO.

-  **USE_SRC_CODESTREAM=YES/NO**: (EXPERIMENTAL!) When source dataset is
   JPEG2000, whether to reuse the codestream of the source dataset
   unmodified. Defaults to NO. Note that enabling that feature might
   result in inconsistent content of the JP2 boxes w.r.t. to the content
   of the source codestream. Most other creation options will be ignored
   in that mode. Can be useful in some use cases when adding/correcting
   georeferencing, metadata, ...

Lossless compression
~~~~~~~~~~~~~~~~~~~~

Lossless compression can be achieved if REVERSIBLE=YES is used (and RATE
is not specified).

Vector information
------------------

A JPEG2000 file containing a GMLJP2 v2 box with GML feature collections
and/or KML annotations embedded can be opened as a vector file with the
OGR API. For example:

::

   ogrinfo -ro my.jp2

   INFO: Open of my.jp2'
         using driver `JP2Lura' successful.
   1: FC_GridCoverage_1_rivers (LineString)
   2: FC_GridCoverage_1_borders (LineString)
   3: Annotation_1_poly

Feature collections can be linked from the GMLJP2 v2 box to a remote
location. By default, the link is not followed. It will be followed if
the open option OPEN_REMOTE_GML is set to YES.

Bugs
----

Proper support of JPEG-2000 images with
Int32/UInt32/Float32-IEEE754-split on Linux 64 bits require a v2.1.00.17
or later SDK.

See Also
--------

-  `LuraTech JPEG-2000
   SDK <https://www.luratech.com/en/solutions/applications/data-compression-imaging-with-jpeg-2000/>`__

Other JPEG2000 GDAL drivers :

-  :ref:`JP2OpenJPEG: based on Openjpeg library (open
   source) <raster.jp2openjpeg>`

-  :ref:`JPEG2000: based on Jasper library (open
   source) <raster.jpeg2000>`

-  :ref:`JP2ECW: based on Erdas ECW library
   (proprietary) <raster.jp2ecw>`

-  :ref:`JP2MRSID: based on LizardTech MrSID library
   (proprietary) <raster.jp2mrsid>`

-  :ref:`JP2KAK: based on Kakadu library (proprietary) <raster.jp2kak>`
