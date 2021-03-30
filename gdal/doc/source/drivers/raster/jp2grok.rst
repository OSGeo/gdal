.. _raster.jp2grok:

================================================================================
JP2Grok -- JPEG2000 driver based on Grok library
================================================================================

.. shortname:: JP2Grok

.. build_dependencies:: grok >= 8.0


This driver is an implementation of a JPEG2000 reader/writer based on
the Grok library.

The driver uses the VSI Virtual File API, so it can read JPEG2000
compressed NITF files.

XMP metadata can be extracted from JPEG2000 files, and will be stored as
XML raw content in the xml:XMP metadata domain.

The driver supports writing georeferencing information as GeoJP2 and
GMLJP2 boxes.

The driver supports creating files with
transparency, arbitrary band count, and adding/reading metadata. Update
of georeferencing or metadata of existing file is also supported.
Optional intellectual property metadata can be read/written in the
xml:IPR box.

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

The allowed sources and their priority order can
be changed with the GDAL_GEOREF_SOURCES configuration option (or
GEOREF_SOURCES open option) whose value is a comma-separated list of the
following keywords : PAM, GEOJP2, GMLJP2, INTERNAL (shortcut for
GEOJP2,GMLJP2), WORLDFILE, NONE. First mentioned sources are the most
prioritary over the next ones. A non mentioned source will be ignored.

For example setting it to "WORLDFILE,PAM,INTERNAL" will make a
geotransformation matrix from a potential worldfile prioritary over PAM
or internal JP2 boxes. Setting it to "PAM,WORLDFILE,GEOJP2" will use the
mentioned sources and ignore GMLJP2 boxes.

Thread support
--------------

By default, if the JPEG2000 file has internal tiling, GDAL will try to
decode several tiles in multiple threads if the RasterIO() request it
receives intersect several tiles. This behavior can be controlled with
the GDAL_NUM_THREADS configuration option that defaults to ALL_CPUS in
that context. In case RAM is limited, it can be needed to set this
configuration option to 1 to disable multi-threading

Option Options
--------------

The following open option is available:

-  **1BIT_ALPHA_PROMOTION=YES/NO**: Whether a 1-bit alpha channel should
   be promoted to 8-bit. Defaults to YES.

-  **GEOREF_SOURCES=string**: (GDAL > 2.2) Define which georeferencing
   sources are allowed and their priority order. See
   `Georeferencing <#georeferencing>`__ paragraph.

-  **USE_TILE_AS_BLOCK=YES/NO**: (GDAL > 2.2) Whether to always use the
   JPEG-2000 block size as the GDAL block size Defaults to NO. Setting
   this option can be useful when doing whole image decompression and
   the image is single-tiled. Note however that the tile size must not
   exceed 2 GB since that's the limit supported by GDAL.

Creation Options
----------------

-  **CODEC=JP2/J2K** : JP2 will add JP2 boxes around the codestream
   data. The value is determined automatically from the file extension.
   If it is neither JP2 nor J2K, J2K codec is used.

-  **GMLJP2=YES/NO**: Indicates whether a GML
   box conforming to the OGC GML in JPEG2000 specification should be
   included in the file. Unless GMLJP2V2_DEF is used, the version of the
   GMLJP2 box will be version 1. Defaults to YES.
-  **GMLJP2V2_DEF=filename**: Indicates whether
   a GML box conforming to the `OGC GML in JPEG2000, version
   2.0.1 <http://docs.opengeospatial.org/is/08-085r5/08-085r5.html>`__
   specification should be included in the file. *filename* must point
   to a file with a JSon content that defines how the GMLJP2 v2 box
   should be built. See :ref:`GMLJP2v2 definition file
   section <gmjp2v2def>` in documentation of
   the JP2OpenJPEG driver for the syntax of the JSon configuration file. 
   It is also possible to directly pass the JSon
   content inlined as a string. If filename is just set to YES, a
   minimal instance will be built. Note: GDAL 2.0 and 2.1 use the older
   `OGC GML in JPEG2000, version
   2.0 <http://docs.opengeospatial.org/is/08-085r4/08-085r4.html>`__
   specification, that differ essentially by the content of the
   gml:domainSet, gml:rangeSet and gmlcov:rangeType elements of
   gmljp2:GMLJP2CoverageCollection.
-  **GeoJP2=YES/NO**: Indicates whether a
   UUID/GeoTIFF box conforming to the GeoJP2 (GeoTIFF in JPEG2000)
   specification should be included in the file. Defaults to YES.
-  **QUALITY=float_value,float_value,...** : Percentage between 0 and
   100. A value of 50 means the file will be half-size in comparison to
   uncompressed data, 33 means 1/3, etc.. Defaults to 25 (unless the
   dataset is made of a single band with color table, in which case the
   default quality is 100). It is possible to
   specify several quality values (comma separated) to ask for several
   quality layers. Quality values should be increasing.

