.. _raster.nitf:

================================================================================
NITF -- National Imagery Transmission Format (also CIB, CADRG, ECRG, HRE)
================================================================================

.. shortname:: NITF

.. built_in_by_default::

.. toctree::
   :maxdepth: 1
   :hidden:

   nitf_advanced

GDAL supports reading of several subtypes of NITF (National Imagery Transmission Format)
image files, and writing NITF 2.1 files, and limited writing support for NITF 2.0.
NITF 1.1, NITF 2.0, NITF 2.1 and NSIF 1.0
files with uncompressed, ARIDPCM (Adaptive Recursive Interpolated Differential Pulse Code Modulation),
JPEG compressed, JPEG2000 (with Kakadu, ECW SDKs or other JPEG2000 capable driver)
or VQ (Vector Quantized) compressed images should be readable.

The read support test has been tested on various products, including
CIB (Controlled Image Base) and CADRG (Compressed ARC Digitized Raster Graphics)
frames from RPF (Raster Product Format) products, ECRG (Enhanced Compressed
Raster Graphics) frames, HRE (High Resolution Elevation) products.
Write support for CADRG is available since GDAL 3.13.

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
RPC information can be retrieved from \_rpc.txt files, and they will be used in priority over
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
      :choices: NC, C3, C4, M3, C8
      :default: NC

      Set the compression method.

      -  NC is the default value, and means no compression.
      -  C3 means JPEG compression and is only available for the
         CreateCopy() method. The QUALITY and PROGRESSIVE JPEG-specific
         creation options can be used. See the :ref:`raster.jpeg` driver.
         Multi-block images can be written.
      -  M3 is a variation of C3. The only difference is that a block map
         is written, which allow for fast seeking to any block.
      -  C4 means Vector Quantized (VQ) compression, and is only available when
         PRODUCT_TYPE=CADRG.
      -  C8 means JPEG2000 compression (one block) and is available for
         CreateCopy() and/or Create() methods. See below paragraph for specificities.

-  .. co:: QUALITY
      :default: 75

      For JPEG, quality as integer values in the 10-100 range
      For JPEG2000, quality as a floating-point value in >0 - 100 range.
      When JPEG2000_DRIVER=JP2OpenJPEG and PROFILE is not one of the NPJE ones,
      several quality layers can be specified as a comma-separated list of values.

-  .. co:: PROGRESSIVE
      :choices: YES, NO
      :default: NO

      JPEG progressive mode

-  .. co:: RESTART_INTERVAL
      :choices: -1, 0, >0
      :default: -1

      Restart interval (in MCUs) for JPEG compression.
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
      :choices: NITF02.10, NSIF01.00, NITF02.00
      :default: NITF02.10

      File version can be selected. "NITF02.10" (the default), and "NSIF01.00"
      are fully supported. Support for NITF02.00 has been introduced in GDAL 3.13.
      Note that for NITF02.00 writing UTM coordinates is not supported.

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

-  .. co:: PRODUCT_TYPE
      :choices: REGULAR, CADRG
      :default: REGULAR
      :since: 3.13

      Sub-specification the output dataset should respect


The following options are only valid for PRODUCT_TYPE=CADRG.

