.. _raster.fits:

================================================================================
FITS -- Flexible Image Transport System
================================================================================

.. shortname:: FITS

.. build_dependencies:: libcfitsio

FITS is a format used mainly by astronomers, but it is a relatively
simple format that supports arbitrary image types and multi-spectral
images, and so has found its way into GDAL. FITS support is implemented
in terms of the standard `CFITSIO
library <http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html>`__,
which you must have on your system in order for FITS support to be
enabled (see :ref:`notes on CFITSIO linking <notes-on-cfitsio-linking>`).
Both reading and writing of FITS files is supported.

Starting from version 3.0
georeferencing system support is implemented via the conversion of
WCS (World Coordinate System) keywords.
Only Latitude - Longitude systems (see the `FITS standard document
<https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf#subsection.8.3>`_)
have been implemented, those for which remote sensing processing is commonly used.
As 3D Datum information is missing in FITS/WCS standard, Radii and target bodies
are translated using the planetary extension proposed `here
<https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2018EA000388>`_. 

Non-standard header keywords that are present in the FITS file will be
copied to the dataset's metadata when the file is opened, for access via
GDAL methods. Similarly, non-standard header keywords that the user
defines in the dataset's metadata will be written to the FITS file when
the GDAL handle is closed.

Note to those familiar with the CFITSIO library: The automatic rescaling
of data values, triggered by the presence of the BSCALE and BZERO header
keywords in a FITS file, is disabled in GDAL < v3.0. Those header keywords are
accessible and updatable via dataset metadata, in the same was as any
other header keywords, but they do not affect reading/writing of data
values from/to the file. Starting from version 3.0 BZERO and BSCALE keywords
are managed via standard :cpp:func:`GDALRasterBand::GetOffset` / :cpp:func:`GDALRasterBand::SetOffset`
and :cpp:func:`GDALRasterBand::GetScale` / :cpp:func:`GDALRasterBand::SetScale` GDAL functions and no more
referred as metadata.

Multiple image support
----------------------

Starting with GDAL 3.2, Multi-Extension FITS (MEF) files that contain one or
more extensions following the primary HDU are supported. When more than 2 image
HDUs are found, they are reported as subdatasets.

The connection string for a given subdataset/HDU is ``FITS:"filename.fits":hdu_number``

Binary table support
--------------------

Starting with GDAL 3.2, binary tables will be exposed as vector layers (update
and creation support from GDAL 3.2.1).

The FITS data types are mapped to OGR data types as the following:

.. list-table:: Data types
   :header-rows: 1

   * - TFORM value
     - TSCAL, TOFFSET value
     - Occurrence count
     - OGR field type
     - OGR field subtype
   * - 'L' (Logical)
     - ignored
     - 1
     - OFTInteger
     - OFSTBoolean
   * - 'L' (Logical)
     - ignored
     - > 1
     - OFTIntegerList
     - OFSTBoolean
   * - 'X' (bit)
     - ignored
     - each bit mapped to a OGR field
     - OFTInteger
     - OFSTNone
   * - 'B' (unsigned byte)
     - 1, 0 (unsigned byte) or 1, -128 (signed byte)
     - 1
     - OFTInteger
     - OFSTNone
   * - 'B' (unsigned byte)
     - 1, 0 (unsigned byte) or 1, -128 (signed byte)
     - > 1
     - OFTIntegerList
     - OFSTNone
   * - 'I' (16 bit signed integer)
     - 1, 0
     - 1
     - OFTInteger
     - OFSTInt16
   * - 'I' (16 bit integer, interpreted as unsigned)
     - 1, 32768
     - 1
     - OFTInteger
     - OFSTNone
   * - 'I' (16 bit signed integer)
     - other than (1,0) and (1,32768)
     - 1
     - OFTReal
     - OFSTNone
   * - 'I' (16 bit integer)
     - 1, 0
     - >1
     - OFTIntegerList
     - OFSTInt16
   * - 'I' (16 bit integer, interpreted as unsigned)
     - 1, 32768
     - >1
     - OFTIntegerList
     - OFSTNone
   * - 'I' (16 bit signed integer)
     - other than (1, 0) and (1, 32768)
     - > 1
     - OFTRealList
     - OFSTNone
   * - 'J' (32 bit signed integer)
     - 1, 0
     - 1
     - OFTInteger
     - OFSTNone
   * - 'J' (32 bit integer, interpreted as unsigned)
     - 1, 2147483648
     - 1
     - OFTInteger
     - OFSTNone
   * - 'J' (32 bit signed integer)
     - other than (1, 0) and (1, 2147483648)
     - 1
     - OFTReal
     - OFSTNone
   * - 'J' (32 bit integer)
     - 1, 0
     - >1
     - OFTIntegerList
     - OFSTNone
   * - 'J' (32 bit integer, interpreted as unsigned)
     - 1, 2147483648
     - >1
     - OFTIntegerList
     - OFSTNone
   * - 'J' (32 bit signed integer)
     - other than (1, 0) and (1, 2147483648)
     - > 1
     - OFTRealList
     - OFSTNone
   * - 'K' (64 bit signed integer)
     - 1, 0
     - 1
     - OFTInteger64
     - OFSTNone
   * - 'K' (64 bit signed integer)
     - other than (1, 0)
     - 1
     - OFTReal
     - OFSTNone
   * - 'K' (64 bit signed integer)
     - 1, 0
     - > 1
     - OFTInteger64
     - OFSTNone
   * - 'K' (64 bit signed integer)
     - other than (1, 0)
     - > 1
     - OFTRealList
     - OFSTNone
   * - 'A' (character)
     - ignored
     - if TFORM='Axxx' and no TDIM header
     - OFTString
     - OFSTNone
   * - 'A' (character)
     - ignored
     - TDIM for 2D field, or variable length ('PA')
     - OFTStringList
     - OFSTNone
   * - 'E' (single precision floating point)
     - 1, 0
     - 1
     - OFTReal
     - OFSTFloat32
   * - 'E' (single precision floating point)
     - other than (1, 0)
     - 1
     - OFTReal
     - OFSTNone
   * - 'E' (single precision floating point)
     - 1, 0
     - > 1
     - OFTRealList
     - OFSTFloat32
   * - 'E' (single precision floating point)
     - other than (1, 0)
     - > 1
     - OFTRealList
     - OFSTNone
   * - 'D' (double precision floating point)
     - any
     - 1
     - OFTReal
     - OFSTNone
   * - 'D' (double precision floating point)
     - any
     - > 1
     - OFTRealList
     - OFSTNone
   * - 'C' (single precision complex)
     - any
     - 1
     - OFTString whose value is of the form "x + yj"
     - OFSTNone
   * - 'C' (single precision complex)
     - any
     - > 1
     - OFTStringList whose values are of the form "x + yj"
     - OFSTNone
   * - 'M' (double precision complex)
     - any
     - 1
     - OFTString whose value is of the form "x + yj"
     - OFSTNone
   * - 'M' (double precision complex)
     - any
     - > 1
     - OFTStringList whose values are of the form "x + yj"
     - OFSTNone

