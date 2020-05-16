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
information is fetched in following order (first listed is the most
prioritary): PAM, GeoJP2, GMLJP2, WORLDFILE.

Starting with GDAL 2.2, the allowed sources and their priority order can
be changed with the GDAL_GEOREF_SOURCES configuration option (or
GEOREF_SOURCES open option) whose value is a comma-separated list of the
following keywords : PAM, GEOJP2, GMLJP2, INTERNAL (shortcut for
GEOJP2,GMLJP2), WORLDFILE, NONE. First mentioned sources are the most
prioritary over the next ones. A non mentioned source will be ignored.

For example setting it to "WORLDFILE,PAM,INTERNAL" will make a
geotransformation matrix from a potential worldfile prioritary over PAM
or internal JP2 boxes. Setting it to "PAM,WORLDFILE,GEOJP2" will use the
mentioned sources and ignore GMLJP2 boxes.

Option Options
--------------

The following open option is available:

-  **1BIT_ALPHA_PROMOTION=YES/NO**: Whether a 1-bit alpha channel should
   be promoted to 8-bit. Defaults to YES.

-  **GEOREF_SOURCES=string**: (GDAL > 2.2) Define which georeferencing
   sources are allowed and their priority order. See
   `Georeferencing <#georeferencing>`__ paragraph.

Creation Options:
-----------------

Note: Only Licensing and compression target need to be specified. The
ECW/JP2 SDK will default all other options to recommended settings based
on the input characteristics. Changing other options can *substantially*
impact decoding speed and compatibility with other JPEG2000 toolkits.

-  **LARGE_OK=YES**: *(v3.x SDK only)* Allow compressing files larger
   than 500MB in accordance with EULA terms. Deprecated since v4.x and
   replaced by ECW_ENCODE_KEY & ECW_ENCODE_COMPANY.
-  **ECW_ENCODE_KEY=key**: *(v4.x SDK or higher)* Provide the OEM
   encoding key to unlock encoding capability up to the licensed
   gigapixel limit. The key is approximately 129 hex digits long. The
   Company and Key must match and must be re-generated with each minor
   release of the SDK. It may also be provided globally as a
   configuration option.
-  **ECW_ENCODE_COMPANY=name**: *(v4.x SDK or higher)* Provide the name
   of the company in the issued OEM key (see ECW_ENCODE_KEY). The
   Company and Key must match and must be re-generated with each minor
   release of the SDK. It may also be provided globally as a
   configuration option.
-  **TARGET=percent**: Set the target size reduction as a percentage of
   the original. If not provided defaults to 75 for an 75% reduction.
   TARGET=0 uses lossless compression.
-  **PROJ=name**: Name of the ECW projection string to use. Common
   examples are NUTM11, or GEODETIC.
-  **DATUM=name**: Name of the ECW datum string to use. Common examples
   are WGS84 or NAD83.
-  **GMLJP2=YES/NO**: Indicates whether a GML box conforming to the OGC
   GML in JPEG2000 specification should be included in the file. Unless
   GMLJP2V2_DEF is used, the version of the GMLJP2 box will be version
   1. Defaults to YES.
-  **GMLJP2V2_DEF=filename**: Indicates whether
   a GML box conforming to the `OGC GML in JPEG2000, version
   2 <http://docs.opengeospatial.org/is/08-085r4/08-085r4.html>`__
   specification should be included in the file. *filename* must point
   to a file with a JSon content that defines how the GMLJP2 v2 box
   should be built. See :ref:`GMLJP2v2 definition file
   section <gmjp2v2def>` in documentation of
   the JP2OpenJPEG driver for the syntax of the JSon configuration file.
   It is also possible to directly pass the JSon content inlined as a
   string. If filename is just set to YES, a minimal instance will be
   built.
-  **GeoJP2=YES/NO**: Indicates whether a UUID/GeoTIFF box conforming to
   the GeoJP2 (GeoTIFF in JPEG2000) specification should be included in
   the file. Defaults to YES.
-  **PROFILE=profile**: One of BASELINE_0, BASELINE_1, BASELINE_2, NPJE
   or EPJE. Review the ECW SDK documentation for details on profile
   meanings.
