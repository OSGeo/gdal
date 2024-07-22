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

GDAL supports reading of several subtypes of NITF (National Imagery Transmission Format)
image files, and writing simple NITF 2.1 files. NITF 1.1, NITF 2.0, NITF 2.1 and NSIF 1.0
files with uncompressed, ARIDPCM (Adaptive Recursive Interpolated Differential Pulse Code Modulation),
JPEG compressed, JPEG2000 (with Kakadu, ECW SDKs or other JPEG2000 capable driver)
or VQ (Vector Quantized) compressed images should be readable.

The read support test has been tested on various products, including
CIB (Controlled Image Base) and CADRG (Compressed ARC Digitized Raster Graphics)
frames from RPF (Raster Product Format) products, ECRG (Enhanced Compressed
Raster Graphics) frames, HRE (High Resolution Elevation) products.

Color tables for pseudocolored images are read. In some cases nodata
values may be identified.

Lat/Long extents are read from the IGEOLO (Image GeoLocation) information in the image
header if available. If high precision lat/long georeferencing
information is available in RPF auxiliary data it will be used in
preference to the low precision IGEOLO information.
In case a BLOCKA (Image Block Information)
instance is found, the higher precision coordinates of BLOCKA are used
if the block data covers the complete image - that is the L_LINES field
with the row count for that block is equal to the row count of the
image. Additionally, all BLOCKA instances are returned as metadata. If
GeoSDE TRE (Tagged Record Extension) are available, they will be used to provide higher precision
coordinates. If the RPC00B (or RPC00A) TRE is available, it is used to
report RPC (Rapid Positioning Capability / Rational Polynomial Coefficients) metadata.
Starting with GDAL 2.2, RPC information can be
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

Open options
------------

.. versionadded:: 3.7

- .. oo:: VALIDATE
     :choices: YES, NO
     :default: NO

     Whether TRE and DES (Data Extension Segment) content should be validated upon
     dataset opening. If errors are found, CE_Failure errors are emitted, but
     dataset opening does not fail, unless the FAIL_IF_VALIDATION_ERROR=YES
     open option is set.
     Note that validation is partial, and limited to the constraints documented in
     the nitf_spec.xml configuration file.
     Details of errors are also logged in ``<error>`` elements in the ``xml:TRE``
     and ``xml:DES`` metadata domains.

- .. oo:: FAIL_IF_VALIDATION_ERROR
     :choices: YES, NO
     :default: NO

     Whether validation errors reported by
     the :oo:`VALIDATE=YES` open option should prevent the dataset from being opened.

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
georeferencing is not supported unless appropriate IREP (Image Representation) and
ICORDS (Image Coordinate) creation options are supplied.

Most file header, imagery header metadata and security fields can be
set with appropriate **creation options** (although they are reported
as metadata item, but must not be set as metadata). For instance
setting
`"FTITLE=Image of abandoned missile silo south west of Karsk"` in
the creation option list would result in setting of the FTITLE field
in the NITF file header. Use the official field names from the NITF
specification document; do not put the "NITF\_" prefix that is
reported when asking the metadata list.

|about-creation-options|
The following creation options are available:

-  .. co:: IC
      :choices: NC, C3, M3, C8
      :default: NC

      Set the compression method.

      -  NC is the default value, and means no compression.
      -  C3 means JPEG compression and is only available for the
         CreateCopy() method. The QUALITY and PROGRESSIVE JPEG-specific
         creation options can be used. See the :ref:`raster.jpeg` driver.
         Multi-block images can be written.
      -  M3 is a variation of C3. The only difference is that a block map
         is written, which allow for fast seeking to any block.
      -  C8 means JPEG2000 compression (one block) and is available for
         CreateCopy() and/or Create() methods. See below paragraph for specificities.

-  .. co:: QUALITY
      :choices: 10-100
      :default: 75

      JPEG quality 10-100

-  .. co:: PROGRESSIVE
      :choices: YES, NO
      :default: NO

      JPEG progressive mode

-  .. co:: RESTART_INTERVAL
      :choices: -1, 0, >0
      :default: -1

      Restart interval (in MCUs) for JPEG compressoin.
      -1 for auto, 0 for none, > 0 for user specified.

-  .. co:: NUMI
      :default: 1

      Number of images.
      See :ref:`raster.nitf_advanced_write_multiple_image_segments` for
      the procedure to follow to write several images in a NITF file.

-  .. co:: WRITE_ONLY_FIRST_IMAGE
      :choices: YES, NO
      :default: NO

      (Only taken into account if NUMI > 1, and on a new NITF file).
      If YES, only write first image. Subsequent ones must be written with APPEND_SUBDATASET=YES

