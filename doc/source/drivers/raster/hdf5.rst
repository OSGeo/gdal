.. _raster.hdf5:

================================================================================
HDF5 -- Hierarchical Data Format Release 5 (HDF5)
================================================================================

.. shortname:: HDF5

.. shortname:: HDF5Image

.. build_dependencies:: libhdf5

This driver intended for HDF5 file formats importing.

Driver capabilities
-------------------

.. supports_georeferencing::

.. versionadded:: 2.4

    .. supports_virtualio::

Multiple Image Handling (Subdatasets)
-------------------------------------

Hierarchical Data Format is a container for several different datasets.
For data storing. HDF contains multidimensional arrays filled by data.
One HDF file may contain several arrays. They may differ in size, number
of dimensions.

The first step is to get a report of the components images (arrays) in
the file using **gdalinfo**, and then to import the desired images using
gdal_translate. The **gdalinfo** utility lists all multidimensional
subdatasets from the input HDF file. The name of individual images
(subdatasets) are assigned to the **SUBDATASET_n_NAME** metadata item.
The description for each image is found in the **SUBDATASET_n_DESC**
metadata item. For HDF5 images the subdataset names will be formatted
like this:

*HDF5:file_name:subdataset*

| where:
| *file_name* is the name of the input file, and
| *subdataset* is the dataset name of the array to use (for internal use
  in GDAL).

On the second step you should provide this name for **gdalinfo** or
**gdal_translate** for actual reading of the data.

For example, we want to read data from the OMI/Aura Ozone (O3) dataset:

