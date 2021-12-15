.. _raster.jp2kak:

================================================================================
JP2KAK -- JPEG-2000 (based on Kakadu)
================================================================================

.. shortname:: JP2KAK

.. build_dependencies:: Kakadu library

Most forms of JPEG2000 JP2 and JPC compressed images (ISO/IEC 15444-1)
can be read with GDAL using a driver based on the Kakadu library. As
well, new images can be written. Existing images cannot be updated in
place.

The JPEG2000 file format supports lossy and lossless compression of 8bit
and 16bit images with 1 or more bands (components). Via the `GeoJP2
(tm) <https://web.archive.org/web/20151028081930/http://www.lizardtech.com/download/geo/geotiff_box.txt>`__
mechanism, GeoTIFF style coordinate system and georeferencing
information can be embedded within the JP2 file. JPEG2000 files use a
substantially different format and compression mechanism than the
traditional JPEG compression and JPEG JFIF format. They are distinct
compression mechanisms produced by the same group. JPEG2000 is based on
wavelet compression.

The JPEG2000 driver documented on this page (the JP2KAK driver) is
implemented on top of the proprietary
`Kakadu <http://www.kakadusoftware.com/>`__ library. This is a high
quality and high performance JPEG2000 library in wide used in the
geospatial and general imaging community. However, it is not free, and
so normally builds of GDAL from source will not include support for this
driver unless the builder purchases a license for the library and
configures accordingly.

When reading images this driver will represent the bands as being Byte
(8bit unsigned), 16 bit signed/unsigned, and 32 bit signed/unsigned. Georeferencing and
coordinate system information will be available if the file is a GeoJP2
(tm) file. Files color encoded in YCbCr color space will be
automatically translated to RGB. Paletted images are also supported.

XMP metadata can be extracted from JPEG2000
files, and will be stored as XML raw content in the xml:XMP metadata
domain.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Configuration Options
---------------------

The JP2KAK driver supports the following `Config
Options <http://trac.osgeo.org/gdal/wiki/ConfigOptions>`__. These
runtime options can be used to alter the behavior of the driver.

-  **JP2KAK_THREADS**\ =n: By default an effort is made to take
   advantage of multi-threading on multi-core computers using default
   rules from the Kakadu library. This option may be set to a value of
   zero to avoid using additional threads or to a specific count to
   create the requested number of worker threads.
-  **JP2KAK_FUSSY**\ =YES/NO: This can be set to YES to turn on fussy
   reporting of problems with the JPEG2000 data stream. Defaults to NO.
-  **JP2KAK_RESILIENT**\ =YES/NO: This can be set to YES to force Kakadu
   to maximize resilience with incorrectly created JPEG2000 data files,
   likely at some cost in performance. This is likely to be necessary
   if, among other reasons, you get an error message about "Expected to
   find EPH marker following packet header" or error reports indicating
   the need to run with the resilient and sequential flags on. Defaults
   to NO.

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

Creation Issues
---------------

JPEG2000 files can only be created using the CreateCopy mechanism to
copy from an existing dataset.

JPEG2000 overviews are maintained as part of the mathematical
description of the image. Overviews cannot be built as a separate
process, but on read the image will generally be represented as having
overview levels at various power of two factors.

Creation Options:

-  **CODEC=JP2/J2K** Codec to use. If not specified, guess based on file
   extension. If unknown, default to JP2
-  **QUALITY=n**: Set the compressed size ratio as a percentage of the
   size of the uncompressed image. The default is 20 indicating that the
   resulting image should be 20% of the size of the uncompressed image.
   Actual final image size may not exactly match that requested
   depending on various factors. A value of 100 will result in use of
   the lossless compression algorithm . On typical image data, if you
   specify a value greater than 65, it might be worth trying with
   QUALITY=100 instead as lossless compression might produce better
   compression than lossy compression.
-  **BLOCKXSIZE=n**: Set the tile width to use. Defaults to 20000.
-  **BLOCKYSIZE=n**: Set the tile height to use. Defaults to image
   height.
-  **FLUSH=TRUE/FALSE**: Enable/Disable incremental flushing when
   writing files. Required to be FALSE for RLPC and LRPC Corder. May use
   a lot of memory when FALSE while writing large images. Defaults to
   TRUE.
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
-  **LAYERS=n**: Control the number of layers produced. These are sort
   of like resolution layers, but not exactly. The default value is 12
   and this works well in most situations.
-  **ROI=xoff,yoff,xsize,ysize**: Selects a region to be a region of
   interest to process with higher data quality. The various "R" flags
   below may be used to control the amount better. For example the
   settings "ROI=0,0,100,100", "Rweight=7" would encode the top left
   100x100 area of the image with considerable higher quality compared
   to the rest of the image.

The following creation options are tightly tied to the Kakadu library,
and are considered to be for advanced use only. Consult Kakadu
documentation to better understand their meaning.

-  **Corder**: Defaults to "PRCL".
-  **Cprecincts**: Defaults to
   "{512,512},{256,512},{128,512},{64,512},{32,512},{16,512},{8,512},{4,512},{2,512}".
-  **ORGgen_plt**: Defaults to "yes".
-  **ORGgen_tlm**: Kakadu library default used.
-  **ORGtparts**: Kakadu library default used.
-  **Cmodes**: Kakadu library default used.
-  **Clevels**: Kakadu library default used.
-  **Rshift**: Kakadu library default used.
-  **Rlevels**: Kakadu library default used.
-  **Rweight**: Kakadu library default used.
-  **Qguard**: Kakadu library default used.
-  **Sprofile**: Kakadu library default used.

Known Kakadu Issues
-------------------

Alpha Channel Writing in v7.8
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Kakadu v7.8 has a bug in jp2_channels::set_opacity_mapping that can
cause an error when writing images with an alpha channel. Please upgrade
to version 7.9.

::

   Error: GdalIO: Error in Kakadu File Format Support: Attempting to
   create a Component Mapping (cmap) box, one of whose channels refers to
   a non-existent image component or palette lookup table. (code = 1)

kdu_get_num_processors always returns 0 for some platforms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On non-windows / non-mac installs (e.g. Linux), Kakadu might not include
unistd.h in kdu_arch.cpp. This means that \_SC_NPROCESSORS_ONLN and
\_SC_NPROCESSORS_CONF are not defined and kdu_get_num_processors will
always return 0. Therefore the jp2kak driver might not default to
creating worker threads.

See Also
--------

-  Implemented as `gdal/frmts/jp2kak/jp2kakdataset.cpp`.
-  If you're using a Kakadu release before v7.5, configure & compile
   GDAL with eg.
   `CXXFLAGS="-DKDU_MAJOR_VERSION=7 -DKDU_MINOR_VERSION=3 -DKDU_PATCH_VERSION=2"`
   for Kakadu version 7.3.2.
-  Alternate :ref:`raster.jp2openjpeg` driver.
