.. _raster.various:

Various Support GDAL Raster Formats
===================================

AAIGrid -- Arc/Info ASCII Grid
------------------------------

Supported for read and write access, including reading of an affine
georeferencing transform and some projections. This format is the ASCII
interchange format for Arc/Info Grid, and takes the form of an ASCII
file, plus sometimes an associated .prj file. It is normally produced
with the Arc/Info ASCIIGRID command.

The projections support (read if a \*.prj file is available) is quite
limited. Additional sample .prj files may be sent to the maintainer,
warmerdam@pobox.com.

The NODATA value for the grid read is also preserved when available in
the same format as the band data.

By default, the datatype returned for AAIGRID datasets by GDAL is
autodetected, and set to Float32 for grid with floating point values or
Int32 otherwise. This is done by analysing the format of the NODATA
value and, if needed, the data of the grid. From GDAL 1.8.0, you can
explicitly specify the datatype by setting the AAIGRID_DATATYPE
configuration option (Int32, Float32 and Float64 values are supported
currently)

If pixels being written are not square (the width and height of a pixel
in georeferenced units differ) then DX and DY parameters will be output
instead of CELLSIZE. Such files can be used in Golden Surfer, but not
most other ascii grid reading programs. For force the X pixel size to be
used as CELLSIZE use the FORCE_CELLSIZE=YES creation option or resample
the input to have square pixels.

When writing floating-point values, the driver uses the "%.20g" format
pattern as a default. You can consult a `reference
manual <http://en.wikipedia.org/wiki/Printf>`__ for printf to have an
idea of the exact behaviour of this ;-). You can alternatively specify
the number of decimal places with the DECIMAL_PRECISION creation option.
For example, DECIMAL_PRECISION=3 will output numbers with 3 decimal
places(using %lf format). Starting with GDAL 1.11, another option is
SIGNIFICANT_DIGITS=3, which will output 3 significant digits (using %g
format).

The `AIG <#AIG>`__ driver is also available for Arc/Info Binary Grid
format.

NOTE: Implemented as ``gdal/frmts/aaigrid/aaigriddataset.cpp``.

ACE2 -- ACE2
------------

(GDAL >= 1.9.0)

This is a convenience driver to read ACE2 DEMs. Those files contain raw
binary data. The georeferencing is entirely determined by the filename.
Quality, source and confidence layers are of Int16 type, whereas
elevation data is returned as Float32.

`ACE2 product
overview <http://tethys.eaprs.cse.dmu.ac.uk/ACE2/shared/overview>`__

NOTE: Implemented as ``gdal/frmts/raw/ace2dataset.cpp``.

ADRG/ARC Digitized Raster Graphics (.gen/.thf)
----------------------------------------------

Supported by GDAL for read access. Creation is possible, but it must be
considered as experimental and a means of testing read access (although
files created by the driver can be read successfully on another GIS
software)

An ADRG dataset is made of several files. The file recognised by GDAL is
the General Information File (.GEN). GDAL will also need the image file
(.IMG), where the actual data is.

The Transmission Header File (.THF) can also be used as an input to
GDAL. If the THF references more than one image, GDAL will report the
images it is composed of as subdatasets. If the THF references just one
image, GDAL will open it directly.

Overviews, legends and insets are not used. Polar zones (ARC zone 9 and
18) are not supported (due to the lack of test data).

This is an alternative to using the `OGDI Bridge <frmt_ogdi.html>`__ for
ADRG datasets.

See also : the `ADRG specification
(MIL-A-89007) <http://earth-info.nga.mil/publications/specs/printed/89007/89007_ADRG.pdf>`__

AIG -- Arc/Info Binary Grid
---------------------------

Supported by GDAL for read access. This format is the internal binary
format for Arc/Info Grid, and takes the form of a coverage level
directory in an Arc/Info database. To open the coverage select the
coverage directory, or an .adf file (such as hdr.adf) from within it. If
the directory does not contain file(s) with names like w001001.adf then
it is not a grid coverage.

Support includes reading of an affine georeferencing transform, some
projections, and a color table (.clr) if available.

This driver is implemented based on a reverse engineering of the format.
See the `format
description <http://home.gdal.org/projects/aigrid/index.html>`__ for
more details.

The projections support (read if a prj.adf file is available) is quite
limited. Additional sample prj.adf files may be sent to the maintainer,
warmerdam@pobox.com.

NOTE: Implemented as ``gdal/frmts/aigrid/aigdataset.cpp``.

ARG -- Azavea Raster Grid
-------------------------

Driver implementation for a raw format that is used in
`GeoTrellis <http://geotrellis.io/>`__ and called ARG. `ARG format
specification <http://geotrellis.io/documentation/0.9.0/geotrellis/io/arg/>`__.
Format is essentially a raw format, with a companion .JSON file.

NOTE: Implemented as ``gdal/frmts/arg/argdataset.cpp``.

BSB -- Maptech/NOAA BSB Nautical Chart Format
---------------------------------------------

BSB Nautical Chart format is supported for read access, including
reading the colour table and the reference points (as GCPs). Note that
the .BSB files cannot be selected directly. Instead select the .KAP
files. Versions 1.1, 2.0 and 3.0 have been tested successfully.

This driver should also support GEO/NOS format as supplied by Softchart.
These files normally have the extension .nos with associated .geo files
containing georeferencing ... the .geo files are currently ignored.

This driver is based on work by Mike Higgins. See the
frmts/bsb/bsb_read.c files for details on patents affecting BSB format.

Starting with GDAL 1.6.0, it is possible to select an alternate color
palette via the BSB_PALETTE configuration option. The default value is
RGB. Other common values that can be found are : DAY, DSK, NGT, NGR,
GRY, PRC, PRG...

NOTE: Implemented as ``gdal/frmts/bsb/bsbdataset.cpp``.

BT -- VTP .bt Binary Terrain Format
-----------------------------------

The .bt format is used for elevation data in the VTP software. The
driver includes support for reading and writing .bt 1.3 format including
support for Int16, Int32 and Float32 pixel data types.

The driver does **not** support reading or writing gzipped (.bt.gz) .bt
files even though this is supported by the VTP software. Please unpack
the files before using with GDAL using the "gzip -d file.bt.gz".

Projections in external .prj files are read and written, and support for
most internally defined coordinate systems is also available.

Read/write imagery access with the GDAL .bt driver is terribly slow due
to a very inefficient access strategy to this column oriented data. This
could be corrected, but it would be a fair effort.

NOTE: Implemented as ``gdal/frmts/raw/btdataset.cpp``.

See Also: The `BT file
format <http://www.vterrain.org/Implementation/Formats/BT.html>`__ is
defined on the `VTP <http://www.vterrain.org/>`__ web site.

CEOS -- CEOS Image
------------------

This is a simple, read-only reader for ceos image files. To use, select
the main imagery file. This driver reads only the image data, and does
not capture any metadata, or georeferencing.

