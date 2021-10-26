.. _raster.safe:

================================================================================
SAFE -- Sentinel-1 SAFE XML Product
================================================================================

.. shortname:: SAFE

.. built_in_by_default::

Driver for Sentinel products. Currently supports only Sentinel-1 SAR
products. See also the :ref:`GDAL Sentinel-2 driver <raster.sentinel2>`

SENTINEL data products are distributed using a SENTINEL-specific
variation of the Standard Archive Format for Europe (SAFE) format
specification. The SAFE format has been designed to act as a common
format for archiving and conveying data within ESA Earth Observation
archiving facilities.

The SAFE driver will be used if the manifest.safe or the containing
directory is selected, and it can treat all the imagery as one
consistent dataset.

The SAFE driver also reads geolocation grid points from the metadata and
represents them as GCPs on the dataset.

ESA will be distributing other satellite datasets in this format;
however, at this time this driver only supports specific Sentinel-1 SAR
products. All other will be ignored, or result in various runtime
errors.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Multiple measurements
---------------------

If the product contains multiple measurements (for example multiple
polarizations), each one is available as a raster band - if the swath is
the same. When the swath is the same, the geographic area is the same.

If the product contains multiple swaths and multiple polatizations, the
driver shows the first swath by default. To access other swaths, the
user must select a specific subdataset.

The syntax of subdataset naming and their content has been significantly
change in GDAL 3.4.

Examples
--------

-  Opening the Sentinel-1 product:

   ::

      $ gdalinfo S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE/manifest.safe

   ::

      Driver: SAFE/Sentinel-1 SAR SAFE Product
      Files: S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE/manifest.safe
             S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE/measurement/s1a-iw-grd-vh-20150705t064241-20150705t064306-006672-008ea0-002.tiff
             S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE/measurement/s1a-iw-grd-vv-20150705t064241-20150705t064306-006672-008ea0-001.tiff
      Size is 256, 167
      Coordinate System is `'
      GCP Projection =
      GEOGCS["WGS 84",
          DATUM["WGS_1984",
              SPHEROID["WGS 84",6378137,298.257223563,
                  AUTHORITY["EPSG","7030"]],
              AUTHORITY["EPSG","6326"]],
          PRIMEM["Greenwich",0,
              AUTHORITY["EPSG","8901"]],
          UNIT["degree",0.0174532925199433,
              AUTHORITY["EPSG","9122"]],
          AUTHORITY["EPSG","4326"]]
      GCP[  0]: Id=1, Info=
                (0,0) -> (-8.03500070209827,39.6332161725022,141.853266630322)
      Metadata:
        ACQUISITION_START_TIME=2015-07-05T06:42:41.504840
        ACQUISITION_STOP_TIME=2015-07-05T06:43:06.503530
        BEAM_MODE=IW
        BEAM_SWATH=IW
        FACILITY_IDENTIFIER=UPA_
        LINE_SPACING=1.000655e+01
        MISSION_ID=S1A
        MODE=IW
        ORBIT_DIRECTION=DESCENDING
        ORBIT_NUMBER=6672
        PIXEL_SPACING=1.000000e+01
        PRODUCT_TYPE=GRD
        SATELLITE_IDENTIFIER=SENTINEL-1
        SENSOR_IDENTIFIER=SAR
        SWATH=IW
      Subdatasets:
        SUBDATASET_1_NAME=SENTINEL1_DS:S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE:IW_VH
        SUBDATASET_1_DESC=Single band with IW swath and VH polarization
        SUBDATASET_2_NAME=SENTINEL1_DS:S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE:IW_VV
        SUBDATASET_2_DESC=Single band with IW swath and VV polarization
        SUBDATASET_3_NAME=SENTINEL1_DS:S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE:IW
        SUBDATASET_3_DESC=IW swath with all polarizations as bands
      Corner Coordinates:
      Upper Left  (    0.0,    0.0)
      Lower Left  (    0.0,  167.0)
      Upper Right (  256.0,    0.0)
      Lower Right (  256.0,  167.0)
      Center      (  128.0,   83.5)
      Band 1 Block=256x16 Type=UInt16, ColorInterp=Undefined
        Metadata:
          POLARISATION=VH
          SWATH=IW
      Band 2 Block=256x16 Type=UInt16, ColorInterp=Undefined
        Metadata:
          POLARISATION=VV
          SWATH=IW

-  It's not mandatory to open manifest.safe, just pass the folder name:

   ::

      $ gdalinfo S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE

-  Opening a single measurement (for example IW/VH):

   ::

      $ gdalinfo SENTINEL1_DS:S1A_IW_GRDH_1SDV_20150705T064241_20150705T064306_006672_008EA0_24EE.SAFE:IW_VV


   or starting with GDAL 3.4

   ::

      $ gdalinfo SENTINEL1_CALIB:UNCALIB:test.SAFE:IW_VV:AMPLITUDE

Data Calibration
----------------

Starting with GDAL 3.4, calibration is applied for SIGMA0, BETA0 and GAMMA calibration subdataset

See Also
--------

-  `SAR Formats (ESA Sentinel
   Online) <https://sentinel.esa.int/web/sentinel/user-guides/sentinel-1-sar/data-formats/sar-formats>`__
-  `SAFE Specification (ESA Sentinel
   Online) <https://sentinel.esa.int/web/sentinel/user-guides/sentinel-1-sar/data-formats/safe-specification>`__
-  :ref:`GDAL Sentinel-2 driver <raster.sentinel2>`