-  **REVERSIBLE=YES/NO** : YES means use of reversible 5x3 integer-only
   filter, NO use of the irreversible DWT 9-7. Defaults to NO (unless
   the dataset is made of a single band with color table, in which case
   reversible filter is used).

-  **RESOLUTIONS=int_value** : Number of resolution levels. Default
   value is selected such the smallest overview of a tile is no bigger
   than 128x128.

-  **BLOCKXSIZE=int_value** : Tile width. Defaults to 1024.

-  **BLOCKYSIZE=int_value** : Tile height. Defaults to 1024.

-  **PROGRESSION=LRCP/RLCP/RPCL/PCRL/CPRL** : Progession order. Defaults
   to LRCP.

-  **SOP=YES/NO** : YES means generate SOP (Start Of Packet) marker
   segments. Defaults to NO.

-  **EPH=YES/NO** : YES means generate EPH (End of Packet Header) marker
   segments. Defaults to NO.

-  **YCBCR420=YES/NO** : YES if RGB must be resampled to
   YCbCr 4:2:0. Defaults to NO.

-  **YCC=YES/NO** : YES if RGB must be transformed to YCC
   color space ("MCT transform", i.e. internal transform, without visual
   degration). Defaults to YES.

-  **NBITS=int_value** : Bits (precision) for sub-byte
   files (1-7), sub-uint16 (9-15), sub-uint32 (17-31).