This driver is known to work with CEOS data produced by Spot Image, but
will have problems with many other data sources. In particular, it will
only work with eight bit unsigned data.

See the separate `SAR_CEOS <#SAR_CEOS>`__ driver for access to SAR CEOS
data products.

NOTE: Implemented as ``gdal/frmts/ceos/ceosdataset.cpp``.

CTG -- USGS LULC Composite Theme Grid
-------------------------------------

(GDAL >= 1.9.0)

This driver can read USGS Land Use and Land Cover (LULC) grids encoded
in the Character Composite Theme Grid (CTG) format. Each file is
reported as a 6-band dataset of type Int32. The meaning of each band is
the following one :

#. Land Use and Land Cover Code
#. Political units Code
#. Census county subdivisions and SMSA tracts Code
#. Hydrologic units Code
#. Federal land ownership Code
#. State land ownership Code

Those files are typically named grid_cell.gz, grid_cell1.gz or
grid_cell2.gz on the USGS site.

-  `Land Use and Land Cover Digital Data (Data Users Guide
   4) <http://edc2.usgs.gov/geodata/LULC/LULCDataUsersGuide.pdf>`__ -
   PDF version from USGS
-  `Land Use and Land Cover Digital Data (Data Users Guide
   4) <http://www.vterrain.org/Culture/LULC/Data_Users_Guide_4.html>`__
   - HTML version converted by Ben Discoe
-  `USGS LULC data at 250K and
   100K <http://edcftp.cr.usgs.gov/pub/data/LULC>`__

NOTE: Implemented as ``gdal/frmts/ctg/ctgdataset.cpp``.

DDS -- DirectDraw Surface
-------------------------

(GDAL >= 1.10.0)

Supported for writing and creation. The DirectDraw Surface file format
(uses the filename extension DDS), from Microsoft, is a standard for
storing data compressed with the lossy S3 Texture Compression (S3TC)
algorithm. The DDS format and compression are provided by the crunch
library.

The driver supports the following texture formats: DXT1. DXT1A, DXT3
(default) and DXT5. You can set the texture format using the creation
option FORMAT.

The driver supports the following compression quality: SUPERFAST, FAST,
NORMAL (default), BETTER and UBER. You can set the compression quality
using the creation option QUALITY.

More information about `Crunch Lib <http://code.google.com/p/crunch/>`__

NOTE: Implemented as ``gdal/frmts/dds/ddsdataset.cpp``.

DIMAP -- Spot DIMAP
-------------------

This is a read-only read for Spot DIMAP described images. To use, select
the METADATA.DIM file in a product directory, or the product directory
itself.

The imagery is in a distinct imagery file, often a TIFF file, but the
DIMAP dataset handles accessing that file, and attaches geolocation and
other metadata to the dataset from the metadata xml file.

From GDAL 1.6.0, the content of the <Spectral_Band_Info> node is
reported as metadata at the level of the raster band. Note that the
content of the Spectral_Band_Info of the first band is still reported as
metadata of the dataset, but this should be considered as a deprecated
way of getting this information.

NOTE: Implemented as ``gdal/frmts/dimap/dimapdataset.cpp``.

DODS/OPeNDAP -- Read rasters from DODS/OPeNDAP servers
------------------------------------------------------

Support for read access to DODS/OPeNDAP servers. Pass the DODS/OPeNDAP
URL to the driver as you would when accessing a local file. The URL
specifies the remote server, data set and raster within the data set. In
addition, you must tell the driver which dimensions are to be
interpreted as distinct bands as well as which correspond to Latitude
and Longitude. See the file README.DODS for more detailed information.

DOQ1 -- First Generation USGS DOQ
---------------------------------

Support for read access, including reading of an affine georeferencing
transform, and capture of the projection string. This format is the old,
unlabelled DOQ (Digital Ortho Quad) format from the USGS.

NOTE: Implemented as ``gdal/frmts/raw/doq1dataset.cpp``.

DOQ2 -- New Labelled USGS DOQ
-----------------------------

Support for read access, including reading of an affine georeferencing
transform, capture of the projection string and reading of other
auxiliary fields as metadata. This format is the new, labelled DOQ
(Digital Ortho Quad) format from the USGS.

This driver was implemented by Derrick J Brashear.

NOTE: Implemented as ``gdal/frmts/raw/doq2dataset.cpp``.

See Also: `USGS DOQ
Standards <http://rockyweb.cr.usgs.gov/nmpstds/doqstds.html>`__

E00GRID -- Arc/Info Export E00 GRID
-----------------------------------

(GDAL >= 1.9.0)

GDAL supports reading DEMs/rasters exported as E00 Grids.

The driver has been tested against datasets such as the one available on
ftp://msdis.missouri.edu/pub/dem/24k/county/

NOTE: Implemented as ``gdal/frmts/e00grid/e00griddataset.cpp``.

EHdr -- ESRI .hdr Labelled
--------------------------

GDAL supports reading and writing the ESRI .hdr labeling format, often
referred to as ESRI BIL format. Eight, sixteen and thirty-two bit
integer raster data types are supported as well as 32 bit floating
point. Coordinate systems (from a .prj file), and georeferencing are
supported. Unrecognized options in the .hdr file are ignored. To open a
dataset select the file with the image file (often with the extension
.bil). If present .clr color table files are read, but not written. If
present, image.rep file will be read to extract the projection system of
SpatioCarte Defense 1.0 raster products.

This driver does not always do well differentiating between floating
point and integer data. The GDAL extension to the .hdr format to
differentiate is to add a field named PIXELTYPE with values of either
FLOAT, SIGNEDINT or UNSIGNEDINT. In combination with the NBITS field it
is possible to described all variations of pixel types.

eg.

::

     ncols 1375
     nrows 649
     cellsize 0.050401
     xllcorner -130.128639
     yllcorner 20.166799
     nodata_value 9999.000000
     nbits 32
     pixeltype float
     byteorder msbfirst

This driver may be sufficient to read GTOPO30 data.

NOTE: Implemented as ``gdal/frmts/raw/ehdrdataset.cpp``.

See Also:

-  `ESRI whitepaper: + Extendable Image Formats for ArcView GIS 3.1 and
   3.2 <http://downloads.esri.com/support/whitepapers/other_/eximgav.pdf>`__
   (BIL, see p. 5)
-  `GTOPO30 - Global Topographic
   Data <http://edcdaac.usgs.gov/gtopo30/gtopo30.html>`__
-  `GTOPO30
   Documentation <http://edcdaac.usgs.gov/gtopo30/README.html>`__
-  `SpatioCarte Defense 1.0
   specification <http://eden.ign.fr/download/pub/doc/emabgi/spdf10.pdf/download>`__
   (in French)
-  `SRTMHGT Driver <#SRTMHGT>`__

ECRG Table Of Contents (TOC.xml)
--------------------------------

Starting with GDAL 1.9.0

