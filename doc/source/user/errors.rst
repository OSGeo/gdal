==================
Error Codes
==================

Overview
--------

This page describes the GDAL error codes that may be returned by GDAL command-line tools and the Python API.
Each GDAL error code is listed below with:

* a brief description,
* examples of operations that may produce the error, and
* possible resolutions

GDAL may also report warnings, which indicate potential issues that do not prevent the operation from completing.
These use the same codes as errors. The different levels are listed at :ref:`error_handling_levels`.
See :ref:`error_handling` for details on GDAL error handling using the C and C++ APIs,
and more details on error codes and levels.

.. contents::
    :depth: 4

General Error Codes
-------------------

ERROR 1 Application Defined Error
+++++++++++++++++++++++++++++++++

**Code:** ``CPLE_AppDefined``

A generic error indicating that the application, driver, or GDAL component
reported a failure that does not match a more specific error classification.
Additional details are typically provided in the accompanying error message.

.. example::
   :title: Overwriting a GeoPackage that is in use by another application

   Although ``--overwrite`` is specified, the output file cannot be replaced
   because it is currently open in another application (for example QGIS).

   This driver-specific failure is reported as ``CPLE_AppDefined``.

   .. code-block:: console

        $ gdal vector buffer --distance=5 in.gpkg out.gpkg --overwrite
        ERROR 1: A file system object called 'out.gpkg' already exists.
        ERROR 1: GPKG driver failed to create out.gpkg

.. example::
   :title: Creating GeoTIFFs larger than 4 GB

   This error occurs when writing a TIFF that exceeds the
   standard 4 GB file size limit. To resolve it, enable the BigTIFF
   format using the ``BIGTIFF=YES`` creation option, as indicated in
   the error message.

   .. code-block:: console

      $ gdal raster mosaic --output-format COG *.tif out.tif
      ERROR 1: TIFFAppendToStrip: Maximum TIFF file size exceeded. Use BIGTIFF=YES creation option.

ERROR 2 Out of Memory Error
+++++++++++++++++++++++++++

**Code:** ``CPLE_OutOfMemory``

An error indicating that GDAL or a driver was unable to allocate the memory
required to complete the operation. This typically occurs when processing large
datasets, performing memory-intensive transformations, or when system memory
limits have been reached.

ERROR 3 File I/O Error
++++++++++++++++++++++

**Code:** ``CPLE_FileIO``

An error indicating a failure while reading from or writing to a file or other
data source. This may occur due to issues such as missing files, permission
problems, corrupted storage, or interruptions during disk operations.

.. example::
   :title: Not enough disk space

   In this example, there is not enough free space available on the disk to
   store the output dataset. Freeing up disk space will resolve the error.

   Alternatively, using a smaller output format can reduce disk usage. For
   example, ``--output-format COG`` typically produces significantly smaller
   files.

   Finally, as suggested in the error message, the :ref:`raster.gtiff`
   creation option ``CHECK_DISK_FREE_SPACE=FALSE`` can be used to disable
   this check if the available disk space is being reported incorrectly.

   .. code-block:: console

      $ gdal raster calc -i "A=a.tif" -i "B=b.tif" --calc "A - 0.3*B" --nodata=-9999 -o c.tif
      ERROR 3: dem_burned.tif: Free disk space available is 1.62 GB, whereas 46.57 GB are at least necessary.
      You can disable this check by defining the CHECK_DISK_FREE_SPACE configuration option to FALSE.

ERROR 4 Open Failed
+++++++++++++++++++

**Code:** ``CPLE_OpenFailed``

An error indicating that GDAL was unable to open a dataset or file. This may
occur if the file does not exist, is inaccessible due to permissions, is
corrupted, or is not recognised as a supported format by any available driver.

To check if a driver is installed, see :example:`gdal-vector-drivers`.