-  .. co:: WRITE_ALL_IMAGES
      :choices: YES, NO
      :default: NO
      :since: 3.4

      (Only taken into account if NUMI > 1, and on a new NITF file).
      When set to NO (the default), this causes the driver to only write the first
      image segment and reserve just the space for extra NUMI-1 images in the file header.
      When WRITE_ALL_IMAGES=YES, the space for all images is allocated, which is
      only compatible with IC=NC (uncompressed images).
      (Behavior with GDAL < 3.4 was similar to WRITE_ALL_IMAGES=YES)

-  .. co:: ICORDS
      :choices: G, D, N, S

      Set to "G" to ensure that space will be reserved
      for geographic corner coordinates (in DMS) to be set later via
      SetGeoTransform(), set to "D" for geographic coordinates in decimal
      degrees, set to "N" for UTM WGS84 projection in Northern hemisphere
      or to "S" for UTM WGS84 projection in southern hemisphere (Only
      needed for Create() method, not CreateCopy()). If you Create() a new
      NITF file and have specified "N" or "S" for ICORDS, you need to call
      later the SetProjection method with a consistent UTM SRS to set the
      UTM zone number (otherwise it will default to zone 0).
      Starting with GDAL 3.5.1, when using the CreateCopy() interface with an
      image whose source SRS is a UTM WGS84 projection and specifying ICORDS=G or D,
      the NITF driver will reproject the image corner coordinates to longitude-latitude.
      This can be useful when it is not possible to encode in the IGEOLO field
      the coordinates of an image in the equatorial zone, whose one of the northing
      expressed in a UTM northern hemisphere projection is below -1e6.

-  .. co:: IGEOLO
      :since: 3.5.1

      Image corner coordinates specified as a
      string of 60 characters (cf MIL-STD-2500C for expected format). Normally
      automatically set from source geotransform and SRS when using the CreateCopy()
      interface. If specified, ICORDS must also be specified.

-  .. co:: FHDR
      :choices: NITF02.10, NSIF01.00
      :default: NITF02.10

      File version can be selected though currently the only two
      variations supported are "NITF02.10" (the default), and "NSIF01.00".

-  .. co:: IREP

      Set to "RGB/LUT" (Look Up Table) to reserve space for a color table for
      each output band. (Only needed for Create() method, not
      CreateCopy()).

-  .. co:: IREPBAND

      Comma separated list of band IREPBANDs in band order.

-  .. co:: ISUBCAT

      Comma separated list of band ISUBCATs in band order.

-  .. co:: LUT_SIZE
      :default: 256

      Set to control the size of pseudocolor tables for
      RGB/LUT bands. (Only needed
      for Create() method, not CreateCopy()).

-  .. co:: BLOCKXSIZE

      Set the block width.

-  .. co:: BLOCKYSIZE

      Set the block height.

-  .. co:: BLOCKSIZE

      Set the block with and height. Overridden by BLOCKXSIZE and BLOCKYSIZE

-  .. co:: BLOCKA_*

      If a complete set of BLOCKA options is provided with
      exactly the same organization as the NITF_BLOCKA metadata reported
      when reading an NITF file with BLOCKA TREs then a file will be
      created with BLOCKA TREs.

-  .. co:: TRE
      :choices: <tre-name=tre-contents>

      One or more TRE (Tagged Record Extension) creation options may
      be used provided to write arbitrary user defined TREs to the image
      header. The tre-name should be at most six characters, and the
      tre-contents should be "backslash escaped" if it contains backslashes
      or zero bytes. The argument is the same format as returned in the TRE
      metadata domain when reading.

-  .. co:: FILE_TRE
      :choices: <tre-name=tre-contents>

      Similar to above
      options, except that the TREs are written in the file header, instead
      of the image header.

-  .. co:: RESERVE_SPACE_FOR_TRE_OVERFLOW
      :choices: YES, NO
      :since: 3.6

      Set to true to reserve space for IXSOFL when writing a TRE_OVERFLOW DES.

-  .. co:: DES
      :choices: <des-name=des-contents>

      One or more DES (Data Extension Segment) creation options may
      be provided to write arbitrary user defined DESs to the NITF file.
      The des-name should be at most 25 characters, and the des-contents
      should be "backslash escaped" if it contains backslashes or zero
      bytes, as in CPLEscapeString(str, -1, CPLES_BackslashQuotable).
      The des-contents must contain standard DES fields, starting
      with DESVER (See MIL-STD-2500C).  DESs are not currently copied in
      CreateCopy(), but may be explicitly added as with Create().

-  .. co:: NUMDES
      :since: 3.4

      Number of DES segments. Only to be used on
      first image segment