This is a read-only reader for ECRG (Enhanced Compressed Raster Graphic)
products, that uses the table of content file, TOC.xml, and exposes it
as a virtual dataset whose coverage is the set of ECRG frames contained
in the table of content.

The driver will report a different subdataset for each subdataset found
in the TOC.xml file. Each subdataset consists of the frames of same
product id, disk id, and starting with GDAL 1.11.3, with same scale.

Result of a gdalinfo on a TOC.xml file.

::

   Subdatasets:
     SUBDATASET_1_NAME=ECRG_TOC_ENTRY:ECRG:FalconView:1_500_K:ECRG_Sample/EPF/TOC.xml
     SUBDATASET_1_DESC=Product ECRG, Disk FalconView, Scale 1:500 K

See Also:

-  `NITF driver <frmt_nitf.html>`__ : format of the ECRG frames
-  `MIL-PRF-32283 <http://www.everyspec.com/MIL-PRF/MIL-PRF+%28030000+-+79999%29/MIL-PRF-32283_26022/>`__
   : specification of ECRG products

NOTE: Implemented as ``gdal/frmts/nitf/ecrgtocdataset.cpp``

EIR -- Erdas Imagine Raw
------------------------

GDAL supports the Erdas Imagine Raw format for read access including 1,
2, 4, 8, 16 and 32bit unsigned integers, 16 and 32bit signed integers
and 32 and 64bit complex floating point. Georeferencing is supported.

To open a dataset select the file with the header information. The
driver finds the image file from the header information. Erdas documents
call the header file the "raw" file and it may have the extension .raw
while the image file that contains the actual raw data may have the
extension .bl.

NOTE: Implemented as ``gdal/frmts/raw/eirdataset.cpp``.

ENVI - ENVI .hdr Labelled Raster
--------------------------------

GDAL supports some variations of raw raster files with associated ENVI
style .hdr files describing the format. To select an existing ENVI
raster file select the binary file containing the data (as opposed to
the .hdr file), and GDAL will find the .hdr file by replacing the
dataset extension with .hdr.

GDAL should support reading bil, bip and bsq interleaved formats, and
most pixel types are supported, including 8bit unsigned, 16 and 32bit
signed and unsigned integers, 32bit and 64 bit floating point, and 32bit
and 64bit complex floating point. There is limited support for
recognising map_info keywords with the coordinate system and
georeferencing. In particular, UTM and State Plane should work.

Starting with GDAL 1.10, all ENVI header fields will be stored in the
ENVI metadata domain, and all of these can then be written out to the
header file.

Creation Options:

-  **INTERLEAVE=BSQ/BIP/BIL**: Force the generation specified type of
   interleaving. **BSQ** --- band sequential (default), **BIP** --- data
   interleaved by pixel, **BIL** --- data interleaved by line.
-  **SUFFIX=REPLACE/ADD**: Force adding ".hdr" suffix to supplied
   filename, e.g. if user selects "file.bin" name for output dataset,
   "file.bin.hdr" header file will be created. By default header file
   suffix replaces the binary file suffix, e.g. for "file.bin" name
   "file.hdr" header file will be created.

NOTE: Implemented as ``gdal/frmts/raw/envidataset.cpp``.

Envisat -- Envisat Image Product
--------------------------------

GDAL supports the Envisat product format for read access. All sample
types are supported. Files with two matching measurement datasets (MDS)
are represented as having two bands. Currently all ASAR Level 1 and
above products, and some MERIS and AATSR products are supported.

The control points of the GEOLOCATION GRID ADS dataset are read if
available, generally giving a good coverage of the dataset. The GCPs are
in WGS84.

Virtually all key/value pairs from the MPH and SPH (primary and
secondary headers) are copied through as dataset level metadata.

ASAR and MERIS parameters contained in the ADS and GADS records
(excluded geolocation ones) can be retrieved as key/value pairs using
the "RECORDS" metadata domain.

NOTE: Implemented as ``gdal/frmts/envisat/envisatdataset.cpp``.

See Also: `Envisat Data
Products <http://envisat.esa.int/dataproducts/>`__ at ESA.

FITS -- Flexible Image Transport System
---------------------------------------

FITS is a format used mainly by astronomers, but it is a relatively
simple format that supports arbitrary image types and multi-spectral
images, and so has found its way into GDAL. FITS support is implemented
in terms of the standard `CFITSIO
library <http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html>`__,
which you must have on your system in order for FITS support to be
enabled. Both reading and writing of FITS files is supported. At the
current time, no support for a georeferencing system is implemented, but
WCS (World Coordinate System) support is possible in the future.

Non-standard header keywords that are present in the FITS file will be
copied to the dataset's metadata when the file is opened, for access via
GDAL methods. Similarly, non-standard header keywords that the user
defines in the dataset's metadata will be written to the FITS file when
the GDAL handle is closed.

Note to those familiar with the CFITSIO library: The automatic rescaling
of data values, triggered by the presence of the BSCALE and BZERO header
keywords in a FITS file, is disabled in GDAL. Those header keywords are
accessible and updatable via dataset metadata, in the same was as any
other header keywords, but they do not affect reading/writing of data
values from/to the file.

NOTE: Implemented as ``gdal/frmts/fits/fitsdataset.cpp``.

GenBin - Generic Binary (.hdr labelled)
---------------------------------------

This driver supporting reading "Generic Binary" files labelled with a
.hdr file, but distinct from the more common ESRI labelled .hdr format
(EHdr driver). The origin of this format is not entirely clear. The .hdr
files supported by this driver are look something like this:

::

   {{{
   BANDS:      1
   ROWS:    6542
   COLS:    9340
   ...
   }}}

Pixel data types of U8, U16, S16, F32, F64, and U1 (bit) are supported.
Georeferencing and coordinate system information should be supported
when provided.

NOTE: Implemented as ``gdal/frmts/raw/genbindataset.cpp``.

GRASSASCIIGrid -- GRASS ASCII Grid
----------------------------------

(GDAL >= 1.9.0)

Supports reading GRASS ASCII grid format (similar to Arc/Info ASCIIGRID
command).

By default, the datatype returned for GRASS ASCII grid datasets by GDAL
is autodetected, and set to Float32 for grid with floating point values
or Int32 otherwise. This is done by analysing the format of the null
value and the first 100k bytes of data of the grid. You can also
explicitly specify the datatype by setting the GRASSASCIIGRID_DATATYPE
configuration option (Int32, Float32 and Float64 values are supported
currently)

NOTE: Implemented as ``gdal/frmts/aaigrid/aaigriddataset.cpp``.

GSAG -- Golden Software ASCII Grid File Format
----------------------------------------------

This is the ASCII-based (human-readable) version of one of the raster
formats used by Golden Software products (such as the Surfer series).
This format is supported for both reading and writing (including create,
delete, and copy). Currently the associated formats for color, metadata,
and shapes are not supported.

