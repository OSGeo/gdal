.. _raster.nitf:

================================================================================
NITF -- National Imagery Transmission Format
================================================================================

.. shortname:: NITF

.. built_in_by_default::

.. toctree::
   :maxdepth: 1
   :hidden:

   nitf_advanced

GDAL supports reading of several subtypes of NITF image files, and
writing simple NITF 2.1 files. NITF 1.1, NITF 2.0, NITF 2.1 and NSIF 1.0
files with uncompressed, ARIDPCM, JPEG compressed, JPEG2000 (with
Kakadu, ECW SDKs or other JPEG2000 capable driver) or VQ compressed
images should be readable.

The read support test has been tested on various products, including CIB
and CADRG frames from RPF products, ECRG frames, HRE products.

Color tables for pseudocolored images are read. In some cases nodata
values may be identified.

Lat/Long extents are read from the IGEOLO information in the image
header if available. If high precision lat/long georeferencing
information is available in RPF auxiliary data it will be used in
preference to the low precision IGEOLO information. In case a BLOCKA
instance is found, the higher precision coordinates of BLOCKA are used
if the block data covers the complete image - that is the L_LINES field
with the row count for that block is equal to the row count of the
image. Additionally, all BLOCKA instances are returned as metadata. If
GeoSDE TRE are available, they will be used to provide higher precision
coordinates. If the RPC00B (or RPC00A) TRE is available, it is used to
report RPC metadata. Starting with GDAL 2.2, RPC information can be
retrieved from \_rpc.txt files, and they will be used in priority over
internal RPC00B values, since the latter have less precision than the
ones stored in external \_rpc.txt.

Most file header and image header fields are returned as dataset level
metadata.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

On export NITF files are always written as NITF 2.1 with one image and
no other auxiliary layers. Images are uncompressed by default, but JPEG
and JPEG2000 compression are also available. Georeferencing can only be
written for images using a geographic coordinate system or a UTM WGS84
projection. Coordinates are implicitly treated as WGS84 even if they are
actually in a different geographic coordinate system. Pseudo-color
tables may be written for 8bit images.

In addition to the export oriented CreateCopy() API, it is also possible
to create a blank NITF file using Create() and write imagery on demand.
However, using this methodology writing of pseudocolor tables and
georeferencing is not supported unless appropriate IREP and ICORDS
creation options are supplied.

Creation Options:

-  Most file header, imagery header metadata and security fields can be
   set with appropriate **creation options** (although they are reported
   as metadata item, but must not be set as metadata). For instance
   setting
   `"FTITLE=Image of abandoned missile silo south west of Karsk"` in
   the creation option list would result in setting of the FTITLE field
   in the NITF file header. Use the official field names from the NITF
   specification document; do not put the "NITF\_" prefix that is
   reported when asking the metadata list.
-  **IC=NC/C3/M3/C8** : Set the compression method.

   -  NC is the default value, and means no compression.
   -  C3 means JPEG compression and is only available for the
      CreateCopy() method. The QUALITY and PROGRESSIVE JPEG-specific
      creation options can be used. See the :ref:`raster.jpeg` driver.
      Multi-block images can be written.
   -  M3 is a variation of C3. The only difference is that a block map
      is written, which allow for fast seeking to any block.
   -  C8 means JPEG2000 compression (one block) and is available for
      CreateCopy() and/or Create() methods. JPEG2000 compression is only
      available if the JP2ECW, JP2KAK, JP2OpenJPEG or Jasper driver are
      available (tried in that order when several ones are available)

      -  :ref:`JP2ECW <raster.jp2ecw>`: The TARGET and PROFILE
         JP2ECW-specific creation options can be used. Both CreateCopy()
         and/or Create() methods are available. By default the NPJE
         PROFILE will be used (thus implying
         BLOCKXSIZE=BLOCKYSIZE=1024).
      -  :ref:`JP2KAK <raster.jp2kak>`: The QUALITY, BLOCKXSIZE,
         BLOCKYSIZE, LAYERS, ROI JP2KAK-specific creation options can be
         used. Only CreateCopy() method is available.
      -  :ref:`JP2OpenJPEG <raster.jp2openjpeg>`:
         (only in the CreateCopy() case). The QUALITY, BLOCKXSIZE
         and BLOCKYSIZE JP2OpenJPEG-specific creation options can be
         used. By default BLOCKXSIZE=BLOCKYSIZE=1024 will be used.
      -  Jasper JPEG2000 driver: only in the CreateCopy() case.

-  **NUMI=n** : Number of images. Default =
   1. This option is only compatible with IC=NC (uncompressed images).