-  .. co:: COLOR_QUANTIZATION_BITS
      :choices: 5, 6, 7, 8
      :default: 5
      :since: 3.13

      Number of bits per R,G,B color component used during color palette
      computation. The higher the better quality and slower computation time.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: COLOR_TABLE_PER_FRAME
      :choices: YES, NO
      :default: NO
      :since: 3.13

      Whether the color table should be optimized on the whole input dataset,
      or per output frame. The default is NO, that is optimized on the whole
      input dataset, to reduce the risk of color seams across frames.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: SCALE
      :since: 3.13

      Reciprocal scale to use when generating output frames.
      Valid values are in the range from 1000 (1:1K) to 20000000 (1:20M).
      Special value ``GUESS`` can be also used to infer the scale from the DPI,
      either explicitly specified with the DPI creation option, or if the
      TIFFTAG_YRESOLUTION / TIFFTAG_RESOLUTIONUNIT metadata items exist on the
      source raster.
      When not specified, the scale is inferred from the SERIES_CODE value from
      data series that have a fixed scale. Otherwise it is required.

      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: DPI
      :choices: <float>
      :since: 3.13

      Dot-Per-Inch value for the input dataset, that may need to be specified
      together with SCALE=GUESS. Valid values are in the range from 1 to 7200.
      If SCALE is not specified to the GUESS value, DPI is ignored.

      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: ZONE
      :choices: <string>
      :since: 3.13

      ARC Zone to which restrict generation of CADRG frames (1 to 9, A to H, J).
      If not specified, the driver automatically determines which zones the
      extent of the source dataset intersects.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: SERIES_CODE
      :choices: GN,JN,ON,TP,LF,JG,JA,JR,TF,AT,TC,TL,HA,CO,OA,CG,CM,MM
      :default: MM
      :since: 3.13

      Two-letter code specifying the map/chart type.
      Used to infer the scale, when not specified, for some of the values where the map/chart type
      has a single nominal scale, and for the first 2 letters of the extension
      of frame files.
      Only used when PRODUCT_TYPE=CADRG.

      Below the codes from the initial version of MIL-STD-2411-1.
      Full table available in `MIL-STD-2411/1 (W/CHANGE 3) <https://everyspec.com/MIL-STD/MIL-STD-2000-2999/MIL-STD-2411_1_CHG-3_26002/>`__, pages 9-13.

      +------+----------------------------------------+--------------+
      | Code | Data series                            | Scale        |
      +======+========================================+==============+
      | GN   | Global Navigation Chart (GNC)          | 1:5 million  |
      +------+----------------------------------------+--------------+
      | JN   | Jet Navigation Chart (JNC)             | 1:2 million  |
      +------+----------------------------------------+--------------+
      | ON   | Operational Navigation Chart (ONC)     | 1:1 million  |
      +------+----------------------------------------+--------------+
      | TP   | Tactical Pilotage Chart (TPC)          | 1:500 K      |
      +------+----------------------------------------+--------------+
      | LF   | Low Flying Chart (LFC) - UK            | 1:500 K      |
      +------+----------------------------------------+--------------+
      | JG   | Joint Operation Graphic (JOG)          | 1:250 K      |
      +------+----------------------------------------+--------------+
      | JA   | Joint Operation Graphic, Air (JOG-A)   | 1:250 K      |
      +------+----------------------------------------+--------------+
      | JR   | Joint Operation Graphic, Radar (JOG-R) | 1:250 K      |
      +------+----------------------------------------+--------------+
      | TF   | Transit Flying Chart (TFC) - UK        | 1:250 K      |
      +------+----------------------------------------+--------------+
      | AT   | Series 200 Air Target Chart (ATC)      | 1:200 K      |
      +------+----------------------------------------+--------------+
      | TC   | Topographic Line Map 100 (TLM 100)     | 1:100 K      |
      +------+----------------------------------------+--------------+
      | TL   | Topographic Line Map 50 (TLM 50)       | 1:50 K       |
      +------+----------------------------------------+--------------+
      | HA   | Harbor and Approach Charts  (HAC)      | Various      |
      +------+----------------------------------------+--------------+
      | CO   | Coastal Charts                         | Various      |
      +------+----------------------------------------+--------------+
      | OP   | Naval Range Operating Area Chart       | Various      |
      +------+----------------------------------------+--------------+
      | CG   | City Graphics                          | Various      |
      +------+----------------------------------------+--------------+
      | CM   | Combat Charts                          | Various      |
      +------+----------------------------------------+--------------+
      | MM   | Miscellaneous Maps and Charts          | Various      |
      +------+----------------------------------------+--------------+