NOTE: Implemented as ``gdal/frmts/gsg/gsagdataset.cpp``.

GSBG -- Golden Software Binary Grid File Format
-----------------------------------------------

This is the binary (non-human-readable) version of one of the raster
formats used by Golden Software products (such as the Surfer series).
Like the ASCII version, this format is supported for both reading and
writing (including create, delete, and copy). Currently the associated
formats for color, metadata, and shapes are not supported.

NOTE: Implemented as ``gdal/frmts/gsg/gsbgdataset.cpp``.

GS7BG -- Golden Software Surfer 7 Binary Grid File Format
---------------------------------------------------------

This is the binary (non-human-readable) version of one of the raster
formats used by Golden Software products (such as the Surfer series).
This format differs from the `GSBG <#GSBG>`__ format (also known as
Surfer 6 binary grid format), it is more complicated and flexible.

NOTE: Implemented as ``gdal/frmts/gsg/gs7bgdataset.cpp``.

GXF -- Grid eXchange File
-------------------------

This is a raster exchange format propagated by Geosoft, and made a
standard in the gravity/magnetics field. GDAL supports reading (but not
writing) GXF-3 files, including support for georeferencing information,
and projections.

By default, the datatype returned for GXF datasets by GDAL is Float32.
From GDAL 1.8.0, you can specify the datatype by setting the
GXF_DATATYPE configuration option (Float64 supported currently)

Details on the supporting code, and format can be found on the
`GXF-3 <http://home.gdal.org/projects/gxf/index.html>`__ page.

NOTE: Implemented as ``gdal/frmts/gxf/gxfdataset.cpp``.

IDA -- Image Display and Analysis
---------------------------------

GDAL supports reading and writing IDA images with some limitations. IDA
images are the image format of WinDisp 4. The files are always one band
only of 8bit data. IDA files often have the extension .img though that
is not required.

Projection and georeferencing information is read though some
projections (i.e. Meteosat, and Hammer-Aitoff) are not supported. When
writing IDA files the projection must have a false easting and false
northing of zero. The support coordinate systems in IDA are Geographic,
Lambert Conformal Conic, Lambert Azimuth Equal Area, Albers Equal-Area
Conic and Goodes Homolosine.

IDA files typically contain values scaled to 8bit via a slope and
offset. These are returned as the slope and offset values of the bands
and they must be used if the data is to be rescaled to original raw
values for analysis.

NOTE: Implemented as ``gdal/frmts/raw/idadataset.cpp``.

See Also:
`WinDisp <http://www.fao.org/giews/english/windisp/windisp.htm>`__

IGNFHeightASCIIGrid -- IGN-France height correction ASCII grids
---------------------------------------------------------------

(GDAL >= 2.4.0)

Supports reading IGN-France height correction ASCII grids (.txt, .mnt,
.gra).

See also:

-  `Format description (in
   French) <https://geodesie.ign.fr/contenu/fichiers/documentation/grilles/notices/Grilles-MNT-TXT_Formats.pdf>`__
-  `Height correction
   grids <https://geodesie.ign.fr/index.php?page=grilles>`__

ILWIS -- Raster Map
-------------------

This driver implements reading and writing of ILWIS raster maps and map
lists. Select the raster files with the.mpr (for raster map) or .mpl
(for maplist) extensions

Features:

-  Support for Byte, Int16, Int32 and Float64 pixel data types.
-  Supports map lists with an associated set of ILWIS raster maps.
-  Read and write geo-reference (.grf). Support for geo-referencing
   transform is limited to north-oriented GeoRefCorner only. If possible
   the affine transform is computed from the corner coordinates.
-  Read and write coordinate files (.csy). Support is limited to:
   Projection type of Projection and Lat/Lon type that are defined in
   .csy file, the rest of pre-defined projection types are ignored.

Limitations:

-  Map lists with internal raster map storage (such as produced through
   Import General Raster) are not supported.
-  ILWIS domain (.dom) and representation (.rpr) files are currently
   ignored.

IRIS - Vaisala's weather radar software format
----------------------------------------------

This read-only GDAL driver is designed to provide access to the products
generated by the IRIS weather radar software.

IRIS software format includes a lot of products, and some of them aren't
even raster. The driver can read currently:

-  PPI (reflectivity and speed): Plan position indicator
-  CAPPI: Constant Altitude Plan position indicator
-  RAIN1: Hourly rainfall accumulation
-  RAINN: N-Hour rainfall accumulation
-  TOPS: Height for selectable dBZ contour
-  VIL: Vertically integrated liquid for selected layer
-  MAX: Column Max Z WF W/NS Sections

Most of the metadata is read.

Vaisala provides information about the format and software at
http://www.vaisala.com/en/defense/products/weatherradar/Pages/IRIS.aspx.

NOTE: Implemented as ``gdal/frmts/iris/irisdataset.cpp``.

ISCE
----

Driver for the image formats used in the JPL's Interferometric synthetic
aperture radar Scientific Computing Environment (ISCE). Only images with
data types mappable to GDAL data types are supported.

Image properties are stored under the ISCE metadata domain, but there is
currently no support to access underlying components elements and their
properties. Likewise, ISCE domain metadata will be saved as properties
in the image XML file.

Georeferencing is not yet implemented.

The ACCESS_MODE property is not currently honored.

The only creation option currently is SCHEME, which value (BIL, BIP,
BSQ) determine the interleaving (default is BIP).

NOTE: Implemented as ``gdal/frmts/raw/iscedataset.cpp``.

JDEM -- Japanese DEM (.mem)
---------------------------

GDAL includes read support for Japanese DEM files, normally having the
extension .mem. These files are a product of the Japanese Geographic
Survey Institute.

These files are represented as having one 32bit floating band with
elevation data. The georeferencing of the files is returned as well as
the coordinate system (always lat/long on the Tokyo datum).

There is no update or creation support for this format.

NOTE: Implemented as ``gdal/frmts/jdem/jdemdataset.cpp``.

See Also: `Geographic Survey Institute (GSI) Web
Site. <http://www.gsi.go.jp/ENGLISH/>`__

KRO -- KOLOR Raw format
-----------------------

(GDAL >= 1.11)

Supported for read access, update and creation. This format is a binary
raw format, that supports data of several depths ( 8 bit, unsigned
integer 16 bit and floating point 32 bit) and with several band number
(3 or 4 typically, for RGB and RGBA). There is no file size limit,
except the limitation of the file system.

`Specification of the
format <http://www.autopano.net/wiki-en/Format_KRO>`__

NOTE: Implemented as ``gdal/frmts/raw/krodataset.cpp``.

LAN -- Erdas 7.x .LAN and .GIS
------------------------------

GDAL supports reading and writing Erdas 7.x .LAN and .GIS raster files.
Currently 4bit, 8bit and 16bit pixel data types are supported for
reading and 8bit and 16bit for writing.

