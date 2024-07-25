.. _raster.jp2ecw:

================================================================================
JP2ECW -- ERDAS JPEG2000 (.jp2)
================================================================================

.. shortname:: JP2ECW

.. build_dependencies:: ECW SDK

GDAL supports reading and writing JPEG2000 files using the ERDAS ECW/JP2
SDK developed by Hexagon Geospatial (formerly Intergraph, ERDAS,
ERMapper). Support is optional and requires linking in the libraries
available from the ECW/JP2 SDK Download page.

Coordinate system and georeferencing transformations are read, and some
degree of support is included for GeoJP2 (tm) (GeoTIFF-in-JPEG2000),
ERDAS GML-in-JPEG2000, and the new GML-in-JPEG2000 specification
developed at OGC.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Licensing
---------

The ERDAS ECW/JP2 SDK v5.x is available under multiple license types.
For Desktop usage, decoding any sized ECW/JP2 image is made available
free of charge. To compress, deploy on a Server platform, or decode
unlimited sized files on Mobile platforms a license must be purchased
from Hexagon Geospatial.

History
-------

-  v3.x - Last release, 2006
-  v4.x - Last release, 2012
-  v5.x - Active development, 2013 - current

Georeferencing
--------------

Georeferencing information can come from different sources : internal
(GeoJP2 or GMLJP2 boxes), worldfile .j2w/.wld sidecar files, or PAM
(Persistent Auxiliary metadata) .aux.xml sidecar files. By default,
information is fetched in following order (first listed is the highest
priority): PAM, GeoJP2, GMLJP2, WORLDFILE.

Starting with GDAL 2.2, the allowed sources and their priority order can
be changed with the :config:`GDAL_GEOREF_SOURCES` configuration option (or
:oo:`GEOREF_SOURCES` open option) whose value is a comma-separated list of the
following keywords : PAM, GEOJP2, GMLJP2, INTERNAL (shortcut for
GEOJP2,GMLJP2), WORLDFILE, NONE. Earlier mentioned sources take
priority over later ones. A non mentioned source will be ignored.

For example setting it to "WORLDFILE,PAM,INTERNAL" will make a
geotransformation matrix from a potential worldfile priority over PAM
or internal JP2 boxes. Setting it to "PAM,WORLDFILE,GEOJP2" will use the
mentioned sources and ignore GMLJP2 boxes.

Open Options
------------

|about-open-options|
The following open options are available:

-  .. oo:: 1BIT_ALPHA_PROMOTION
      :choices: YES, NO
      :default: YES

      Whether a 1-bit alpha channel should be promoted to 8-bit.

-  .. oo:: GEOREF_SOURCES
      :since: 2.2

      Define which georeferencing
      sources are allowed and their priority order. See
      `Georeferencing`_ paragraph.

Creation Options:
-----------------

|about-creation-options|
Supported creation options are listed below.

Note: Only Licensing and compression target need to be specified. The
ECW/JP2 SDK will default all other options to recommended settings based
on the input characteristics. Changing other options can *substantially*
impact decoding speed and compatibility with other JPEG2000 toolkits.

-  .. co:: LARGE_OK
      :choices: YES

      (v3.x SDK only) Allow compressing files larger
      than 500MB in accordance with EULA terms. Deprecated since v4.x and
      replaced by :co:`ECW_ENCODE_KEY` & :co:`ECW_ENCODE_COMPANY`.

-  .. co:: ECW_ENCODE_KEY
      :choices: <key>

      (v4.x SDK or higher) Provide the OEM
      encoding key to unlock encoding capability up to the licensed
      gigapixel limit. The key is approximately 129 hex digits long. The
      Company and Key must match and must be re-generated with each minor
      release of the SDK. It may also be provided globally as a
      configuration option.

-  .. co:: ECW_ENCODE_COMPANY
      :choices: <name>

      *(v4.x SDK or higher)* Provide the name
      of the company in the issued OEM key (see ECW_ENCODE_KEY). The
      Company and Key must match and must be re-generated with each minor
      release of the SDK. It may also be provided globally as a
      configuration option.

-  .. co:: TARGET
      :choices: <percent>
      :default: 75

      Set the target size reduction as a percentage of
      the original. If not provided defaults to 75 for an 75% reduction.
      TARGET=0 uses lossless compression.

