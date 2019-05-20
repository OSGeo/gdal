.. _raster.sde:

================================================================================
ESRI ArcSDE Raster
================================================================================

.. shortname:: SDE

ESRI ArcSDE provides an abstraction layer over numerous databases that
allows the storage of raster data. ArcSDE supports n-band imagery at
many bit depths, and the current implementation of the GDAL driver
should support as many bands as you can throw at it. ArcSDE supports the
storage of LZW, JP2K, and uncompressed data and transparently presents
this through its C API SDK.

Driver capabilities
-------------------

.. supports_georeferencing::

GDAL ArcSDE Raster driver features
----------------------------------

The current driver **supports** the following features:

-  **Read** support only.
-  GeoTransform information for rasters that have defined it.
-  Coordinate reference information.
-  Color interpretation (palette for datasets with colormaps, greyscale
   otherwise).
-  Band statistics if ArcSDE has cached them, otherwise GDAL will
   calculate.
-  ArcSDE overview (pyramid) support
-  1 bit, 4 bit, 8 bit, 16 bit, and 32 bit data
-  IReadBlock support that maps to ArcSDE's representation of the data
   in the database.
-  ArcSDE 9.1 and 9.2 SDK's. Older versions may also work, but they have
   not been tested.

The current driver **does not support** the following:

-  **Writing** GDAL datasets into the database.
-  Large, fast, single-pass reads from the database.
-  Reading from "ESRI ArcSDE Raster Catalogs."
-  NODATA masks.

Performance considerations
--------------------------

The ArcSDE raster driver currently only supports block read methods.
Each call to this method results in a request for a block of raster data
for **each** band of data in the raster, and single-pass requests for
all of the bands for a block or given area is not currently done. This
approach consequently results in extra network overhead. It is hoped
that the driver will be improved to support single-pass reads in the
near future.

The ArcSDE raster driver should only consume a single ArcSDE connection
throughout the lifespan of the dataset. Each connection to the database
has an overhead of approximately 2 seconds, with additional overhead
that is taken for calculating dataset information. Therefore, usage of
the driver in situations where there is a lot of opening and closing of
datasets is not expected to be very performant.

Although the ArcSDE C SDK does support threading and locking, the GDAL
ArcSDE raster driver does not utilize this capability. Therefore, the
ArcSDE raster driver should be considered **not threadsafe**, and
sharing datasets between threads will have undefined (and often
disastrous) results.

Dataset specification
---------------------

SDE datasets are specified with the following information:

::

    SDE:sdemachine.iastate.edu,5151,database,username,password,fully.specified.tablename,RASTER

-  **SDE:** -- this is the prefix that tips off GDAL to use the SDE
   driver.
-  **sdemachine.iastate.edu** -- the DNS name or IP address of the
   server we are connecting to.
-  **5151** -- the port number (5151 or port:5151) or service entry
   (typically *esri_sde*).
-  **database** -- the database to connect to. This can also be blank
   and specified as ,,.
-  **username** -- username.
-  **password** -- password.
-  **fully.specified.tablename** -- It is prudent to use a fully
   specified tablename wherever possible, although it is not absolutely
   required.
-  **RASTER** -- Optional raster column name.