GDAL does read the map extents (geotransform) from LAN/GIS files, and
attempts to read the coordinate system information. However, this format
of file does not include complete coordinate system information, so for
state plane and UTM coordinate systems a LOCAL_CS definition is returned
with valid linear units but no other meaningful information.

The .TRL, .PRO and worldfiles are ignored at this time.

NOTE: Implemented as ``gdal/frmts/raw/landataset.cpp``

Development of this driver was financially supported by Kevin Flanders
of (`PeopleGIS <http://www.peoplegis.com>`__).

MFF -- Vexcel MFF Raster
------------------------

GDAL includes read, update, and creation support for Vexcel's MFF raster
format. MFF dataset consist of a header file (typically with the
extension .hdr) and a set of data files with extensions like .x00, .b00
and so on. To open a dataset select the .hdr file.

Reading lat/long GCPs (TOP_LEFT_CORNER, ...) is supported but there is
no support for reading affine georeferencing or projection information.

Unrecognized keywords from the .hdr file are preserved as metadata.

All data types with GDAL equivalents are supported, including 8, 16, 32
and 64 bit data precisions in integer, real and complex data types. In
addition tile organized files (as produced by the Vexcel SAR Processor -
APP) are supported for reading.

On creation (with a format code of MFF) a simple, ungeoreferenced raster
file is created.

MFF files are not normally portable between systems with different byte
orders. However GDAL honours the new BYTE_ORDER keyword which can take a
value of LSB (Integer -- little endian), and MSB (Motorola -- big
endian). This may be manually added to the .hdr file if required.

NOTE: Implemented as ``gdal/frmts/raw/mffdataset.cpp``.

NDF -- NLAPS Data Format
------------------------

GDAL has limited support for reading NLAPS Data Format files. This is a
format primarily used by the Eros Data Center for distribution of
Landsat data. NDF datasets consist of a header file (often with the
extension .H1) and one or more associated raw data files (often .I1,
.I2, ...). To open a dataset select the header file, often with the
extension .H1, .H2 or .HD.

The NDF driver only supports 8bit data. The only supported projection is
UTM. NDF version 1 (NDF_VERSION=0.00) and NDF version 2 are both
supported.

NOTE: Implemented as ``gdal/frmts/raw/ndfdataset.cpp``.

See Also: `NLAPS Data Format
Specification <http://landsat.usgs.gov/documents/NLAPSII.pdf>`__.

GMT -- GMT Compatible netCDF
----------------------------

GDAL has limited support for reading and writing netCDF *grid* files.
NetCDF files that are not recognised as grids (they lack variables
called dimension, and z) will be silently ignored by this driver. This
driver is primarily intended to provide a mechanism for grid interchange
with the `GMT <http://gmt.soest.hawaii.edu/>`__ package. The netCDF
driver should be used for more general netCDF datasets.

The units information in the file will be ignored, but x_range, and
y_range information will be read to get georeferenced extents of the
raster. All netCDF data types should be supported for reading.

Newly created files (with a type of ``GMT``) will always have units of
"meters" for x, y and z but the x_range, y_range and z_range should be
correct. Note that netCDF does not have an unsigned byte data type, so
8bit rasters will generally need to be converted to Int16 for export to
GMT.

NetCDF support in GDAL is optional, and not compiled in by default.

NOTE: Implemented as ``gdal/frmts/netcdf/gmtdataset.cpp``.

See Also: `Unidata NetCDF
Page <http://www.unidata.ucar.edu/software/netcdf/>`__

PAux -- PCI .aux Labelled Raw Format
------------------------------------

GDAL includes a partial implementation of the PCI .aux labelled raw
raster file for read, write and creation. To open a PCI labelled file,
select the raw data file itself. The .aux file (which must have a common
base name) will be checked for automatically.

The format type for creating new files is ``PAux``. All PCI data types
(8U, 16U, 16S, and 32R) are supported. Currently georeferencing,
projections, and other metadata is ignored.

Creation Options:

-  **INTERLEAVE=PIXEL/LINE/BAND**: Establish output interleaving, the
   default is BAND.

NOTE: Implemented as ``gdal/frmts/raw/pauxdataset.cpp``.

See Also: `PCI's .aux Format
Description <http://www.pcigeomatics.com/cgi-bin/pcihlp/GDB%7CSupported+File+Formats%7CRaw+Binary+Image+Format+(RAW)%7CRaw+.aux+Format>`__

PCRaster raster file format
---------------------------

GDAL includes support for reading and writing PCRaster raster files.
PCRaster is a dynamic modeling system for distributed simulation models.
The main applications of PCRaster are found in environmental modeling:
geography, hydrology, ecology to name a few. Examples include models for
research on global hydrology, vegetation competition models, slope
stability models and land use change models.

The driver reads all types of PCRaster maps: booleans, nominal,
ordinals, scalar, directional and ldd. The same cell representation used
to store values in the file is used to store the values in memory.

The driver detects whether the source of the GDAL raster is a PCRaster
file. When such a raster is written to a file the value scale of the
original raster will be used. The driver **always** writes values using
UINT1, INT4 or REAL4 cell representations, depending on the value scale:

============ ===================
Value scale  Cell representation
============ ===================
VS_BOOLEAN   CR_UINT1
VS_NOMINAL   CR_INT4
VS_ORDINAL   CR_INT4
VS_SCALAR    CR_REAL4
VS_DIRECTION CR_REAL4
VS_LDD       CR_UINT1
============ ===================

For rasters from other sources than a PCRaster raster file a value scale
and cell representation is determined according to the following rules:

Source type

Target value scale

Target cell representation

GDT_Byte

VS_BOOLEAN

CR_UINT1

GDT_Int32

VS_NOMINAL

CR_INT4

GDT_Float32

VS_SCALAR

CR_REAL4

GDT_Float64

VS_SCALAR

CR_REAL4

The driver can convert values from one supported cell representation to
another. It cannot convert to unsupported cell representations. For
example, it is not possible to write a PCRaster raster file from values
which are used as CR_INT2 (GDT_Int16).

Although the de-facto file extension of a PCRaster raster file is .map,
the PCRaster software does not require a standardized file extension.

NOTE: Implemented as ``gdal/frmts/pcraster/pcrasterdataset.cpp``.

See also: `PCRaster website at Utrecht
University <http://pcraster.geo.uu.nl>`__.

PNG -- Portable Network Graphics
--------------------------------

GDAL includes support for reading, and creating .png files. Greyscale,
pseudo-colored, Paletted, RGB and RGBA PNG files are supported as well
as precisions of eight and sixteen bits per sample.

PNG files are linearly compressed, so random reading of large PNG files
can be very inefficient (resulting in many restarts of decompression
from the start of the file).

Text chunks are translated into metadata, typically with multiple lines
per item. `World files <#WLD>`__ with the extensions of .pgw, .pngw or
.wld will be read. Single transparency values in greyscale files will be
recognised as a nodata value in GDAL. Transparent index in paletted
images are preserved when the color table is read.

