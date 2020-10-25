.. _raster.hdf4:

================================================================================
HDF4 -- Hierarchical Data Format Release 4 (HDF4)
================================================================================

.. shortname:: HDF4

.. shortname:: HDF4Image

.. build_dependencies:: libdf

There are two HDF formats, HDF4 (4.x and previous releases) and HDF5.
These formats are completely different and NOT compatible. This driver
intended for HDF4 file formats importing. NASA's Earth Observing System
(EOS) maintains its own HDF modification called HDF-EOS. This
modification is suited for use with remote sensing data and fully
compatible with underlying HDF. This driver can import HDF4-EOS files.
Currently EOS use HDF4-EOS for data storing (telemetry form \`Terra' and
\`Aqua' satellites). In the future they will switch to HDF5-EOS format,
which will be used for telemetry from \`Aura' satellite.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

Multiple Image Handling (Subdatasets)
-------------------------------------

Hierarchical Data Format is a container for several different datasets.
For data storing Scientific Datasets (SDS) used most often. SDS is a
multidimensional array filled by data. One HDF file may contain several
different SDS arrays. They may differ in size, number of dimensions and
may represent data for different regions.

If the file contains only one SDS that appears to be an image, it may be
accessed normally, but if it contains multiple images it may be
necessary to import the file via a two step process. The first step is
to get a report of the components images (SDS arrays) in the file using
**gdalinfo**, and then to import the desired images using
gdal_translate. The **gdalinfo** utility lists all multidimensional
subdatasets from the input HDF file. The name of individual images
(subdatasets) are assigned to the **SUBDATASET_n_NAME** metadata item.
The description for each image is found in the **SUBDATASET_n_DESC**
metadata item. For HDF4 images the subdataset names will be formatted
like this:

*HDF4_SDS:subdataset_type:file_name:subdataset_index*

where *subdataset_type* shows predefined names for some of the well
known HDF datasets, *file_name* is the name of the input file, and
*subdataset_index* is the index of the image to use (for internal use in
GDAL).

On the second step you should provide this name for **gdalinfo** or
**gdal_translate** for actual reading of the data.

For example, we want to read data from the MODIS Level 1B dataset:

::

   $ gdalinfo GSUB1.A2001124.0855.003.200219309451.hdf
   Driver: HDF4/Hierarchical Data Format Release 4
   Size is 512, 512
   Coordinate System is `'
   Metadata:
     HDFEOSVersion=HDFEOS_V2.7
     Number of Scans=204
     Number of Day mode scans=204
     Number of Night mode scans=0
     Incomplete Scans=0

...a lot of metadata output skipped...

::

   Subdatasets:
     SUBDATASET_1_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:0
     SUBDATASET_1_DESC=[408x271] Latitude (32-bit floating-point)
     SUBDATASET_2_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:1
     SUBDATASET_2_DESC=[408x271] Longitude (32-bit floating-point)
     SUBDATASET_3_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:2
     SUBDATASET_3_DESC=[12x2040x1354] EV_1KM_RefSB (16-bit unsigned integer)
     SUBDATASET_4_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:3
     SUBDATASET_4_DESC=[12x2040x1354] EV_1KM_RefSB_Uncert_Indexes (8-bit unsigned integer)
     SUBDATASET_5_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:4
     SUBDATASET_5_DESC=[408x271] Height (16-bit integer)
     SUBDATASET_6_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:5
     SUBDATASET_6_DESC=[408x271] SensorZenith (16-bit integer)
     SUBDATASET_7_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:6
     SUBDATASET_7_DESC=[408x271] SensorAzimuth (16-bit integer)
     SUBDATASET_8_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:7
     SUBDATASET_8_DESC=[408x271] Range (16-bit unsigned integer)
     SUBDATASET_9_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:8
     SUBDATASET_9_DESC=[408x271] SolarZenith (16-bit integer)
     SUBDATASET_10_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:9
     SUBDATASET_10_DESC=[408x271] SolarAzimuth (16-bit integer)
     SUBDATASET_11_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:10
     SUBDATASET_11_DESC=[408x271] gflags (8-bit unsigned integer)
     SUBDATASET_12_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:12
     SUBDATASET_12_DESC=[16x10] Noise in Thermal Detectors (8-bit unsigned integer)
     SUBDATASET_13_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:13
     SUBDATASET_13_DESC=[16x10] Change in relative responses of thermal detectors (8-bit unsigned integer)
     SUBDATASET_14_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:14
     SUBDATASET_14_DESC=[204x16x10] DC Restore Change for Thermal Bands (8-bit integer)
     SUBDATASET_15_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:15
     SUBDATASET_15_DESC=[204x2x40] DC Restore Change for Reflective 250m Bands (8-bit integer)
     SUBDATASET_16_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:16
     SUBDATASET_16_DESC=[204x5x20] DC Restore Change for Reflective 500m Bands (8-bit integer)
     SUBDATASET_17_NAME=HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:17
     SUBDATASET_17_DESC=[204x15x10] DC Restore Change for Reflective 1km Bands (8-bit integer)
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,  512.0)
   Upper Right (  512.0,    0.0)
   Lower Right (  512.0,  512.0)
   Center      (  256.0,  256.0)

Now select one of the subdatasets, described as
``[12x2040x1354] EV_1KM_RefSB (16-bit unsigned integer)``:

::

   $ gdalinfo HDF4_SDS:MODIS_L1B:GSUB1.A2001124.0855.003.200219309451.hdf:2
   Driver: HDF4Image/HDF4 Internal Dataset
   Size is 1354, 2040
   Coordinate System is `'
   Metadata:
     long_name=Earth View 1KM Reflective Solar Bands Scaled Integers

...metadata skipped...

::

   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0, 2040.0)
   Upper Right ( 1354.0,    0.0)
   Lower Right ( 1354.0, 2040.0)
   Center      (  677.0, 1020.0)
   Band 1 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 2 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 3 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 4 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 5 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 6 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 7 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 8 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 9 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 10 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 11 Block=1354x2040 Type=UInt16, ColorInterp=Undefined
   Band 12 Block=1354x2040 Type=UInt16, ColorInterp=Undefined

Or you may use **gdal_translate** for reading image bands from this
dataset.

Note that you should provide exactly the contents of the line marked
**SUBDATASET_n_NAME** to GDAL, including the **HDF4_SDS:** prefix.

This driver is intended only for importing remote sensing and geospatial
datasets in form of raster images. If you want explore all data
contained in HDF file you should use another tools (you can find
information about different HDF tools using links at end of this page).

Georeference
------------

There is no universal way of storing georeferencing in HDF files.
However, some product types have mechanisms for saving georeferencing,
and some of these are supported by GDAL. Currently supported are
(*subdataset_type* shown in parenthesis):

-  HDF4 files created by GDAL (**GDAL_HDF4**)
-  ASTER Level 1A (**ASTER_L1A**)
-  ASTER Level 1B (**ASTER_L1B**)
-  ASTER Level 2 (**ASTER_L2**)
-  ASTER DEM (**AST14DEM**)
-  MODIS Level 1B Earth View products (**MODIS_L1B**)
-  MODIS Level 3 products (**MODIS_L3**)
-  SeaWiFS Level 3 Standard Mapped Image Products (**SEAWIFS_L3**)

By default the hdf4 driver only reads the gcps from every 10th row and
column from EOS_SWATH datasets. You can change this behavior by setting
the GEOL_AS_GCPS environment variable to PARTIAL (default), NONE, or
FULL.

Creation Issues
---------------

This driver supports creation of the HDF4 Scientific Datasets. You may
create set of 2D datasets (one per each input band) or single 3D dataset
where the third dimension represents band numbers. All metadata and band
descriptions from the input dataset are stored as HDF4 attributes.
Projection information (if it exists) and affine transformation
coefficients also stored in form of attributes. Files, created by GDAL
have the special attribute:

"Signature=Created with GDAL (http://www.remotesensing.org/gdal/)"

and are automatically recognised when read, so the projection info and
transformation matrix restored back.

Creation Options:

-  **RANK=n**: Create **n**-dimensional SDS. Currently only 2D and 3D
   datasets supported. By default a 3-dimensional dataset will be
   created.

Metadata
--------

All HDF4 attributes are transparently translated as GDAL metadata. In
the HDF file attributes may be assigned assigned to the whole file as
well as to particular subdatasets.

Open options
------------

The following open option is supported:

- **LIST_SDS=AUTO/YES/NO**: (GDAL >= 3.2) Whether to report Scientific Data Sets (SDS).
  By default, when a HDF file contains EOS_SWATH or EOS_GRID, SDS will not be
  listed as GDAL subdatasets (as this would cause them to be reported twice).
  Listing them can be forced by setting LIST_SDS to YES.


Multidimensional API support
----------------------------

.. versionadded:: 3.1

The HDF4 driver supports the :ref:`multidim_raster_data_model` for reading
operations.

Driver building
---------------

This driver built on top of NCSA HDF library, so you need one to compile
GDAL with HDF4 support. You may search your operating system
distribution for the precompiled binaries or download source code or
binaries from the NCSA HDF Home Page (see links below).

Please note, that NCSA HDF library compiled with several defaults which
is defined in *hlimits.h* file. For example, *hlimits.h* defines the
maximum number of opened files:

::

   #   define MAX_FILE   32

If you need open more HDF4 files simultaneously you should change this
value and rebuild HDF4 library (and relink GDAL if using static HDF
libraries).

See Also
--------

-  Implemented as ``gdal/frmts/hdf4/hdf4dataset.cpp`` and
   ``gdal/frmts/hdf4/hdf4imagedataset.cpp``.
-  `The HDF Group <http://www.hdfgroup.org/>`__
-  Sources of the data in HDF4 and HDF4-EOS formats:

   `Earth Observing System Data
   Gateway <http://edcimswww.cr.usgs.gov/pub/imswelcome/>`__

Documentation to individual products, supported by this driver:

-  `Geo-Referencing ASTER L1B
   Data <http://edcdaac.usgs.gov/aster/ASTER_GeoRef_FINAL.pdf>`__
-  `ASTER Standard Data Product Specifications
   Document <http://asterweb.jpl.nasa.gov/documents/ASTERHigherLevelUserGuideVer2May01.pdf>`__
-  `MODIS Level 1B Product Information and
   Status <http://www.mcst.ssai.biz/mcstweb/L1B/product.html>`__
-  `MODIS Ocean User's
   Guide <http://modis-ocean.gsfc.nasa.gov/userguide.html>`__