::

   $ gdalinfo OMI-Aura_L2-OMTO3_2005m0326t2307-o03709_v002-2005m0428t201311.he5
   Driver: HDF5/Hierarchical Data Format Release 5
   Size is 512, 512
   Coordinate System is `'

   Subdatasets:
     SUBDATASET_1_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/APrioriLayerO3
     SUBDATASET_1_DESC=[1496x60x11] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/APrioriLayerO3 (32-bit floating-point)
     SUBDATASET_2_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/AlgorithmFlags
     SUBDATASET_2_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/AlgorithmFlags (8-bit unsigned character)
     SUBDATASET_3_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/CloudFraction
     SUBDATASET_3_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/CloudFraction (32-bit floating-point)
     SUBDATASET_4_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/CloudTopPressure
     SUBDATASET_4_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/CloudTopPressure (32-bit floating-point)
     SUBDATASET_5_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/ColumnAmountO3
     SUBDATASET_5_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/ColumnAmountO3 (32-bit floating-point)
     SUBDATASET_6_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/LayerEfficiency
     SUBDATASET_6_DESC=[1496x60x11] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/LayerEfficiency (32-bit floating-point)
     SUBDATASET_7_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/NValue
     SUBDATASET_7_DESC=[1496x60x12] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/NValue (32-bit floating-point)
     SUBDATASET_8_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/O3BelowCloud
     SUBDATASET_8_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/O3BelowCloud (32-bit floating-point)
     SUBDATASET_9_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/QualityFlags
     SUBDATASET_9_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/QualityFlags (16-bit unsigned integer)
     SUBDATASET_10_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Reflectivity331
     SUBDATASET_10_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Reflectivity331 (32-bit floating-point)
     SUBDATASET_11_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Reflectivity360
     SUBDATASET_11_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Reflectivity360 (32-bit floating-point)
     SUBDATASET_12_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Residual
     SUBDATASET_12_DESC=[1496x60x12] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Residual (32-bit floating-point)
     SUBDATASET_13_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/ResidualStep1
     SUBDATASET_13_DESC=[1496x60x12] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/ResidualStep1 (32-bit floating-point)
     SUBDATASET_14_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/ResidualStep2
     SUBDATASET_14_DESC=[1496x60x12] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/ResidualStep2 (32-bit floating-point)
     SUBDATASET_15_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/SO2index
     SUBDATASET_15_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/SO2index (32-bit floating-point)
     SUBDATASET_16_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Sensitivity
     SUBDATASET_16_DESC=[1496x60x12] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/Sensitivity (32-bit floating-point)
     SUBDATASET_17_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/StepOneO3
     SUBDATASET_17_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/StepOneO3 (32-bit floating-point)
     SUBDATASET_18_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/StepTwoO3
     SUBDATASET_18_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/StepTwoO3 (32-bit floating-point)
     SUBDATASET_19_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/TerrainPressure
     SUBDATASET_19_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/TerrainPressure (32-bit floating-point)
     SUBDATASET_20_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex
     SUBDATASET_20_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex (32-bit floating-point)
     SUBDATASET_21_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/dN_dR
     SUBDATASET_21_DESC=[1496x60x12] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/dN_dR (32-bit floating-point)
     SUBDATASET_22_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/dN_dT
     SUBDATASET_22_DESC=[1496x60x12] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/dN_dT (32-bit floating-point)
     SUBDATASET_23_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/GroundPixelQualityFlags
     SUBDATASET_23_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/GroundPixelQualityFlags (16-bit unsigned integer)
     SUBDATASET_24_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/Latitude
     SUBDATASET_24_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/Latitude (32-bit floating-point)
     SUBDATASET_25_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/Longitude
     SUBDATASET_25_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/Longitude (32-bit floating-point)
     SUBDATASET_26_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/RelativeAzimuthAngle
     SUBDATASET_26_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/RelativeAzimuthAngle (32-bit floating-point)
     SUBDATASET_27_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/SolarAzimuthAngle
     SUBDATASET_27_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/SolarAzimuthAngle (32-bit floating-point)
     SUBDATASET_28_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/SolarZenithAngle
     SUBDATASET_28_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/SolarZenithAngle (32-bit floating-point)
     SUBDATASET_29_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/TerrainHeight
     SUBDATASET_29_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/TerrainHeight (16-bit integer)
     SUBDATASET_30_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/ViewingAzimuthAngle
     SUBDATASET_30_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/ViewingAzimuthAngle (32-bit floating-point)
     SUBDATASET_31_NAME=HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/ViewingZenithAngle
     SUBDATASET_31_DESC=[1496x60] //HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/ViewingZenithAngle (32-bit floating-point)
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,  512.0)
   Upper Right (  512.0,    0.0)
   Lower Right (  512.0,  512.0)
   Center      (  256.0,  256.0)

Now select one of the subdatasets, described as
``[1645x60] CloudFraction (32-bit floating-point)``:

::

   $ gdalinfo HDF5:"OMI-Aura_L2-OMTO3_2005m0326t2307-o03709_v002-2005m0428t201311.he5":CloudFraction
   Driver: HDF5Image/HDF5 Dataset
   Size is 60, 1645
   Coordinate System is:
   GEOGCS["WGS 84",
       DATUM["WGS_1984",
           SPHEROID["WGS 84",6378137,298.257223563,
               AUTHORITY["EPSG","7030"]],
           TOWGS84[0,0,0,0,0,0,0],
           AUTHORITY["EPSG","6326"]],
       PRIMEM["Greenwich",0,
           AUTHORITY["EPSG","8901"]],
       UNIT["degree",0.0174532925199433,
           AUTHORITY["EPSG","9108"]],
       AXIS["Lat",NORTH],
       AXIS["Long",EAST],
       AUTHORITY["EPSG","4326"]]
   GCP Projection = GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4326"]]
   GCP[  0]: Id=, Info=
             (0.5,0.5) -> (261.575,-84.3495,0)
   GCP[  1]: Id=, Info=
             (2.5,0.5) -> (240.826,-85.9928,0)
   GCP[  2]: Id=, Info=
             (4.5,0.5) -> (216.754,-86.5932,0)
   GCP[  3]: Id=, Info=
             (6.5,0.5) -> (195.5,-86.5541,0)
   GCP[  4]: Id=, Info=
             (8.5,0.5) -> (180.265,-86.2009,0)
   GCP[  5]: Id=, Info=
             (10.5,0.5) -> (170.011,-85.7315,0)
   GCP[  6]: Id=, Info=
             (12.5,0.5) -> (162.987,-85.2337,0)
   ... 3000 GCPs are read from the file if Latitude and Longitude arrays are presents

Corner Coordinates: Upper Left ( 0.0, 0.0) Lower Left ( 0.0, 1645.0)
Upper Right ( 60.0, 0.0) Lower Right ( 60.0, 1645.0) Center ( 30.0,
822.5) Band 1 Block=60x1 Type=Float32, ColorInterp=Undefined Open GDAL
Datasets: 1 N DriverIsNULL 512x512x0

You may use **gdal_translate** for reading image bands from this
dataset.

Note that you should provide exactly the contents of the line marked
**SUBDATASET_n_NAME** to GDAL, including the **HDF5:** prefix.

This driver is intended only for importing remote sensing and geospatial
datasets in form of raster images(2D or 3D arrays). If you want explore
all data contained in HDF file you should use another tools (you can
find information about different HDF tools using links at end of this
page).

Georeference
------------

There is no universal way of storing georeferencing in HDF files.
However, some product types have mechanisms for saving georeferencing,
and some of these are supported by GDAL. Currently supported are
(*subdataset_type* shown in parenthesis):

-  HDF5 OMI/Aura Ozone (O3) Total Column 1-Orbit L2 Swath 13x24km
   (**Level-2 OMTO3**)


Multi-file support
------------------

Starting with GDAL 3.1, the driver supports opening datasets split over
several files using the 'family' HDF5 file driver. For that, GDAL must be
provided with the filename of the first part, containing in it a single '0'
(zero) character, or ending with 0.h5 or 0.hdf5

Multidimensional API support
----------------------------

.. versionadded:: 3.1

The HDF5 driver supports the :ref:`multidim_raster_data_model` for reading
operations.

Driver building
---------------

This driver built on top of NCSA HDF5 library, so you need to download
prebuild HDF5 libraries: HDF5-1.6.4 library or higher. You also need
zlib 1.2 and szlib 2.0. For windows user be sure to set the attributes
writable (especially if you are using Cygwin) and that the DLLs can be
located somewhere by your PATH environment variable. You may also
download source code NCSA HDF Home Page (see links below).

See Also
--------

Implemented as ``gdal/frmts/hdf5/hdf5dataset.cpp`` and
``gdal/frmts/hdf5/hdf5imagedataset.cpp``.

`The NCSA HDF5 Download
Page <http://hdf.ncsa.uiuc.edu/HDF5/release/obtain5.html>`__ at the
`National Center for Supercomputing
Applications <http://www.ncsa.uiuc.edu/>`__

`The HDFView is a visual tool for browsing and editing NCSA HDF4 and
HDF5 files. <http://hdf.ncsa.uiuc.edu/hdf-java-html/hdfview/>`__

Documentation to individual products, supported by this driver:

-  `OMTO3: OMI/Aura Ozone (O3) Total Column 1-Orbit L2 Swath 13x24km
   V003 <https://disc.gsfc.nasa.gov/uui/datasets/OMTO3_V003/summary>`__