PNG files can be created with a type of PNG, using the CreateCopy()
method, requiring a prototype to read from. Writing includes support for
the various image types, and will preserve transparency/nodata values.
Georeferencing .wld files are written if option WORLDFILE is set. All
pixel types other than 16bit unsigned will be written as eight bit.

Starting with GDAL 1.9.0, XMP metadata can be extracted from the file,
and will be stored as XML raw content in the xml:XMP metadata domain.

Color Profile Metadata
----------------------

Starting with GDAL 1.11, GDAL can deal with the following color profile
metadata in the COLOR_PROFILE domain:

-  SOURCE_ICC_PROFILE (Base64 encoded ICC profile embedded in file. If
   available, other tags are ignored.)
-  SOURCE_ICC_PROFILE_NAME : ICC profile name. sRGB is recognized as a
   special value.
-  SOURCE_PRIMARIES_RED (xyY in "x,y,1" format for red primary.)
-  SOURCE_PRIMARIES_GREEN (xyY in "x,y,1" format for green primary)
-  SOURCE_PRIMARIES_BLUE (xyY in "x,y,1" format for blue primary)
-  SOURCE_WHITEPOINT (xyY in "x,y,1" format for whitepoint)
-  PNG_GAMMA

Note that these metadata properties can only be used on the original raw
pixel data. If automatic conversion to RGB has been done, the color
profile information cannot be used.

All these metadata tags can be used as creation options.

Creation Options:

-  **WORLDFILE=YES**: Force the generation of an associated ESRI world
   file (with the extension .wld). See `World File <#WLD>`__ section for
   details.
-  **ZLEVEL=n**: Set the amount of time to spend on compression. The
   default is 6. A value of 1 is fast but does no compression, and a
   value of 9 is slow but does the best compression.
-  **TITLE=value**: Title, written in a TEXT or iTXt chunk (GDAL >= 2.0
   )
-  **DESCRIPTION=value**: Description, written in a TEXT or iTXt chunk
   (GDAL >= 2.0 )
-  **COPYRIGHT=value**: Copyright, written in a TEXT or iTXt chunk (GDAL
   >= 2.0 )
-  **COMMENT=value**: Comment, written in a TEXT or iTXt chunk (GDAL >=
   2.0 )
-  **WRITE_METADATA_AS_TEXT=YES/NO**: Whether to write source dataset
   metadata in TEXT chunks (GDAL >= 2.0 )
-  **NBITS=1/2/4**: Force number of output bits (GDAL >= 2.1 )

NOTE: Implemented as ``gdal/frmts/png/pngdataset.cpp``.

PNG support is implemented based on the libpng reference library. More
information is available at http://www.libpng.org/pub/png.

PNM -- Netpbm (.pgm, .ppm)
--------------------------

GDAL includes support for reading, and creating .pgm (greyscale), and
.ppm (RGB color) files compatible with the Netpbm tools. Only the binary
(raw) formats are supported.

Netpbm files can be created with a type of PNM.

Creation Options:

-  **MAXVAL=n**: Force setting the maximum color value to **n** in the
   output PNM file. May be useful if you planning to use the output
   files with software which is not liberal to this value.

NOTE: Implemented as ``gdal/frmts/raw/pnmdataset.cpp``.

ROI_PAC
-------