-  .. co:: PROJ

      Name of the ECW projection string to use. Common
      examples are NUTM11, or GEODETIC.

-  .. co:: DATUM

      Name of the ECW datum string to use. Common examples
      are WGS84 or NAD83.

-  .. co:: GMLJP2
      :choices: YES, NO
      :default: YES

      Indicates whether a GML box conforming to the OGC
      GML in JPEG2000 specification should be included in the file. Unless
      GMLJP2V2_DEF is used, the version of the GMLJP2 box will be version
      1.

-  .. co:: GMLJP2V2_DEF
      :choices: <filename>, <json>, YES

      Indicates whether
      a GML box conforming to the `OGC GML in JPEG2000, version
      2 <http://docs.opengeospatial.org/is/08-085r4/08-085r4.html>`__
      specification should be included in the file. *filename* must point
      to a file with a JSON content that defines how the GMLJP2 v2 box
      should be built. See :ref:`GMLJP2v2 definition file
      section <gmjp2v2def>` in documentation of
      the JP2OpenJPEG driver for the syntax of the JSON configuration file.
      It is also possible to directly pass the JSON content inlined as a
      string. If filename is just set to YES, a minimal instance will be
      built.

-  .. co:: GeoJP2
      :choices: YES, NO
      :default: YES

      Indicates whether a UUID/GeoTIFF box conforming to
      the GeoJP2 (GeoTIFF in JPEG2000) specification should be included in
      the file.

-  .. co:: PROFILE
      :choices: BASELINE_0, BASELINE_1, BASELINE_2, NPJE, EPJE

      Review the ECW SDK documentation for details on profile meanings.

-  .. co:: PROGRESSION
      :choices: LRCP, RLCP, RPCL
      :default: RPCL

      Set the progression order with which
      the JPEG2000 codestream is written.

-  .. co:: CODESTREAM_ONLY
      :choices: YES, NO
      :default: NO

      If set to YES, only the compressed
      imagery code stream will be written. If NO a JP2 package will be
      written around the code stream including a variety of meta
      information.

-  .. co:: LEVELS
      :choices: <integer>

      Resolution levels in pyramid (by default so many that
      the size of the smallest thumbnail image is 64x64 pixels at maximum)

-  .. co:: LAYERS
      :default: 1

      Quality layers (default, 1)

-  .. co:: PRECINCT_WIDTH
      :default: 64

      Precinct Width

-  .. co:: PRECINCT_HEIGHT
      :default: 64

      Precinct Height

-  .. co:: TILE_WIDTH

      Tile Width (default, image width eg. 1 tile). Apart
      from GeoTIFF, in JPEG2000 tiling is not critical for speed if
      precincts are used. The minimum tile size allowed by the standard is
      1024x1024 pixels.

-  .. co:: TILE_HEIGHT

      Tile Height (default, image height eg. 1 tile)

-  .. co:: INCLUDE_SOP
      :choices: YES, NO

      Output Start of Packet Marker (default false)

-  .. co:: INCLUDE_EPH
      :choices: YES, NO

      Output End of Packet Header Marker (default true)

-  .. co:: DECOMPRESS_LAYERS

      The number of quality layers to decode

-  .. co:: DECOMPRESS_RECONSTRUCTION_PARAMETER
      :choices: IRREVERSIBLE_9x7, REVERSIBLE_5x3

-  .. co:: WRITE_METADATA
      :choices: YES, NO

      Whether metadata should be
      written, in a dedicated JP2 XML box. Defaults to NO. The content of
      the XML box will be like:

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
      will be written each as separate JP2 XML box.

      If there is a metadata domain whose name is "xml:XMP", its content
      will be written as a JP2 UUID XMP box.

-  .. co:: MAIN_MD_DOMAIN_ONLY
      :choices: YES, NO
      :default: NO

      (Only if :co:`WRITE_METADATA=YES`)
      Whether only metadata from the main domain should
      be written.

"JPEG2000 format does not support creation of GDAL overviews since the
format is already considered to be optimized for "arbitrary overviews".
JP2ECW driver also arranges JP2 codestream to allow optimal access to
power of two overviews. This is controlled with the creation option
LEVELS."