-  .. co:: TEXT

      TEXT options as text-option-name=text-option-content. Cf :ref:`raster.nitf_advanced_text`.

-  .. co:: CGM

      CGM options in cgm-option-name=cgm-option-content.  Cf :ref:`raster.nitf_advanced_cgm`.

-  .. co:: SDE_TRE
      :choices: YES, NO

      Write GEOLOB (Local Geographic (lat/long) Coordinate System) and
      GEOPSB (Geo positioning Information) TREs to
      get more precise georeferencing. This is limited to geographic SRS,
      and to CreateCopy() for now.

-  .. co:: RPC00B
      :choices: YES, NO
      :default: YES
      :since: 2.2.0

      Write RPC00B TRE, from a source
      RPC00B TRE if it exists (NITF to NITF conversion), or from values
      found in the RPC metadata domain. This is only taken into account by
      CreateCopy() for now. Note that the NITF RPC00B format uses limited
      prevision ASCII encoded numbers.

-  .. co:: RPCTXT
      :choices: YES, NO
      :default: NO
      :since: 2.2.0

      Whether to write RPC metadata in a
      external \_rpc.txt file. This may be useful since internal RPC00B TRE
      have limited precision. This is only taken into account by
      CreateCopy() for now.

-  .. co:: USE_SRC_NITF_METADATA
      :choices: YES, NO
      :default: YES
      :since: 2.3.0

      Whether to use
      NITF_xxx metadata items and TRE segments from the input dataset. It
      may needed to set this option to NO if changing the georeferencing of
      the input file.


The following creation options to se fields in the NITF file header are available:

-  .. co:: OSTAID
      :choices: string of up to 10 characters

      Originating Station ID

-  .. co:: FDT
      :choices: string of up to 14 characters

      File Date and Time

-  .. co:: FTITLE
      :choices: string of up to 80 characters

      File Title

-  .. co:: FSCLAS
      :choices: string of 1 character

      File Security Classification

-  .. co:: FSCLSY
      :choices: string of up to 2 characters

      File Classification Security System

-  .. co:: FSCODE
      :choices: string of up to 11 characters

      File Codewords

-  .. co:: FSCTLH
      :choices: string of up to 2 characters

      File Control and Handling

-  .. co:: FSREL
      :choices: string of up to 20 characters

      File Releasing Instructions

-  .. co:: FSDCTP
      :choices: string of up to 2 characters

      File Declassification Type

-  .. co:: FSDCDT
      :choices: string of 8 characters

      File Declassification Date

-  .. co:: FSDCXM
      :choices: string of up to 4 characters

      File Declassification Exemption

-  .. co:: FSDG
      :choices: string of 1 character

      File Downgrade

-  .. co:: FSDGDT
      :choices: string of 8 characters

      File Downgrade Date

-  .. co:: FSCLTX
      :choices: string of up to 43 characters

      File Classification Text

-  .. co:: FSCATP
      :choices: string of 1 character

      File Classification Authority Type

-  .. co:: FSCAUT
      :choices: string of up to 40 characters

      File Classification Authority

-  .. co:: FSCRSN
      :choices: string of 1 character

      File Classification Reason

-  .. co:: FSSRDT
      :choices: string of 8 characters

      File Security Source Date

-  .. co:: FSCTLN
      :choices: string of up to 15 characters

      File Security Control Number

-  .. co:: FSCOP
      :choices: string of up to 5 characters

      File Copy Number

-  .. co:: FSCPYS
      :choices: string of up to 5 characters

      File Number of Copies

-  .. co:: ONAME
      :choices: string of up to 24 characters

      Originator Name

-  .. co:: OPHONE
      :choices: string of up to 18 characters

      Originator Phone Number

The following creation options to se fields in the NITF image header are available:

-  .. co:: IID1
      :choices: string of up to 10 characters

      Image Identifier 1

-  .. co:: IDATIM
      :choices: string of 14 characters

      Image Date and Time

-  .. co:: TGTID
      :choices: string of up to 17 characters

      Target Identifier

-  .. co:: IID2
      :choices: string of up to 80 characters

      Image Identifier 2

-  .. co:: ISCLAS
      :choices: string of 1 character

      Image Security Classification

-  .. co:: ISCLSY
      :choices: string of up to 2 characters

      Image Classification Security System

-  .. co:: ISCODE
      :choices: string of up to 11 characters

      Image Codewords

-  .. co:: ISCTLH
      :choices: string of up to 2 characters

      Image Control and Handling

-  .. co:: ISREL
      :choices: string of up to 20 characters

      Image Releasing Instructions

-  .. co:: ISDCTP
      :choices: string of up to 2 characters

      Image Declassification Type

-  .. co:: ISDCDT
      :choices: string of 8 characters

      Image Declassification Date

