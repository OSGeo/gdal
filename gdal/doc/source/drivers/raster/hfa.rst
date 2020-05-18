.. _raster.hfa:

================================================================================
HFA -- Erdas Imagine .img
================================================================================

.. shortname:: HFA

.. built_in_by_default::

GDAL supports Erdas Imagine .img format for read access and write. The
driver supports reading overviews, palettes, and georeferencing. It
supports the Erdas band types u8, s8, u16, s16, u32, s32, f32, f64, c64
and c128.

Compressed and missing tiles in Erdas files should be handled properly
on read. Files between 2GiB and 4GiB in size should work on Windows NT,
and may work on some Unix platforms. Files with external spill files
(needed for datasets larger than 2GiB) are also support for reading and
writing.

Metadata reading and writing is supported at the dataset level, and for
bands, but this is GDAL specific metadata - not metadata in an Imagine
recognized form. The metadata is stored in a table called GDAL_MetaData
with each column being a metadata item. The title is the key and the row
1 value is the value.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

Erdas Imagine files can be created with any GDAL defined band type,
including the complex types. Created files may have any number of bands.
Pseudo-Color tables will be written if using the
GDALDriver::CreateCopy() methodology. Most projections should be
supported though translation of unusual datums (other than WGS84, WGS72,
NAD83, and NAD27) may be problematic.

Creation Options:

-  **BLOCKSIZE=blocksize**: Tile width/height (32-2048). Default=64
-  **USE_SPILL=YES**: Force the generation of a spill file (by default
   spill file created for images larger 2GiB only). Default=NO
-  **COMPRESSED=YES**: Create file as compressed. Use of spill file
   disables compression. Default=NO
-  **NBITS=1/2/4**: Create file with special sub-byte data types.
-  **PIXELTYPE=[DEFAULT/SIGNEDBYTE]**: By setting this to SIGNEDBYTE, a
   new Byte file can be forced to be written as signed byte.
-  **AUX=YES**: To create a .aux file. Default=NO
-  **IGNOREUTM=YES** : Ignore UTM when selecting coordinate system -
   will use Transverse Mercator. Only used for Create() method.
   Default=NO
-  **STATISTICS=YES** : To generate statistics and a histogram.
   Default=NO
-  **DEPENDENT_FILE=filename** : Name of dependent file (must not have
   absolute path). Optional
-  **FORCETOPESTRING=YES**: Force use of ArcGIS PE String in file
   instead of Imagine coordinate system format. In some cases this
   improves ArcGIS coordinate system compatibility.

Erdas Imagine supports external creation of overviews (with gdaladdo for
instance). To force them to be created in an .rrd file (rather than
inside the original .img) set the global config option HFA_USE_RRD=YES).

Layer names can be set and retrieved with the
GDALSetDescription/GDALGetDescription calls on the Raster Band objects.

Some HFA band metadata exported to GDAL metadata:

-  LAYER_TYPE - layer type (athematic, ... )
-  OVERVIEWS_ALGORITHM - layer overviews algorithm ('IMAGINE 2X2
   Resampling', 'IMAGINE 4X4 Resampling', and others)

Configuration Options
---------------------

Currently three `runtime configuration
options <http://trac.osgeo.org/gdal/wiki/ConfigOptions>`__ are supported
by the HFA driver:

-  **HFA_USE_RRD=YES/NO** : Whether to force creation of external
   overviews in Erdas rrd format and with .rrd file name extension
   (gdaladdo with combination -ro --config USE_RRD YES creates overview
   file with .aux extension).
-  **HFA_COMPRESS_OVR=YES/NO** : Whether to create
   compressed overviews. Default is to only create compressed overviews
   when the file is compressed.

   This configuration option can be used when building external
   overviews for a base image that is not in Erdas Imagine format.
   Resulting overview file will use the rrd structure and have .aux
   extension.

   ::

      gdaladdo out.tif --config USE_RRD YES --config HFA_COMPRESS_OVR YES 2 4 8

   Erdas Imagine and older ArcGIS versions may recognize overviews for
   some image formats only if they have .rrd extension. In this case
   use:

   ::

      gdaladdo out.tif --config USE_RRD YES --config HFA_USE_RRD YES --config HFA_COMPRESS_OVR YES 2 4 8

-  (GDAL >= 2.3) The block size (tile width/height) used for overviews
   can be specified by setting the **GDAL_HFA_OVR_BLOCKSIZE**
   configuration option to a power- of-two value between 32 and 2048.
   The default value is 64.

See Also
--------

-  Implemented as ``gdal/frmts/hfa/hfadataset.cpp``.
-  More information, and other tools are available on the `Imagine
   (.img)
   Reader <http://web.archive.org/web/20130730133056/http://home.gdal.org/projects/imagine/hfa_index.html>`__
   page as saved by archive.org.
-  `Erdas.com <http://www.erdas.com/>`__