.. example::
   :title: Opening a NetCDF file

   GDAL supports :ref:`raster.netcdf`, but it requires the NetCDF driver plugin
   to be available in the installation.

   As indicated in the error message, this can be installed in a
   :ref:`conda`-based GDAL environment using ``conda install``.

   .. code-block:: console

      $ gdal vector info trmm-2x2.nc
      ERROR 4: `trmm-2x2.nc` not recognized as being in a supported file format.
      It could have been recognized by driver netCDF, but plugin gdal_netCDF.dll is not available in your installation.
      You may install it with 'conda install -c conda-forge libgdal-netcdf'

.. example::
   :title: Opening a zipped Shapefile

   Even though the ESRI Shapefile driver supports compressed archives
   (``*.zip``), attempting to open a ZIP file directly may result in an error.

   In this case, the dataset is stored inside a ZIP archive. GDAL requires the
   :ref:`vsizip` virtual filesystem prefix to access its contents.

   .. code-block:: console

      $ gdal vector info C:\Data\in.zip
      ERROR 4: 'C:\Data\in.zip' not recognized as being in a supported file format.
      Changing the filename to /vsizip/C:\Data\in.zip may help it to be recognized.

      $ gdal vector info "/vsizip/C:\Data\in.zip"
      INFO: Open of '/vsizip/C:\Data\in.zip'
        using driver 'ESRI Shapefile' successful.

ERROR 5 Illegal Argument
++++++++++++++++++++++++

**Code:** ``CPLE_IllegalArg``

An invalid or unsupported argument was passed to a GDAL function or command.

.. example::
   :title: Using an unknown command-line option

   The ``gdal raster reproject`` command does not support the ``--crs`` option.
   Use ``--output-crs`` instead. Available options can be listed using ``--help``
   or by checking the documentation for :ref:`gdal_raster_reproject`.

   .. code-block:: console

        $ gdal raster reproject --crs=EPSG:32632 in.tif out.tif --overwrite
        ERROR 5: reproject: Option '--crs' is unknown.
        Usage: gdal raster reproject [OPTIONS] <INPUT> <OUTPUT>
        Try 'gdal raster reproject --help' for help.

ERROR 6 Not Supported
+++++++++++++++++++++

**Code:** ``CPLE_NotSupported``

The requested operation is not supported by the driver, dataset, or GDAL component.

ERROR 7 Assertion Failed
++++++++++++++++++++++++

**Code:** ``CPLE_AssertionFailed``

An internal GDAL assertion failed, typically indicating a programming or logic error.

ERROR 8 No Write Access
+++++++++++++++++++++++

**Code:** ``CPLE_NoWriteAccess``

Write access to the dataset, file, or resource was denied.

ERROR 9 User Interrupted
++++++++++++++++++++++++

**Code:** ``CPLE_UserInterrupt``

The operation was cancelled or interrupted by the user.

Network Related Error Codes
---------------------------

ERROR 10 HTTP Response
++++++++++++++++++++++

**Code:** ``CPLE_HttpResponse``

The server returned an error response to an HTTP request. This typically
occurs when accessing remote datasets over HTTP or HTTPS.

ERROR 11 Bucket Not Found
+++++++++++++++++++++++++

**Code:** ``CPLE_BucketNotFound``

The requested object storage bucket could not be found.

``ERROR 11: HTTP response code: 404``

ERROR 12 Object Not Found
+++++++++++++++++++++++++

**Code:** ``CPLE_ObjectNotFound``

The requested object (such as a file or resource) does not exist in the
remote storage system.

ERROR 13 Access Denied
++++++++++++++++++++++

**Code:** ``CPLE_AccessDenied``

Access to the requested resource was denied by the remote storage system.

ERROR 14 Invalid Credentials
++++++++++++++++++++++++++++

**Code:** ``CPLE_InvalidCredentials``

Authentication failed due to invalid or missing credentials.

ERROR 15 Signature Does Not Match
+++++++++++++++++++++++++++++++++

**Code:** ``CPLE_SignatureDoesNotMatch``

The provided authentication signature does not match the expected value.

ERROR 16 Object Storage Generic Error
+++++++++++++++++++++++++++++++++++++

**Code:** ``CPLE_ObjectStorageGenericError``

A generic object storage error that does not fit a more specific error
category.