-  .. co:: VERSION_NUMBER
      :choices: <string>
      :default: 01
      :since: 3.13

      Two letter version number (using letters among 0-9, A-H and J).
      Used for the 6th and 7th letters of the file name of frame files.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: PRODUCER_CODE_ID
      :choices: <string>
      :default: 0
      :since: 3.13

      One letter code indicating the data producer. This is a base-34 encoded
      value with valid letters '0' to '9', 'A' to 'Z' (excluding 'I' and 'O').
      Used for the last letter of the file name of frame files.
      Only used when PRODUCT_TYPE=CADRG.

      +----+--------------------------+--------------------------------------------------------------+
      | ID | Producer code            | Producer                                                     |
      +====+==========================+==============================================================+
      | 1  | AFACC                    | Air Force Air Combat Command                                 |
      +----+--------------------------+--------------------------------------------------------------+
      | 2  | AFESC                    | Air Force Electronic Systems Center                          |
      +----+--------------------------+--------------------------------------------------------------+
      | 3  | NIMA                     | National Imagery and Mapping Agency, Primary                 |
      +----+--------------------------+--------------------------------------------------------------+
      | 4  | NIMA1                    | NIMA, Alternate Site 1                                       |
      +----+--------------------------+--------------------------------------------------------------+
      | 5  | NIMA2                    | NIMA, Alternate Site 2                                       |
      +----+--------------------------+--------------------------------------------------------------+
      | 6  | NIMA3                    | NIMA, Alternate Site 3                                       |
      +----+--------------------------+--------------------------------------------------------------+
      | 7  | SOCAF                    | Air Force Special Operations Command                         |
      +----+--------------------------+--------------------------------------------------------------+
      | 8  | SOCOM                    | United States Special Operations Command                     |
      +----+--------------------------+--------------------------------------------------------------+
      | 9  | PACAF                    | Pacific Air Forces                                           |
      +----+--------------------------+--------------------------------------------------------------+
      | A  | USAFE                    | United States Air Force, Europe                              |
      +----+--------------------------+--------------------------------------------------------------+
      | B  | Non-DoD (NonDD)          | US producer outside the Department of Defense                |
      +----+--------------------------+--------------------------------------------------------------+
      | C  | Non-US (NonUS)           | Non-US producer                                              |
      +----+--------------------------+--------------------------------------------------------------+
      | D  | NIMA DCHUM (DCHUM)       | NIMA produced Digital CHUM file                              |
      +----+--------------------------+--------------------------------------------------------------+
      | E  | Non-NIMA DCHUM (DCHMD)   | DoD producer of Digital CHUM file other than NIMA            |
      +----+--------------------------+--------------------------------------------------------------+
      | F  | Non-US DCHUM (DCHMF)     | Non-US (foreign) producer of Digital CHUM files              |
      +----+--------------------------+--------------------------------------------------------------+
      | G  | Non-DoD DCHUM (DCHMG)    | US producer of Digital CHUM files outside DoD                |
      +----+--------------------------+--------------------------------------------------------------+
      | H  | IMG2RPF                  | Non-specified, Imagery formatted to RPF                      |
      +----+--------------------------+--------------------------------------------------------------+
      | I–Z| Reserved                 | Reserved for future standardization                          |
      +----+--------------------------+--------------------------------------------------------------+

-  .. co:: SECURITY_COUNTRY_CODE
      :choices: <string>
      :since: 3.13

      Two letter country ISO code of the security classification.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: CURRENCY_DATE
      :choices: <string>
      :since: 3.13

      Date of the most recent revision to the RPF product, as YYYYMMDD.
      Can be set to empty to avoid writing it, or the special value NOW for the
      current date, otherwise a default value of 20260101 is used.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: PRODUCTION_DATE
      :choices: <string>
      :since: 3.13

      Date that the source data was transferred to RPF format, as YYYYMMDD.
      Can be set to empty to avoid writing it, or the special value NOW for the
      current date, otherwise a default value of 20260101 is used.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: SIGNIFICANT_DATE
      :choices: <string>
      :since: 3.13

      Date describing the basic date of the source product, as YYYYMMDD.
      Can be set to empty to avoid writing it, or the special value NOW for the
      current date, otherwise a default value of 20260101 is used.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: DATA_SERIES_DESIGNATION
      :choices: <string>
      :since: 3.13

      Short title for the identification of a group of products usually having
      the same scale and/or cartographic specification (e.g. JOG 1501A).
      Up to 10 characters. Derived from SERIES_CODE and SCALE when not specified.
      Only used when PRODUCT_TYPE=CADRG.

-  .. co:: MAP_DESIGNATION
      :choices: <string>
      :since: 3.13

      Designation, within the data series, of the hard-copy source (e.g. G18 if
      the hard-copy source is ONC G18).
      Up to 8 characters.
      Only used when PRODUCT_TYPE=CADRG.


The following creation options to set fields in the NITF file header are available:

-  .. co:: OSTAID
      :choices: string of up to 10 characters

      Originating Station ID

-  .. co:: FDT
      :choices: string of up to 14 characters

      File Date and Time.
      Format is DDHHMMSSZMONYY for NITF 2.0 and CCYYMMDDhhmmss for NITF 2.1.
      ``NOW`` is also accepted as a special value for the current date-time.

-  .. co:: FTITLE
      :choices: string of up to 80 characters

      File Title

-  .. co:: FSCLAS
      :choices: string of 1 character

      File Security Classification (U/R/C/S/T)

-  .. co:: FSCLSY
      :choices: string of up to 2 characters

      File Classification Security System (NITF02.10/NSIF only)

-  .. co:: FSCODE
      :choices: string of up to 11 characters

      File Codewords (NITF02.10/NSIF only)

-  .. co:: FSCTLH
      :choices: string of up to 2 characters

      File Control and Handling

-  .. co:: FSREL
      :choices: string of up to 20 characters

      File Releasing Instructions

-  .. co:: FSDCTP
      :choices: string of up to 2 characters

      File Declassification Type (NITF02.10/NSIF only)

-  .. co:: FSDCDT
      :choices: string of 8 characters

      File Declassification Date (NITF02.10/NSIF only)

-  .. co:: FSDCXM
      :choices: string of up to 4 characters

      File Declassification Exemption (NITF02.10/NSIF only)

-  .. co:: FSDG
      :choices: string of 1 character

      File Downgrade (NITF02.10/NSIF only)

-  .. co:: FSDGDT
      :choices: string of 8 characters

      File Downgrade Date (NITF02.10/NSIF only)

-  .. co:: FSDWNG
      :choices: string of up to 6 characters

      File Security Downgrade (NITF02.00 only)

-  .. co:: FSDEVT
      :choices: string of 40 characters

      File Downgrading Event (NITF02.00 only)

-  .. co:: FSCLTX
      :choices: string of up to 43 characters

      File Classification Text (NITF02.10/NSIF only)

-  .. co:: FSCATP
      :choices: string of 1 character

      File Classification Authority Type (NITF02.10/NSIF only)

-  .. co:: FSCAUT
      :choices: string of up to 40 characters

      File Classification Authority

-  .. co:: FSCRSN
      :choices: string of 1 character

      File Classification Reason

-  .. co:: FSSRDT
      :choices: string of 8 characters

      File Security Source Date (NITF02.10/NSIF only)

-  .. co:: FSCTLN
      :choices: string of up to 15 characters

      File Security Control Number (NITF02.10/NSIF only)

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

The following creation options to set fields in the NITF image header are available
for NITF 02.10 and NSIF 1.0. For NITF 02.00, consult MIL-STD-2500A.

-  .. co:: IID1
      :choices: string of up to 10 characters

      Image Identifier 1

-  .. co:: IDATIM
      :choices: string of 14 characters

      Image Date and Time.
      Format is DDHHMMSSZMONYY for NITF 2.0 and CCYYMMDDhhmmss for NITF 2.1.
      ``NOW`` is also accepted as a special value for the current date-time.

-  .. co:: TGTID
      :choices: string of up to 17 characters

      Target Identifier

-  .. co:: IID2
      :choices: string of up to 80 characters

      Image Identifier 2 (NITF02.10/NSIF only)

-  .. co:: ITITLE
      :choices: string of up to 80 characters

      Image Title (NITF02.00 only)

-  .. co:: ISCLAS
      :choices: string of 1 character

      Image Security Classification (U/R/C/S/T)

-  .. co:: ISCLSY
      :choices: string of up to 2 characters

      Image Classification Security System (NITF02.10/NSIF only)

-  .. co:: ISCODE
      :choices: string of up to 11 characters

      Image Codewords

-  .. co:: ISCTLH
      :choices: string of up to 2 characters

      Image Control and Handling

-  .. co:: ISREL
      :choices: string of up to 20 characters

      Image Releasing Instructions (NITF02.10/NSIF only)

-  .. co:: ISDCTP
      :choices: string of up to 2 characters

      Image Declassification Type (NITF02.10/NSIF only)

-  .. co:: ISDCDT
      :choices: string of 8 characters

      Image Declassification Date (NITF02.10/NSIF only)