-  .. co:: ISDCXM
      :choices: string of up to 4 characters

      Image Declassification Exemption

-  .. co:: ISDG
      :choices: string of 1 character

      Image Downgrade

-  .. co:: ISDGDT
      :choices: string of 8 characters

      Image Downgrade Date

-  .. co:: ISCLTX
      :choices: string of up to 43 characters

      Image Classification Text

-  .. co:: ISCATP
      :choices: string of 1 character

      Image Classification Authority Type

-  .. co:: ISCAUT
      :choices: string of up to 40 characters

      Image Classification Authority

-  .. co:: ISCRSN
      :choices: string of 1 character

      Image Classification Reason

-  .. co:: ISSRDT
      :choices: string of 8 characters

      Image Security Source Date

-  .. co:: ISCTLN
      :choices: string of up to 15 characters

      Image Security Control Number

-  .. co:: ISORCE
      :choices: string of up to 42 characters

      Image Source

-  .. co:: ICAT
      :choices: string of up to 8 characters

      Image Category

-  .. co:: ABPP
      :choices: integer

      Actual Bits-Per-Pixel Per Band.
      Starting with GDAL 3.10, also available as the ``NBITS`` creation option.

-  .. co:: PJUST
      :choices: string of 1 character

      Pixel Justification

-  .. co:: ICOM
      :choices: string of up to 720 characters

      Image Comments (organized as up to 9 lines of 80 characters)

-  .. co:: IDLVL
      :choices: integer of up to 3 characters

      Image Display Level

-  .. co:: IALVL
      :choices: integer of up to 3 characters

      Image Attachment Level

-  .. co:: ILOCROW
      :choices: integer of up to 5 characters

      Image Location Row

-  .. co:: ILOCCOL
      :choices: integer of up to 5 characters

      Image Location Column


JPEG2000 compression (write support)
------------------------------------

JPEG2000 compression is available when using the IC=C8 creation option,
if the JP2ECW (SDK 3.3, or for later versions assuming the user has the key to
enable JPEG2000 writing), JP2KAK or JP2OpenJPEG driver are available.

They are tried in that order when several ones are available, unless the
JPEG2000_DRIVER creation option (added in GDAL 3.4) is set to explicitly specify
the JPEG2000 capable driver to use.

- :ref:`JP2ECW <raster.jp2ecw>`: The :co:`drivers/raster/jp2ecw TARGET` (target size reduction as a
  percentage of the original) and :co:`drivers/raster/jp2ecw PROFILE`\ =BASELINE_0/BASELINE_1/BASELINE_2/NPJE/EPJE
  JP2ECW-specific creation options can be used. Both CreateCopy()
  and/or Create() methods are available. By default the NPJE
  PROFILE will be used (thus implying BLOCKXSIZE=BLOCKYSIZE=1024).

- :ref:`JP2KAK <raster.jp2kak>`: The
  :co:`drivers/raster/jp2kak QUALITY`,
  :co:`drivers/raster/jp2kak BLOCKXSIZE`,
  :co:`drivers/raster/jp2kak BLOCKYSIZE`,
  :co:`drivers/raster/jp2kak LAYERS`,
  :co:`drivers/raster/jp2kak ROI` JP2KAK-specific creation options can be
  used. Only CreateCopy() method is available.

- :ref:`JP2OpenJPEG <raster.jp2openjpeg>`:
  (only in the CreateCopy() case). The
  :co:`drivers/raster/jp2openjpeg QUALITY`,
  :co:`drivers/raster/jp2openjpeg BLOCKXSIZE`
  and
  :co:`drivers/raster/jp2openjpeg BLOCKYSIZE`,
  :co:`drivers/raster/jp2ecw TARGET` (target size reduction as a
  percentage of the original) JP2OpenJPEG-specific creation options can be
  used. By default BLOCKXSIZE=BLOCKYSIZE=1024 will be used.

  Starting with GDAL 3.4.0 and OpenJPEG 2.5, the
  PROFILE=NPJE_VISUALLY_LOSSLESS/NPJE_NUMERICALLY_LOSSLESS
  creation option can be used to create files that comply with
  `STDI-0006 NITF Version 2.1 Commercial Dataset Requirements Document (NCDRD) <https://gwg.nga.mil/ntb/baseline/docs/stdi0006/STDI-0006-NCDRD-16Feb06.doc>`__.
  For NPJE_VISUALLY_LOSSLESS, the last quality layer defaults to 3.9 bits per
  pixel and per band. It can be adjusted with the QUALITY creation option.
  When those profiles are specified, the J2KLRA TRE will also be written, unless
  the ``J2KLRA=NO`` creation option is specified.

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