Driver for the image formats used in the JPL's ROI_PAC project
(https://aws.roipac.org/). All image type are supported excepted .raw
images.

Metadata are stored in the ROI_PAC domain.

Georeferencing is supported, but expect problems when using the UTM
projection, as ROI_PAC format do not store any hemisphere field.

When creating files, you have to be able to specify the right data type
corresponding to the file type (slc, int, etc), else the driver will
output an error.

NOTE: Implemented as ``gdal/frmts/raw/roipacdataset.cpp``.

Raster Product Format/RPF (a.toc)
---------------------------------

This is a read-only reader for RPF products, like CADRG or CIB, that
uses the table of content file - A.TOC - from a RPF exchange, and
exposes it as a virtual dataset whose coverage is the set of frames
contained in the table of content.

The driver will report a different subdataset for each subdataset found
in the A.TOC file.

Result of a gdalinfo on a A.TOC file.

::

   Subdatasets:
     SUBDATASET_1_NAME=NITF_TOC_ENTRY:CADRG_GNC_5M_1_1:GNCJNCN/rpf/a.toc
     SUBDATASET_1_DESC=CADRG:GNC:Global Navigation Chart:5M:1:1
   [...]
     SUBDATASET_5_NAME=NITF_TOC_ENTRY:CADRG_GNC_5M_7_5:GNCJNCN/rpf/a.toc
     SUBDATASET_5_DESC=CADRG:GNC:Global Navigation Chart:5M:7:5
     SUBDATASET_6_NAME=NITF_TOC_ENTRY:CADRG_JNC_2M_1_6:GNCJNCN/rpf/a.toc
     SUBDATASET_6_DESC=CADRG:JNC:Jet Navigation Chart:2M:1:6
   [...]
     SUBDATASET_13_NAME=NITF_TOC_ENTRY:CADRG_JNC_2M_8_13:GNCJNCN/rpf/a.toc
     SUBDATASET_13_DESC=CADRG:JNC:Jet Navigation Chart:2M:8:13

In some situations, `NITF <frmt_nitf.html>`__ tiles inside a subdataset
don't share the same palettes. The RPFTOC driver will do its best to
remap palettes to the reported palette by gdalinfo (which is the palette
of the first tile of the subdataset). In situations where it would not
give a good result, you can try to set the RPFTOC_FORCE_RGBA environment
variable to TRUE before opening the subdataset. This will cause the
driver to expose the subdataset as a RGBA dataset, instead of a paletted
one.

It is possible to build external overviews for a subdataset. The
overview for the first subdataset will be named A.TOC.1.ovr for example,
for the second dataset it will be A.TOC.2.ovr, etc. Note that you must
re-open the subdataset with the same setting of RPFTOC_FORCE_RGBA as the
one you have used when you have created it. Do not use any method other
than NEAREST resampling when building overviews on a paletted subdataset
(RPFTOC_FORCE_RGBA unset)

A gdalinfo on one of this subdataset will return the various NITF
metadata, as well as the list of the NITF tiles of the subdataset.

See Also:

-  `OGDI Bridge <frmt_ogdi.html>`__ : the RPFTOC driver gives an
   equivalent functionality (without external dependency) to the RPF
   driver from the OGDI library.
-  `MIL-PRF-89038 <http://www.everyspec.com/MIL-PRF/MIL-PRF+%28080000+-+99999%29/MIL-PRF-89038_25371/>`__
   : specification of RPF, CADRG, CIB products

NOTE: Implemented as ``gdal/frmts/nitf/rpftocdataset.cpp``

RRASTER -- R Raster
-------------------

(GDAL >= 2.2)

This is a read-only reader for the datasets handled by the `R Raster
package <https://cran.r-project.org/web/packages/raster/index.html>`__.
Those datasets are made of a .grd file, which is a text header file, and
a .gri binary file containing the raster data itself. The .grd is the
file opened by GDAL. Starting with GDAL 2.3, the driver will read
ratvalues as RAT or color tables. Layer names will be assigned to GDAL
band description. The 'creator' and 'created' attributes of the
'[general]' section will be assigned to the GDAL 'CREATOR' and 'CREATED'
dataset metadata items.

Starting with GDAL 2.3, the driver has write capabilities. Color tables
or RAT will be written. The 'CREATOR' and 'CREATED' dataset metadata
items will be written as the 'creator' and 'created' attributes of the
'[general]' section. Band description will be written as the 'layername'
attribute of the '[description]' section.

The following creation options are supported:

-  INTERLEAVE=BIP/BIL/BSQ. Respectively band interleaved by pixel, band
   interleaved by line, band sequential. Default to BIL
-  PIXELTYPE=SIGNEDBYTE. To write Byte bands as signed byte instead of
   unsigned byte.

See Also:

-  Description of the `"rasterfile"
   format <https://cran.r-project.org/web/packages/raster/vignettes/rasterfile.pdf>`__

SAR_CEOS -- CEOS SAR Image
--------------------------

This is a read-only reader for CEOS SAR image files. To use, select the
main imagery file.

This driver works with most Radarsat and ERS data products, including
single look complex products; however, it is unlikely to work for
non-Radar CEOS products. The simpler `CEOS <#CEOS>`__ driver is often
appropriate for these.

This driver will attempt to read 15 lat/long GCPS by sampling the
per-scanline CEOS superstructure information. It also captures various
pieces of metadata from various header files, including:

::

     CEOS_LOGICAL_VOLUME_ID=EERS-1-SAR-MLD
     CEOS_PROCESSING_FACILITY=APP
     CEOS_PROCESSING_AGENCY=CCRS
     CEOS_PROCESSING_COUNTRY=CANADA
     CEOS_SOFTWARE_ID=APP 1.62
     CEOS_ACQUISITION_TIME=19911029162818919
     CEOS_SENSOR_CLOCK_ANGLE=  90.000
     CEOS_ELLIPSOID=IUGG_75
     CEOS_SEMI_MAJOR=    6378.1400000
     CEOS_SEMI_MINOR=    6356.7550000

The SAR_CEOS driver also includes some support for SIR-C and PALSAR
polarimetric data. The SIR-C format contains an image in compressed
scattering matrix form, described
`here <http://southport.jpl.nasa.gov/software/dcomp/dcomp.html>`__. GDAL
decompresses the data as it is read in. The PALSAR format contains bands
that correspond almost exactly to elements of the 3x3 Hermitian
covariance matrix- see the
`ERSDAC-VX-CEOS-004A.pdf <http://www.ersdac.or.jp/palsar/palsar_E.html>`__
document for a complete description (pixel storage is described on page
193). GDAL converts these to complex floating point covariance matrix
bands as they are read in. The convention used to represent the
covariance matrix in terms of the scattering matrix elements HH, HV
(=VH), and VV is indicated below. Note that the non-diagonal elements of
the matrix are complex values, while the diagonal values are real
(though represented as complex bands).

-  Band 1: Covariance_11 (Float32) = HH*conj(HH)
-  Band 2: Covariance_12 (CFloat32) = sqrt(2)*HH*conj(HV)
-  Band 3: Covariance_13 (CFloat32) = HH*conj(VV)
-  Band 4: Covariance_22 (Float32) = 2*HV*conj(HV)
-  Band 5: Covariance_23 (CFloat32) = sqrt(2)*HV*conj(VV)
-  Band 6: Covariance_33 (Float32) = VV*conj(VV)

The identities of the bands are also reflected in the metadata.

NOTE: Implemented as ``gdal/frmts/ceos2/sar_ceosdataset.cpp``.

SDAT -- SAGA GIS Binary Grid File Format
----------------------------------------

(starting with GDAL 1.7.0)

The driver supports both reading and writing (including create, delete,
and copy) SAGA GIS binary grids. SAGA binary grid datasets are made of
an ASCII header (.SGRD) and a binary data (.SDAT) file with a common
basename. The .SDAT file should be selected to access the dataset.
Starting with GDAL 2.3, the driver can read compressed .sg-grd-z files
that are ZIP archives with .sgrd, .sdat and .prj files.

The driver supports reading the following SAGA datatypes (in brackets
the corresponding GDAL types): BIT (GDT_Byte), BYTE_UNSIGNED (GDT_Byte),
BYTE (GDT_Byte), SHORTINT_UNSIGNED (GDT_UInt16), SHORTINT (GDT_Int16),
INTEGER_UNSIGNED (GDT_UInt32), INTEGER (GDT_Int32), FLOAT (GDT_Float32)
and DOUBLE (GDT_Float64).

The driver supports writing the following SAGA datatypes: BYTE_UNSIGNED
(GDT_Byte), SHORTINT_UNSIGNED (GDT_UInt16), SHORTINT (GDT_Int16),
INTEGER_UNSIGNED (GDT_UInt32), INTEGER (GDT_Int32), FLOAT (GDT_Float32)
and DOUBLE (GDT_Float64).

Currently the driver does not support zFactors other than 1 and reading
SAGA grids which are written TOPTOBOTTOM.

NOTE: Implemented as ``gdal/frmts/saga/sagadataset.cpp``.

SDTS -- USGS SDTS DEM
---------------------

GDAL includes support for reading USGS SDTS formatted DEMs. USGS DEMs
are always returned with a data type of signed sixteen bit integer, or
32bit float. Projection and georeferencing information is also returned.

SDTS datasets consist of a number of files. Each DEM should have one
file with a name like XXXCATD.DDF. This should be selected to open the
dataset.

The elevation units of DEMs may be feet or meters. The GetType() method
on a band will attempt to return if the units are Feet ("ft") or Meters
("m").

NOTE: Implemented as ``gdal/frmts/sdts/sdtsdataset.cpp``.

SGI - SGI Image Format
----------------------

The SGI driver currently supports the reading and writing of SGI Image
files.

The driver currently supports 1, 2, 3, and 4 band images. The driver
currently supports "8 bit per channel value" images. The driver supports
both uncompressed and run-length encoded (RLE) images for reading, but
created files are always RLE compressed..

The GDAL SGI Driver was based on Paul Bourke's SGI image read code.

See Also:

-  `Paul Bourke's SGI Image Read
   Code <http://astronomy.swin.edu.au/~pbourke/dataformats/sgirgb/>`__
-  `SGI Image File Format
   Document <ftp://ftp.sgi.com/graphics/SGIIMAGESPEC>`__

NOTE: Implemented as ``gdal/frmts/sgi/sgidataset.cpp``.

SIGDEM -- Scaled Integer Gridded DEM
------------------------------------

(GDAL >= 2.4.0)

The SIGDEM driver supports reading and writing `Scaled Integer Gridded
DEM <https://github.com/revolsys/sigdem>`__ files.

SIGDEM files contain exactly 1 band. The in-memory band data is stored
using GDT_Float64.

SIGDEM prefers use of an EPSG ID inside the file for coordinate systems.
Only if the spatial reference doesn't have an EPSG ID will a .prj file
be written or read.

NOTE: Implemented as ``gdal/frmts/sigdem/sigdemdataset.cpp``.

SNODAS -- Snow Data Assimilation System
---------------------------------------

(GDAL >= 1.9.0)

This is a convenience driver to read Snow Data Assimilation System data.
Those files contain Int16 raw binary data. The file to provide to GDAL
is the .Hdr file.

`Snow Data Assimilation System (SNODAS) Data Products at
NSIDC <http://nsidc.org/data/docs/noaa/g02158_snodas_snow_cover_model/index.html>`__

NOTE: Implemented as ``gdal/frmts/raw/snodasdataset.cpp``.

Standard Product Format (ASRP/USRP) (.gen)
------------------------------------------

(starting with GDAL 1.7.0)

The ASRP and USRP raster products (as defined by DGIWG) are variations
on a common standard product format and are supported for reading by
GDAL. ASRP and USRP datasets are made of several files - typically a
.GEN, .IMG, .SOU and .QAL file with a common basename. The .IMG file
should be selected to access the dataset.

ASRP (in a geographic coordinate system) and USRP (in a UTM/UPS
coordinate system) products are single band images with a palette and
georeferencing.

Starting with GDAL 1.11, the Transmission Header File (.THF) can also be
used as an input to GDAL. If the THF references more than one image,
GDAL will report the images it is composed of as subdatasets. If the THF
references just one image, GDAL will open it directly.

NOTE: Implemented as ``gdal/frmts/adrg/srpdataset.cpp``.

SRTMHGT - SRTM HGT Format
-------------------------

The SRTM HGT driver currently supports the reading of SRTM-3 and SRTM-1
V2 (HGT) files. The files must be named like NXXEYYY.hgt, or starting
with GDAL 2.1.2, NXXEYYY[.something].hgt

Starting with GDAL 2.2, the driver can directly read .hgt.zip files
provided that they are named like NXXEYYY[.something].hgt.zip and
contain a NXXEYYY.hgt file. For previous versions, use
/vsizip//path/to/NXXEYYY[.something].hgt.zip/NXXEYYY.hgt syntax

The driver does support creating new files, but the input data must be
exactly formatted as a SRTM-3 or SRTM-1 cell. That is the size, and
bounds must be appropriate for a cell.

See Also:

-  `SRTM
   documentation <http://dds.cr.usgs.gov/srtm/version2_1/Documentation>`__
-  `SRTM FAQ <http://www2.jpl.nasa.gov/srtm/faq.html>`__
-  `SRTM data <http://dds.cr.usgs.gov/srtm/version2_1/>`__

NOTE: Implemented as ``gdal/frmts/srtmhgt/srtmhgtdataset.cpp``.

WLD -- ESRI World File
----------------------

A world file file is a plain ASCII text file consisting of six values
separated by newlines. The format is:

::

    pixel X size
    rotation about the Y axis (usually 0.0)
    rotation about the X axis (usually 0.0)
    negative pixel Y size
    X coordinate of upper left pixel center
    Y coordinate of upper left pixel center

For example:

::

   60.0000000000
   0.0000000000
   0.0000000000
   -60.0000000000
   440750.0000000000
   3751290.0000000000

You can construct that file simply by using your favorite text editor.

World file usually has suffix .wld, but sometimes it may has .tfw, tifw,
.jgw or other suffixes depending on the image file it comes with.

XPM - X11 Pixmap
----------------

GDAL includes support for reading and writing XPM (X11 Pixmap Format)
image files. These are colormapped one band images primarily used for
simple graphics purposes in X11 applications. It has been incorporated
in GDAL primarily to ease translation of GDAL images into a form usable
with the GTK toolkit.

The XPM support does not support georeferencing (not available from XPM
files) nor does it support XPM files with more than one character per
pixel. New XPM files must be colormapped or greyscale, and colortables
will be reduced to about 70 colors automatically.

NOTE: Implemented as ``gdal/frmts/xpm/xpmdataset.cpp``.

GFF - Sandia National Laboratories GSAT File Format
---------------------------------------------------

This read-only GDAL driver is designed to provide access to processed
data from Sandia National Laboratories' various experimental sensors.
The format is essentially an arbitrary length header containing
instrument configuration and performance parameters along with a binary
matrix of 16- or 32-bit complex or byte real data.

The GFF format was implemented based on the Matlab code provided by
Sandia to read the data. The driver supports all types of data (16-bit
or 32-bit complex, real bytes) theoretically, however due to a lack of
data only 32-bit complex data has been tested.

Sandia provides some sample data at
http://www.sandia.gov/radar/complex-data/.

The extension for GFF formats is .gff.

NOTE: Implemented as ``gdal/frmts/gff/gff_dataset.cpp``.

ZMap -- ZMap Plus Grid
----------------------

(GDAL >= 1.9.0)

Supported for read access and creation. This format is an ASCII
interchange format for gridded data in an ASCII line format for
transport and storage. It is commonly used in applications in the Oil
and Gas Exploration field.

By default, files are interpreted and written according to the
PIXEL_IS_AREA convention. If you define the ZMAP_PIXEL_IS_POINT
configuration option to TRUE, the PIXEL_IS_POINT convention will be
followed to interpret/write the file (the georeferenced values in the
header of the file will then be considered as the coordinate of the
center of the pixels). Note that in that case, GDAL will report the
extent with its usual PIXEL_IS_AREA convention (the coordinates of the
topleft corner as reported by GDAL will be a half-pixel at the top and
left of the values that appear in the file).

Informal specification given in this `GDAL-dev mailing list
thread <http://lists.osgeo.org/pipermail/gdal-dev/2011-June/029173.html>`__

NOTE: Implemented as ``gdal/frmts/zmap/zmapdataset.cpp``.

CAD -- AutoCAD DWG raster layer
-------------------------------

(GDAL >= 2.2.0)

OGR DWG support is based on libopencad, so the list of supported DWG
(DXF) versions can be seen in libopencad documentation. All drawing
entities are separated into layers as they are in DWG file. The rasters
are usually a separate georeferenced files (GeoTiff, Jpeg, Png etc.)
which exist in DWG file as separate layers. The driver try to get
spatial reference and other methadata from DWG Image description and set
it to GDALDataset.

NOTE: Implemented as ``ogr/ogrsf_frmts/cad/gdalcaddataset.cpp``.

--------------

`Full list of GDAL Raster Formats <formats_list.html>`__

$Id$