-  **ICORDS=G/D/N/S**: Set to "G" to ensure that space will be reserved
   for geographic corner coordinates (in DMS) to be set later via
   SetGeoTransform(), set to "D" for geographic coordinates in decimal
   degrees, set to "N" for UTM WGS84 projection in Northern hemisphere
   or to "S" for UTM WGS84 projection in southern hemisphere (Only
   needed for Create() method, not CreateCopy()). If you Create() a new
   NITF file and have specified "N" or "S" for ICORDS, you need to call
   later the SetProjection method with a consistent UTM SRS to set the
   UTM zone number (otherwise it will default to zone 0).
-  **FHDR**: File version can be selected though currently the only two
   variations supported are "NITF02.10" (the default), and "NSIF01.00".
-  **IREP**: Set to "RGB/LUT" to reserve space for a color table for
   each output band. (Only needed for Create() method, not
   CreateCopy()).
-  **IREPBAND**: Comma separated list of band IREPBANDs
   in band order.
-  **ISUBCAT**: Comma separated list of band ISUBCATs in
   band order.
-  **LUT_SIZE**: Set to control the size of pseudocolor tables for
   RGB/LUT bands. A value of 256 assumed if not present. (Only needed
   for Create() method, not CreateCopy()).
-  **BLOCKXSIZE=n**: Set the block width.
-  **BLOCKYSIZE=n**: Set the block height.
-  **BLOCKA_*=**: If a complete set of BLOCKA options is provided with
   exactly the same organization as the NITF_BLOCKA metadata reported
   when reading an NITF file with BLOCKA TREs then a file will be
   created with BLOCKA TREs.
-  **TRE=tre-name=tre-contents**: One or more TRE creation options may
   be used provided to write arbitrary user defined TREs to the image
   header. The tre-name should be at most six characters, and the
   tre-contents should be "backslash escaped" if it contains backslashes
   or zero bytes. The argument is the same format as returned in the TRE
   metadata domain when reading.
-  **FILE_TRE=tre-name=tre-contents**: Similar to above
   options, except that the TREs are written in the file header, instead
   of the image header.
-  **SDE_TRE=YES/NO**: Write GEOLOB and GEOPSB TREs to
   get more precise georeferencing. This is limited to geographic SRS,
   and to CreateCopy() for now.
-  **RPC00B=YES/NO**: (GDAL >= 2.2.0) Write RPC00B TRE, from a source
   RPC00B TRE if it exists (NITF to NITF conversion), or from values
   found in the RPC metadata domain. This is only taken into account by
   CreateCopy() for now. Note that the NITF RPC00B format uses limited
   prevision ASCII encoded numbers. Default to YES.
-  **RPCTXT=YES/NO**: (GDAL >= 2.2.0) Whether to write RPC metadata in a
   external \_rpc.txt file. This may be useful since internal RPC00B TRE
   have limited precision. This is only taken into account by
   CreateCopy() for now. Default to NO.
-  **USE_SRC_NITF_METADATA=YES/NO**: (GDAL >= 2.3.0) Whether to use
   NITF_xxx metadata items and TRE segments from the input dataset. It
   may needed to set this option to NO if changing the georeferencing of
   the input file. Default to YES.

Links
-----

-  :ref:`Advanced GDAL NITF Driver Information <raster.nitf_advanced>`
-  `NITFS Technical Board Public Page <http://www.gwg.nga.mil/ntb/>`__
-  `DIGEST Part 2 Annex D (describe encoding of NITF Spatial Data
   Extensions) <http://www.gwg.nga.mil/ntb/baseline/docs/digest/part2_annex_d.pdf>`__
-  :ref:`raster.rpftoc`: to read the Table Of  Contents of CIB and CADRG products.
-  `MIL-PRF-89038 <http://www.everyspec.com/MIL-PRF/MIL-PRF+%28080000+-+99999%29/MIL-PRF-89038_25371/>`__
   : specification of RPF, CADRG, CIB products
-  :ref:`raster.ecrgtoc`: to read the Table Of Contents of ECRG products.
-  `MIL-PRF-32283 <http://www.everyspec.com/MIL-PRF/MIL-PRF+%28030000+-+79999%29/MIL-PRF-32283_26022/>`__
   : specification of ECRG products

Credit
------

The author wishes to thank `AUG Signals <http://www.augsignals.com/>`__
and the `GeoConnections <http://geoconnections.org/>`__ program for
supporting development of this driver, and to thank Steve Rawlinson
(JPEG), Reiner Beck (BLOCKA) for assistance adding features.