Create support
--------------

While the driver advertises the Create() capability, contrary to most other
drivers that implement it, the implementation of RasterIO() and WriteBlock()
in the JP2ECW driver does not support arbitrary random writing.
Data must be written in the dataset from top to bottom, whole line(s) at a
time.

Configuration Options
---------------------

|about-config-options|
The ERDAS ECW/JP2 SDK supports a variety of runtime configuration options to
control various features. See the ECW/JP2 SDK documentation for full details on
the meaning of these options.

-  :copy-config:`ECW_CACHE_MAXMEM`

-  :copy-config:`ECW_TEXTURE_DITHER`

-  .. co:: ECW_FORCE_FILE_REOPEN
      :choices: TRUE, FALSE
      :default: FALSE

      This may be set to TRUE to
      force open a file handle for each file for each connection made.

-  .. co:: ECW_CACHE_MAXOPEN
      :default: unlimited

      The maximum number of files to keep
      open for ECW file handle caching.

-  .. co:: ECW_AUTOGEN_J2I
      :choices: TRUE, FALSE
      :default: TRUE

      Controls whether .j2i index files
      should be created when opening jpeg2000 files.

-  .. co:: ECW_RESILIENT_DECODING
      :choices: TRUE, FALSE
      :default: TRUE

      Controls whether the reader
      should be forgiving of errors in a file, trying to return as much
      data as is available. If set to FALSE an invalid
      file will result in an error.

Metadata
--------

XMP metadata can be extracted from JPEG2000
files, and will be stored as XML raw content in the xml:XMP metadata
domain.

ECW/JP2 SDK v5.1+ also advertises JPEG2000 structural information as
generic File Metadata reported under "JPEG2000" metadata domain (-mdd):

-  **ALL_COMMENTS**: Generic comment text field
-  **PROFILE**: Profile type (0,1,2). Refer to ECW/JP2 SDK documentation
   for more info
-  **TILES_X**: Number of tiles on X (horizontal) Axis
-  **TILES_Y**: Number of tiles on Y (vertical) Axis
-  **TILE_WIDTH**: Tile size on X Axis
-  **TILE_HEIGHT**: Tile size on Y Axis
-  **PRECINCT_SIZE_X**: Precinct size for each resolution level
   (smallest to largest) on X Axis
-  **PRECINCT_SIZE_Y**: Precinct size for each resolution level
   (smallest to largest) on Y Axis
-  **CODE_BLOCK_SIZE_X**: Code block size on X Axis
-  **CODE_BLOCK_SIZE_Y**: Code block size on Y Axis
-  **PRECISION**: Precision / Bit-depth of each component eg. 8,8,8 for
   8bit 3 band imagery.
-  **RESOLUTION_LEVELS**: Number of resolution levels
-  **QUALITY_LAYERS**: Number of quality layers
-  **PROGRESSION_ORDER**: Progression order (RPCL, LRCP, CPRL, RLCP)
-  **TRANSFORMATION_TYPE**: Filter transformation used (9x7, 5x3)
-  **USE_SOP**: Start of Packet marker detected (TRUE/FALSE)
-  **USE_EPH**: End of Packet header marker detected (TRUE/FALSE)
-  **GML_JP2_DATA**: OGC GML GeoReferencing box detected (TRUE/FALSE)
-  **COMPRESSION_RATE_TARGET**: Target compression rate used on encoding

See Also
--------

-  Implemented as :source_file:`frmts/ecw/ecwdataset.cpp`.
-  ECW/JP2 SDK available at
   `www.hexagongeospatial.com <http://hexagongeospatial.com/products/data-management-compression/ecw/erdas-ecw-jp2-sdk>`__
-  Further product information available in the `User
   Guide <http://hexagongeospatial.com/products/data-management-compression/ecw/erdas-ecw-jp2-sdk/literature>`__
-  Support for non-GDAL specific issues should be directed to the
   `Hexagon Geospatial public
   forum <https://sgisupport.intergraph.com/infocenter/index?page=forums&forum=507301383c17ef4e013d8dfa30c2007ef1>`__
-  `GDAL ECW Build Hints <http://trac.osgeo.org/gdal/wiki/ECW>`__