Fields with a repeat count > 1 expressing fixed size arrays, or fields using
array descriptors 'P' and 'Q' for variable length arrays are mapped to OGR OFTxxxxxList
data types. The potential 2D structure of such field has no direct equivalence in
OGR, so OGR will expose a linear structure. For fixed size arrays, the user can retrieve
the value of the TDIMxx header in the layer metadata to recover the dimensionality
of the field.

Fields that have TSCAL and/or TZERO headers are automatically scaled and offset
to the physical value (only applies to numeric data types)

TNULL headers are used for integer numeric data types and for a single-occurence
field to set a OGR field to NULL.

Layer creation options
----------------------

The following layer creation options are available:

- **REPEAT_{fieldname}=number**. For a given field (substitute {fieldname} by its
  name) of type IntegerList, Integer64List
  or RealList, specify a fixed number of elements. Otherwise those fields will be
  created as variable-length FITS columns, which can have performance impact on
  creation.

- **COMPUTE_REPEAT=AT_FIELD_CREATION/AT_FIRST_FEATURE_CREATION**. For fields of
  type IntegerList, Integer64List or RealList, specifies when they are mapped to
  a FITS column type. The default is AT_FIELD_CREATION, and implies that they
  will be created as variable-length FITS columns, unless a REPEAT_{fieldname}
  option is specified. When AT_FIRST_FEATURE_CREATION is specified, the number of
  elements in the first feature will be taken into account to create fixed-size
  FITS columns.

When using ogr2ogr or :cpp:func:`GDALVectorTranslate` with a FITS source, the
FITS header will be taken into account, in particular to help to determine the
FITS data type of target columns.

Examples
--------

* Listing subdatasets in a MEF .fits:

    ::

        $ gdalinfo ../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits

        Driver: FITS/Flexible Image Transport System
        Files: ../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits
        Size is 512, 512
        Metadata:
        EXTNAME=FIRST_IMAGE
        Subdatasets:
        SUBDATASET_1_NAME=FITS:"../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits":1
        SUBDATASET_1_DESC=HDU 1 (1x2, 1 band), FIRST_IMAGE
        SUBDATASET_2_NAME=FITS:"../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits":2
        SUBDATASET_2_DESC=HDU 2 (1x3, 1 band)
        Corner Coordinates:
        Upper Left  (    0.0,    0.0)
        Lower Left  (    0.0,  512.0)
        Upper Right (  512.0,    0.0)
        Lower Right (  512.0,  512.0)
        Center      (  256.0,  256.0)

* Opening a given raster HDU:

    ::

        $ gdalinfo FITS:"../autotest/gdrivers/data/fits/image_in_first_and_second_hdu.fits":1

        Driver: FITS/Flexible Image Transport System
        Files: none associated
        Size is 1, 2
        Metadata:
        EXTNAME=FIRST_IMAGE
        Corner Coordinates:
        Upper Left  (    0.0,    0.0)
        Lower Left  (    0.0,    2.0)
        Upper Right (    1.0,    0.0)
        Lower Right (    1.0,    2.0)
        Center      (    0.5,    1.0)
        Band 1 Block=1x1 Type=Byte, ColorInterp=Undefined

* Listing potential binary tables in a FITS file:

    ::

        $ ogrinfo my.fits


* Converting a GeoPackage layer into a FITS binary table:


    ::

        $ ogr2ogr out.fits my.gpkg my_table


Other
-----

NOTE: Implemented as ``gdal/frmts/fits/fitsdataset.cpp``.

.. _notes-on-cfitsio-linking:

Notes on CFITSIO linking in GDAL
--------------------------------
Linux
^^^^^
From source
"""""""""""
Install CFITSIO headers from your distro (eg, cfitsio-devel on Fedora; libcfitsio-dev on Debian-Ubuntu), then compile GDAL as usual. CFITSIO will be automatically detected and linked.

From distros
""""""""""""
On Fedora/CentOS install CFITSIO then GDAL with dnf (yum): cfitsio is automatically linked.

MacOSX
^^^^^^
The last versions of the MacOSX packages are not linked against CFITSIO.
Install CFITSIO as described in the `official documentation <https://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio_macosx.html>`__.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
