.. _Raster.MRR:

MRR --- Multi Resolution Raster
===============================

.. shortname:: MRR

This driver supports reading of MRR (Multiple Resolution Raster) files using the MapInfo Pro Advanced SDK, developed by Pitney Bowes. This driver does not provide support for creating, writing or editing MRR files. 

Contents
--------

#. `Overview of MRR Driver <#driver_overview_mrr>`__
#. `Issues and Limitations <#issues_mrr>`__
#. `Building GDAL <#building_gdal_mrr>`__


--------------------------------

.. _driver_overview_mrr:

Overview of MRR (Multiple Resolution Raster) Driver
---------------------------------------------------

MRR unifies the storage of all types of raster data such as imagery, spectral imagery, continuous gridded data and thematic data. MRR extends the concept of a multi-banded raster to a “four dimensional” raster which may contain –

-  One or more fields, each of which contain a particular type of raster data. A field may contain multi-banded continuous grid data, multi-banded classified data, color imagery or color imagery utilising a fixed size color palette.
-  One or more events, each of which contain an addition or modification to the field data at a specified time. Events provide a time dimension in MRR.
-  One or more bands which contain data in one of many supported data types such as 1/2/4/8/16/32/64 bit integers and 32/64 bit floating point. Some data types, like color or complex numbers, may contain multiple components. These are exposed as virtual bands.
-  A stack of overviews, referred to as resolutions levels. Level 0 contains the ‘base resolution’ raster data. Levels 1 upward contain overviews within which the cell size doubles at each level. Levels -1 downward contain underviews which are generated on demand by interpolation from the base level.
-  MRR is designed to enable the creation of very large and high resolution rasters and the SDK ensures that access to data at any resolution level is constant. Raster data is stored in a sparse collection of tiles of equal size. Lossless compression codecs are employed to store data within each tile, and lossy image compression codec can be used for imagery fields. Each resolution level has a fixed cell size, but MRR supports a multi-resolution tile concept which allows the cell size to set in each tile separately.

NOTE: Some MRR features may not be accesible through GDAL driver.

The MRR driver for GDAL is supported on 64-bit Windows, Ubuntu, AmazonLinux, OracleLinux and CentOS.

--------------

.. _issues_mrr:

Issues and Limitations
----------------------

-  The driver does not provide support for creating, writing or editing MRR files.
-  Although an MRR may contain multiple fields, this driver can only access the first field.
-  Although an MRR may contain multiple events, this driver can only access data that represents the roll-up of all events. This represents the “final state” of the raster.
-  Multi resolution tiles creation is not supported, however such tile if already exsited in MRR they could be read.

--------------

.. _building_gdal_mrr:

Building GDAL
-------------


The MRR driver will build as part of the GDAL even if the MapInfo Pro Advanced SDK is
not found or not on the machine. This will allow GDAL to always be
MRR-ready. 

::

   nmake -f makefile.vc MSVC_VER=1900 DEBUG=1 WIN64=YES

| **NOTE:** vcvars must have already been run - if using a VC comamnd
  prompt this will be automatic but will cause issues with 32-bit
  builds.
  
MapInfo Pro Advanced SDK (Runtime)
----------------------------------
  
The MRR driver needs the MapInfo Pro Advanced SDK to be installed on the machine to work with GDAL.

Download the MapInfo Pro Advanced SDK by navigating to "https://www.pitneybowes.com/us/campaign/sdkrequest.html"
and fill up the SDK request form to receive the download link on your e-mail.
Once the SDK package is downloaded, unzip the package on your machine at the desired location.

Structure
---------

The MapInfo Pro Advanced SDK zip file contains the following folders:

-  AmazonLinux --> 64 bit Binaries for AmazonLinux.
   - ColorPalettes --> color palettes.
-  CentOS7 --> 64 bit Binaries for CentOS7.
   - ColorPalettes --> color palettes.
-  OracleLinux --> 64 bit Binaries for OracleLinux.
   - ColorPalettes --> color palettes.
-  Ubuntu --> 64 bit Binaries for Ubuntu.
   - ColorPalettes --> color palettes.
-  Windows --> 64 bit Binaries for Windows.
   

Choose the binaries for the desired platform from the "Raster GDAL" folder and copy all files into the folder containing GDAL binaries.