-  .. co:: ISDCXM
      :choices: string of up to 4 characters

      Image Declassification Exemption (NITF02.10/NSIF only)

-  .. co:: ISDG
      :choices: string of 1 character

      Image Downgrade (NITF02.10/NSIF only)

-  .. co:: ISDGDT
      :choices: string of 8 characters

      Image Downgrade Date (NITF02.10/NSIF only)

-  .. co:: ISDWNG
      :choices: string of up to 6 characters

      Image Security Downgrade (NITF02.00 only)

-  .. co:: ISDEVT
      :choices: string of 40 characters

      Image Downgrading Event (NITF02.00 only)

-  .. co:: ISCLTX
      :choices: string of up to 43 characters

      Image Classification Text (NITF02.10/NSIF only)

-  .. co:: ISCATP
      :choices: string of 1 character

      Image Classification Authority Type (NITF02.10/NSIF only)

-  .. co:: ISCAUT
      :choices: string of up to 40 characters

      Image Classification Authority

-  .. co:: ISCRSN
      :choices: string of 1 character

      Image Classification Reason (NITF02.10/NSIF only)

-  .. co:: ISSRDT
      :choices: string of 8 characters

      Image Security Source Date (NITF02.10/NSIF only)

-  .. co:: ISCTLN
      :choices: string of up to 15 characters

      Image Security Control Number (NITF02.10/NSIF only)

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

CADRG (Compressed ARC Digitized Raster Graphics) (write support)
----------------------------------------------------------------

.. versionadded:: 3.13

The driver supports generating CADRG frames from any georeferenced source dataset,
which has a single band with a color table of up to 216 colors, a 3-band RGB
source dataset or a 4-band RGBA datasets. It will automatically create the CADRG
frames intersecting the source dataset extent in the appropriate Arc zones, at
the target scale. The :co:`PRODUCT_TYPE` creation option must be set to
``CADRG``. The output dataset name must be a directory name.

The target scale is determined by decreasing order of priority:

- from the value of the :co:`SCALE` creation option, if specified.

- from the value of the :co:`DPI` creation option, if specified.

- from the value of the :co:`SERIES_CODE` creation option, if a scale is associated
  with it (which is the case of all valid codes except
  ``HA``, ``CO``, ``OP``, ``CG``, ``CM`` and ``MM``)

- from the value of the TIFFTAG_RESOLUTIONUNIT and TIFFTAG_YRESOLUTION metadata
  items, if existing in the source dataset.

By default, hard-coded values of dates/date-times are written in the file, so
as to get binary reproducible outputs from a given input. It is possible to customize
them by setting the :co:`FDT`, :co:`IDATIM`, :co:`CURRENCY_DATE`, :co:`PRODUCTION_DATE`
and :co:`SIGNIFICANT_DATE` creation options. For all of them the special value ``NOW``
can be specified to use the current timestamp.

The frame index ``A.TOC`` file is automatically generated (it can also be
generated manually with the :ref:`gdal driver rpftoc create <raster.rpftoc.create>` program).

.. example::
   :title: Create CADRG frames from a VRT mosaic, specifying it is an
           Operational Navigation Chart (scale 1:1 million), using hard-coded dates

   .. code-block:: bash

        gdal raster convert input_mosaic.vrt output_directory \
            --format=NITF --co PRODUCT_TYPE=CADRG --co SERIES_CODE=ON

.. example::
   :title: Create CADRG frames from a VRT mosaic, specifying it is a City Graphic
           map at scale 1:5,000, with dates corresponding to the current timestamp.

   .. code-block:: bash

        gdal raster convert input_mosaic.vrt output_directory \
            --format=NITF --co PRODUCT_TYPE=CADRG --co SERIES_CODE=CG --co SCALE=5000 \
            --co OSTAID=MyCompany --co ONAME=Norway --co PRODUCER_CODE_ID=X --co SECURITY_COUNTRY_CODE=NO \
            --co FDT=NOW --co IDATIM=NOW --co CURRENCY_DATE=NOW \
            --co PRODUCTION_DATE=NOW --co SIGNIFICANT_DATE=NOW


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



.. below is an allow-list for spelling checker.

.. spelling:word-list::
        Pilotage
        DoD
        NIMA
