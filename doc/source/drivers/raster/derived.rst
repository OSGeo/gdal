.. _raster.derived:

================================================================================
DERIVED -- Derived subdatasets driver
================================================================================

.. shortname:: DERIVED

.. built_in_by_default::

This driver allows accessing subdatasets derived from a given
dataset. Those derived datasets have the same projection reference,
geo-transform and metadata than the original dataset, but derives new
pixel values using gdal pixel functions.

Available functions
-------------------

Available derived datasets are:

-  AMPLITUDE: Amplitude of pixels from input bands
-  PHASE: Phase of pixels from input bands
-  REAL: Real part of pixels from input bands
-  IMAG: Imaginary part of pixels from input bands
-  CONJ: Conjugate of pixels from input bands
-  INTENSITY: Intensity (squared amplitude) of pixels from input bands
-  LOGAMPLITUDE: Log10 of amplitude of pixels from input bands

Note: for non-complex data types, only LOGAMPLITUDE will be listed.

A typical use is to directly access amplitude, phase or log-amplitude of
any complex dataset.

Accessing derived subdatasets
-----------------------------

Derived subdatasets are stored in the DERIVED_SUBDATASETS metadata
domain, and can be accessed using the following syntax:

::

     DERIVED_SUBDATASET:FUNCTION:dataset_name

where function is one of AMPLITUDE, PHASE, REAL, IMAG, CONJ, INTENSITY,
LOGAMPLITUDE. So as to ensure numerical precision, all derived
subdatasets bands will have Float64 or CFloat64 precision (depending on
the function used).

For instance:

::

     $ gdalinfo cint_sar.tif

::

   Driver: GTiff/GeoTIFF
   Files: cint_sar.tif
   Size is 5, 6
   Coordinate System is `'
   GCP Projection =
   GEOGCS["WGS 84",
       DATUM["WGS_1984",
           SPHEROID["WGS 84",6378137,298.257223563,
               AUTHORITY["EPSG","7030"]],
           AUTHORITY["EPSG","6326"]],
       PRIMEM["Greenwich",0],
       UNIT["degree",0.0174532925199433],
       AUTHORITY["EPSG","4326"]]
   GCP[  0]: Id=1, Info=
             (-1910.5,-7430.5) -> (297.507,16.368,0)
   GCP[  1]: Id=2, Info=
             (588.5,-7430.5) -> (297.938,16.455,0)
   GCP[  2]: Id=3, Info=
             (588.5,7363.5) -> (297.824,16.977,0)
   GCP[  3]: Id=4, Info=
             (-1910.5,7363.5) -> (297.393,16.89,0)
   Metadata:
     AREA_OR_POINT=Area
     CEOS_ACQUISITION_TIME=19970718024119087
     CEOS_ELLIPSOID=GEM6
     CEOS_INC_ANGLE=24.824
     CEOS_LINE_SPACING_METERS=3.9900000
     CEOS_LOGICAL_VOLUME_ID=0001667400297672
     CEOS_PIXEL_SPACING_METERS=7.9040000
     CEOS_PIXEL_TIME_DIR=INCREASE
     CEOS_PLATFORM_HEADING=347.339
     CEOS_PLATFORM_LATITUDE=16.213
     CEOS_PLATFORM_LONGITUDE=-65.311
     CEOS_PROCESSING_AGENCY=ESA
     CEOS_PROCESSING_COUNTRY=ITALY
     CEOS_PROCESSING_FACILITY=ES
     CEOS_SEMI_MAJOR=6378.1440000
     CEOS_SEMI_MINOR=6356.7590000
     CEOS_SENSOR_CLOCK_ANGLE=90.000
     CEOS_SOFTWARE_ID=ERS2-SLC-6.1
     CEOS_TRUE_HEADING=345.5885834
   Image Structure Metadata:
     INTERLEAVE=BAND
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,    6.0)
   Upper Right (    5.0,    0.0)
   Lower Right (    5.0,    6.0)
   Center      (    2.5,    3.0)
   Band 1 Block=5x6 Type=CInt16, ColorInterp=Gray

::

     $ gdalinfo DERIVED_SUBDATASET:LOGAMPLITUDE:cint_sar.tif

::

   Driver: DERIVED/Derived datasets using VRT pixel functions
   Files: cint_sar.tif
   Size is 5, 6
   Coordinate System is `'
   GCP Projection =
   GEOGCS["WGS 84",
       DATUM["WGS_1984",
           SPHEROID["WGS 84",6378137,298.257223563,
               AUTHORITY["EPSG","7030"]],
           AUTHORITY["EPSG","6326"]],
       PRIMEM["Greenwich",0],
       UNIT["degree",0.0174532925199433],
       AUTHORITY["EPSG","4326"]]
   GCP[  0]: Id=1, Info=
             (-1910.5,-7430.5) -> (297.507,16.368,0)
   GCP[  1]: Id=2, Info=
             (588.5,-7430.5) -> (297.938,16.455,0)
   GCP[  2]: Id=3, Info=
             (588.5,7363.5) -> (297.824,16.977,0)
   GCP[  3]: Id=4, Info=
             (-1910.5,7363.5) -> (297.393,16.89,0)
   Metadata:
     AREA_OR_POINT=Area
     CEOS_ACQUISITION_TIME=19970718024119087
     CEOS_ELLIPSOID=GEM6
     CEOS_INC_ANGLE=24.824
     CEOS_LINE_SPACING_METERS=3.9900000
     CEOS_LOGICAL_VOLUME_ID=0001667400297672
     CEOS_PIXEL_SPACING_METERS=7.9040000
     CEOS_PIXEL_TIME_DIR=INCREASE
     CEOS_PLATFORM_HEADING=347.339
     CEOS_PLATFORM_LATITUDE=16.213
     CEOS_PLATFORM_LONGITUDE=-65.311
     CEOS_PROCESSING_AGENCY=ESA
     CEOS_PROCESSING_COUNTRY=ITALY
     CEOS_PROCESSING_FACILITY=ES
     CEOS_SEMI_MAJOR=6378.1440000
     CEOS_SEMI_MINOR=6356.7590000
     CEOS_SENSOR_CLOCK_ANGLE=90.000
     CEOS_SOFTWARE_ID=ERS2-SLC-6.1
     CEOS_TRUE_HEADING=345.5885834
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,    6.0)
   Upper Right (    5.0,    0.0)
   Lower Right (    5.0,    6.0)
   Center      (    2.5,    3.0)
   Band 1 Block=5x6 Type=Float64, ColorInterp=Undefined