-  **1BIT_ALPHA=YES/NO**: Whether to encode the alpha
   channel as a 1-bit channel (when there's an alpha channel). Defaults
   to NO, unless INSPIRE_TG=YES. Enabling this option might cause
   compatibility problems with some readers. At the time of writing,
   those based on the MrSID JPEG2000 SDK are unable to open such files.
   And regarding the ECW JPEG2000 SDK, decoding of 1-bit alpha channel
   with lossy/irreversible compression gives visual artifacts (OK with
   lossless encoding).

-  **ALPHA=YES/NO**: Whether to force encoding last
   channel as alpha channel. Only useful if the color interpretation of
   that channel is not already Alpha. Defaults to NO.

-  **PROFILE=AUTO/UNRESTRICTED/PROFILE_1**: Determine
   which codestream profile to use. UNRESTRICTED corresponds to the
   "Unrestricted JPEG 2000 Part 1 codestream" (RSIZ=0). PROFILE_1
   corresponds to the "JPEG 2000 Part 1 Profile 1 codestream" (RSIZ=2),
   which add constraints on tile dimensions and number of resolutions.
   In AUTO mode, the driver will determine if the BLOCKXSIZE,
   BLOCKYSIZE, RESOLUTIONS, CODEBLOCK_WIDTH and CODEBLOCK_HEIGHT values
   are compatible with PROFILE_1 and advertise it in the relevant case.
   Note that the default values of those options are compatible with
   PROFILE_1. Otherwise UNRESTRICTED is advertized. Defaults to AUTO.

-  **INSPIRE_TG=YES/NO**: Whether to use JPEG2000 features
   that comply with `Inspire Orthoimagery Technical
   Guidelines <http://inspire.ec.europa.eu/documents/Data_Specifications/INSPIRE_DataSpecification_OI_v3.0.pdf>`__.
   Defaults to NO. When set to YES, implies PROFILE=PROFILE_1,
   1BIT_ALPHA=YES, GEOBOXES_AFTER_JP2C=YES. The CODEC, BLOCKXSIZE,
   BLOCKYSIZE, RESOLUTIONS, NBITS, PROFILE, CODEBLOCK_WIDTH and
   CODEBLOCK_HEIGHT options will be checked against the requirements and
   recommendations of the Technical Guidelines.

-  **JPX=YES/NO**: Whether to advertise JPX features, and
   add a Reader requirement box, when a GMLJP2 box is written. Defaults
   to YES. This option should not be used unless compatibility problems
   with a reader occur.

-  **GEOBOXES_AFTER_JP2C=YES/NO**: Whether to place
   GeoJP2/GMLJP2 boxes after the code-stream. Defaults to NO, unless
   INSPIRE_TG=YES. This option should not be used unless compatibility
   problems with a reader occur.

-  **PRECINCTS={prec_w,prec_h},{prec_w,prec_h},...**: A
   list of {precincts width,precincts height} tuples to specify
   precincts size. Each value should be a multiple of 2. The maximum
   number of tuples used will be the number of resolutions. The first
   tuple corresponds to the higher resolution level, and the following
   ones to the lower resolution levels. If less tuples are specified,
   the last one is used by dividing its values by 2 for each extra lower
   resolution level. The default value used is
   {512,512},{256,512},{128,512},{64,512},{32,512},{16,512},{8,512},{4,512},{2,512}.
   An empty string may be used to disable precincts ( i.e. the default
   {32767,32767},{32767,32767}, ... will then be used).

-  **TILEPARTS=DISABLED/RESOLUTIONS/LAYERS/COMPONENTS**:
   Whether to generate tile-parts and according to which criterion.
   Defaults to DISABLED.

-  **CODEBLOCK_WIDTH=int_value**: Codeblock width: power
   of two value between 4 and 1024. Defaults to 64. Note that
   CODEBLOCK_WIDTH \* CODEBLOCK_HEIGHT must not be greater than 4096.
   For PROFILE_1 compatibility, CODEBLOCK_WIDTH must not be greater than
   64.

-  **CODEBLOCK_HEIGHT=int_value**: Codeblock height: power
   of two value between 4 and 1024. Defaults to 64. Note that
   CODEBLOCK_WIDTH \* CODEBLOCK_HEIGHT must not be greater than 4096.
   For PROFILE_1 compatibility, CODEBLOCK_HEIGHT must not be greater
   than 64.

-  **CODEBLOCK_STYLE=string**: (GDAL >= 2.4) Style
   of the code-block coding passes. The following 6 independent settings
   can be combined together (values should be comma separated):

   -  *BYPASS* (1): enable selective arithmetic coding bypass (can
      substantially improve coding/decoding speed, at the expense of
      larger file size)
   -  *RESET* (2): reset context probabilities on coding pass boundaries
   -  *TERMALL* (4): enable termination on each coding pass
   -  *VSC* (8): enable vertically causal context
   -  *PREDICTABLE* (16): enable predictable termination (helps for
      error detection)
   -  *SEGSYM* (32): enable segmentation symbols (helps for error
      detection)

   Instead of specifying them by text, it is also possible to give the
   corresponding numeric value of the global codeblock style, by adding
   the selected options (for example "BYPASS,TERMALL" is equivalent to
   "5"=1+4)

   By default, none of them are enabled. Enabling them will generally
   increase codestream size, but improve either coding/decoding speed or
   resilience/error detection.

-  **PLT=YES/NO**: (GDAL >= 3.1.1) Whether to write a
   PLT (Packet Length) marker segments in tile-part headers. Defaults to NO.

-  **WRITE_METADATA=YES/NO**: Whether metadata should be
   written, in a dedicated JP2 'xml ' box. Defaults to NO. The content
   of the 'xml ' box will be like:

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

   If there is a metadata domain whose name is "xml:IPR", its content
   will be written as a JP2 'jp2i' box.

-  **MAIN_MD_DOMAIN_ONLY=YES/NO**: (Only if
   WRITE_METADATA=YES) Whether only metadata from the main domain should
   be written. Defaults to NO.

-  **USE_SRC_CODESTREAM=YES/NO**: (EXPERIMENTAL!) When
   source dataset is JPEG2000, whether to reuse the codestream of the
   source dataset unmodified. Defaults to NO. Note that enabling that
   feature might result in inconsistent content of the JP2 boxes w.r.t.
   to the content of the source codestream. Most other creation options
   will be ignored in that mode. Can be useful in some use cases when
   adding/correcting georeferencing, metadata, ... INSPIRE_TG and
   PROFILE options will be ignored, and the profile of the codestream
   will be overridden with the one specified/implied by the options
   (which may be inconsistent with the characteristics of the
   codestream).

Lossless compression
~~~~~~~~~~~~~~~~~~~~

Lossless compression can be achieved if ALL the following creation
options are defined :

-  QUALITY=100
-  REVERSIBLE=YES
-  YCBCR420=NO (which is the default)


See Also
---------

-  Implemented as ``gdal/frmts/grok/grokdataset.cpp``.

-  `Official JPEG-2000 page <http://www.jpeg.org/jpeg2000/index.html>`__

-  `The Grok library home
   page <https://github.com/GrokImageCompression/grok>`__

-  `OGC GML in JPEG2000, version
   2.0 <http://docs.opengeospatial.org/is/08-085r4/08-085r4.html>`__
   (GDAL 2.0 and 2.1)

-  `OGC GML in JPEG2000, version
   2.0.1 <http://docs.opengeospatial.org/is/08-085r5/08-085r5.html>`__
   (GDAL 2.2 and above)

-  `Inspire Data Specification on Orthoimagery - Technical
   Guidelines <http://inspire.ec.europa.eu/documents/Data_Specifications/INSPIRE_DataSpecification_OI_v3.0.pdf>`__

Other JPEG2000 GDAL drivers :

-  :ref:`JP2OpenJpeg: based on OpenJPEG library (open
   source) <raster.jp2openjpeg>`

-  :ref:`JPEG2000: based on Jasper library (open
   source) <raster.jpeg2000>`

-  :ref:`JP2ECW: based on Erdas ECW library
   (proprietary) <raster.jp2ecw>`

-  :ref:`JP2MRSID: based on LizardTech MrSID library
   (proprietary) <raster.jp2mrsid>`

-  :ref:`JP2KAK: based on Kakadu library (proprietary) <raster.jp2kak>`