-  **PROGRESSION=LRCP/RLCP/RPCL**: Set the progression order with which
   the JPEG2000 codestream is written. (Default, RPCL)
-  **CODESTREAM_ONLY=YES/NO**: If set to YES, only the compressed
   imagery code stream will be written. If NO a JP2 package will be
   written around the code stream including a variety of meta
   information. (Default, NO)
-  **LEVELS=n**: Resolution levels in pyramid (by default so many that
   the size of the smallest thumbnail image is 64x64 pixels at maximum)
-  **LAYERS=n**: Quality layers (default, 1)
-  **PRECINCT_WIDTH=n**: Precinct Width (default, 64)
-  **PRECINCT_HEIGHT=n**: Precinct Height (default 64)
-  **TILE_WIDTH=n**: Tile Width (default, image width eg. 1 tile). Apart
   from GeoTIFF, in JPEG2000 tiling is not critical for speed if
   precincts are used. The minimum tile size allowed by the standard is
   1024x1024 pixels.
-  **TILE_HEIGHT=n**: Tile Height (default, image height eg. 1 tile)
-  **INCLUDE_SOP=YES/NO**: Output Start of Packet Marker (default false)
-  **INCLUDE_EPH=YES/NO**: Output End of Packet Header Marker (default
   true)
-  **DECOMPRESS_LAYERS=n**: The number of quality layers to decode
-  **DECOMPRESS_RECONSTRUCTION_PARAMETER=n**: IRREVERSIBLE_9x7 or
   REVERSIBLE_5x3
-  **WRITE_METADATA=YES/NO**: Whether metadata should be
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

-  **MAIN_MD_DOMAIN_ONLY=YES/NO**: (Only if
   WRITE_METADATA=YES) Whether only metadata from the main domain should
   be written. Defaults to NO.

"JPEG2000 format does not support creation of GDAL overviews since the
format is already considered to be optimized for "arbitrary overviews".
JP2ECW driver also arranges JP2 codestream to allow optimal access to
power of two overviews. This is controlled with the creation option
LEVELS."

Configuration Options
---------------------

The ERDAS ECW/JP2 SDK supports a variety of `runtime configuration
options <http://trac.osgeo.org/gdal/wiki/ConfigOptions>`__ to control
various features. Most of these are exposed as GDAL configuration
options. See the ECW/JP2 SDK documentation for full details on the
meaning of these options.

-  **ECW_CACHE_MAXMEM=bytes**: maximum bytes of RAM used for in-memory
   caching. If not set, up to one quarter of physical RAM will be used
   by the SDK for in-memory caching.
-  **ECW_TEXTURE_DITHER=TRUE/FALSE**: This may be set to FALSE to
   disable dithering when decompressing ECW files. Defaults to TRUE.
-  **ECW_FORCE_FILE_REOPEN=TRUE/FALSE**: This may be set to TRUE to
   force open a file handle for each file for each connection made.
   Defaults to FALSE.
-  **ECW_CACHE_MAXOPEN=number**: The maximum number of files to keep
   open for ECW file handle caching. Defaults to unlimited.
-  **ECW_AUTOGEN_J2I=TRUE/FALSE**: Controls whether .j2i index files
   should be created when opening jpeg2000 files. Defaults to TRUE.
-  **ECW_RESILIENT_DECODING=TRUE/FALSE**: Controls whether the reader
   should be forgiving of errors in a file, trying to return as much
   data as is available. Defaults to TRUE. If set to FALSE an invalid
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

-  Implemented as ``gdal/frmts/ecw/ecwdataset.cpp``.
-  ECW/JP2 SDK available at
   `www.hexagongeospatial.com <http://hexagongeospatial.com/products/data-management-compression/ecw/erdas-ecw-jp2-sdk>`__
-  Further product information available in the `User
   Guide <http://hexagongeospatial.com/products/data-management-compression/ecw/erdas-ecw-jp2-sdk/literature>`__
-  Support for non-GDAL specific issues should be directed to the
   `Hexagon Geospatial public
   forum <https://sgisupport.intergraph.com/infocenter/index?page=forums&forum=507301383c17ef4e013d8dfa30c2007ef1>`__
-  `GDAL ECW Build Hints <http://trac.osgeo.org/gdal/wiki/ECW>`__