Listing available subdatasets
-----------------------------

Available subdatasets are reported in the DERIVED_SUBDATASETS metadata
domain. Only functions that make sense will be reported for a given
dataset, which means that AMPLITUDE, PHASE, REAL, IMAG, CONJ and
INTENSITY will only be reported if the dataset has at least one complex
band. Nevertheless, even if not reported, those derived datasets are
still reachable with the syntax presented above.

::

       $ gdalinfo -mdd DERIVED_SUBDATASETS cint_sar.tif


::

   Driver: GTiff/GeoTIFF
   Files: cint_sar.tif
   Size is 5, 6
   Coordinate System is `'
   GCP Projection =
   GEOGCS["WGS 84",
       DATUM["WGS_1984",
           SPHEROID["WGS 84",6378137,298.257223563,
               AUTHORITY["EPSG","7030"]],
           AUTHORITY["EPSG","6326"]],
       PRIMEM["Greenwich",0],
       UNIT["degree",0.0174532925199433],
       AUTHORITY["EPSG","4326"]]
   GCP[  0]: Id=1, Info=
             (-1910.5,-7430.5) -> (297.507,16.368,0)
   GCP[  1]: Id=2, Info=
             (588.5,-7430.5) -> (297.938,16.455,0)
   GCP[  2]: Id=3, Info=
             (588.5,7363.5) -> (297.824,16.977,0)
   GCP[  3]: Id=4, Info=
             (-1910.5,7363.5) -> (297.393,16.89,0)
   Metadata:
     AREA_OR_POINT=Area
     CEOS_ACQUISITION_TIME=19970718024119087
     CEOS_ELLIPSOID=GEM6
     CEOS_INC_ANGLE=24.824
     CEOS_LINE_SPACING_METERS=3.9900000
     CEOS_LOGICAL_VOLUME_ID=0001667400297672
     CEOS_PIXEL_SPACING_METERS=7.9040000
     CEOS_PIXEL_TIME_DIR=INCREASE
     CEOS_PLATFORM_HEADING=347.339
     CEOS_PLATFORM_LATITUDE=16.213
     CEOS_PLATFORM_LONGITUDE=-65.311
     CEOS_PROCESSING_AGENCY=ESA
     CEOS_PROCESSING_COUNTRY=ITALY
     CEOS_PROCESSING_FACILITY=ES
     CEOS_SEMI_MAJOR=6378.1440000
     CEOS_SEMI_MINOR=6356.7590000
     CEOS_SENSOR_CLOCK_ANGLE=90.000
     CEOS_SOFTWARE_ID=ERS2-SLC-6.1
     CEOS_TRUE_HEADING=345.5885834
   Metadata (DERIVED_SUBDATASETS):
     DERIVED_SUBDATASET_1_NAME=DERIVED_SUBDATASET:AMPLITUDE:cint_sar.tif
     DERIVED_SUBDATASET_1_DESC=Amplitude of input bands from cint_sar.tif
     DERIVED_SUBDATASET_2_NAME=DERIVED_SUBDATASET:PHASE:cint_sar.tif
     DERIVED_SUBDATASET_2_DESC=Phase of input bands from cint_sar.tif
     DERIVED_SUBDATASET_3_NAME=DERIVED_SUBDATASET:REAL:cint_sar.tif
     DERIVED_SUBDATASET_3_DESC=Real part of input bands from cint_sar.tif
     DERIVED_SUBDATASET_4_NAME=DERIVED_SUBDATASET:IMAG:cint_sar.tif
     DERIVED_SUBDATASET_4_DESC=Imaginary part of input bands from cint_sar.tif
     DERIVED_SUBDATASET_5_NAME=DERIVED_SUBDATASET:CONJ:cint_sar.tif
     DERIVED_SUBDATASET_5_DESC=Conjugate of input bands from cint_sar.tif
     DERIVED_SUBDATASET_6_NAME=DERIVED_SUBDATASET:INTENSITY:cint_sar.tif
     DERIVED_SUBDATASET_6_DESC=Intensity (squared amplitude) of input bands from cint_sar.tif
     DERIVED_SUBDATASET_7_NAME=DERIVED_SUBDATASET:LOGAMPLITUDE:cint_sar.tif
     DERIVED_SUBDATASET_7_DESC=log10 of amplitude of input bands from cint_sar.tif
   Image Structure Metadata:
     INTERLEAVE=BAND
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,    6.0)
   Upper Right (    5.0,    0.0)
   Lower Right (    5.0,    6.0)
   Center      (    2.5,    3.0)
   Band 1 Block=5x6 Type=CInt16, ColorInterp=Gray

See Also:
---------

-  :ref:`Using Derived Bands part of the GDAL VRT tutorial <vrt_derived_bands>`
