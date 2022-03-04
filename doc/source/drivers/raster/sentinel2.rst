.. _raster.sentinel2:

================================================================================
SENTINEL2 -- Sentinel-2 Products
================================================================================

.. shortname:: SENTINEL2

.. built_in_by_default::

Driver for Sentinel-2 Level-1B, Level-1C and Level-2A products.
Starting with GDAL 2.1.3, Level-1C with "Safe Compact" encoding
are also supported.

The SENTINEL2 driver will be used if the main metadata .xml file at the
root of a SENTINEL2 data product is opened (whose name is typically
S2A_OPER_MTD_SAFL1C\_....xml). It can also accept directly .zip files
downloaded from the `Sentinels Scientific Data
Hub <https://scihub.copernicus.eu/>`__

To be able to read the imagery, GDAL must be configured with at least
one of the JPEG2000 capable drivers.

SENTINEL-2 data are acquired on 13 spectral bands in the visible and
near-infrared (VNIR) and Short-wavelength infrared (SWIR) spectrum, as
show in the below table:

========= ============== ======================= =============== =================================
Band name Resolution (m) Central wavelength (nm) Band width (nm) Purpose
========= ============== ======================= =============== =================================
B01       60             443                     20              Aerosol detection
B02       10             490                     65              Blue
B03       10             560                     35              Green
B04       10             665                     30              Red
B05       20             705                     15              Vegetation classification
B06       20             740                     15              Vegetation classification
B07       20             783                     20              Vegetation classification
B08       10             842                     115             Near infrared
B08A      20             865                     20              Vegetation classification
B09       60             945                     20              Water vapour
B10       60             1375                    30              Cirrus
B11       20             1610                    90              Snow / ice / cloud discrimination
B12       20             2190                    180             Snow / ice / cloud discrimination
========= ============== ======================= =============== =================================

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Level-1B
--------

Level-1B products are composed of several "granules" of ~ 25 km
across-track x ~ 23km along-track, in sensor geometry (i.e. non
ortho-rectified). Each granule correspond to the imagery captured by one
of the 12 detectors across-track (for a total 290 km swath width). The
imagery of each band is put in a separate JPEG2000 file.

Level-1B products are aimed at advanced users.

When opening the main metadata .xml file, the driver will typically
expose N \* 3 sub-datasets, where N is the number of granules composing
the user product, and 3 corresponds to the number of spatial
resolutions. There's one for the 4 10m bands, one for the 6 20m bands
and one for the 3 60m bands. Caution: the number of such subdatasets can
be typically of several hundreds or more.

It is also possible to open the metadata .xml of a given granule, in
which case 3 subdatasets will be reported for each of the 3 spatial
resolutions.

When opening a subdataset, the georeferencing is made of 5 ground
control points for the 4 corner of the images and the center of image.

Level-1C
--------

Level-1C products are organized in ortho-rectified tiles of 100 km x 100
km in UTM WGS84 projections. The imagery of each band is put in a
separate JPEG2000 file.

When opening the main metadata .xml file, the driver will typically
expose 4 sub-datasets:

-  one for the 4 10m bands,
-  one for the 6 20m bands,
-  one for the 3 60m bands and,
-  one for a preview of the R,G,B bands at a 320m resolution

All tiles of same resolution and projection are mosaiced together. If a
product spans over several UTM zones, they will be exposed as separate
subdatasets.

It is also possible to open the metadata .xml file of each tile (only
for original L1C encoding, not supported on "Safe Compact" encoding), in
which case the driver will typically expose the 4 above mentioned types
of sub-datasets.

Level-2A
--------

Similarly to Level-1C, Level-2A products are organized in
ortho-rectified tiles of 100 km x 100 km in UTM WGS84 projections. The
imagery of each band is put in a separate JPEG2000 file. The values are
Bottom-Of-Atmosphere (BOA) reflectances. L2A specific bands are also
computed:

-  AOT: Aerosol Optical Thickness map (at 550nm)
-  CLD: Raster mask values range from 0 for high confidence clear sky to
   100 for high confidence cloudy
-  SCL: Scene Classification. The meaning of the values is indicated in
   the Category Names of the band.
-  SNW: Raster mask values range from 0 for high confidence NO snow/ice
   to 100 for high confidence snow/ice
-  WVP: Scene-average Water Vapour map

When opening the main metadata .xml file, the driver will typically
expose 4 sub-datasets:

-  one for the 4 native 10m bands, and L2A specific bands (AOT and WVP)
-  one for the 6 native 20m bands, plus the 10m bands, except B8,
   resampled to 20m, and L2A specific bands (AOT, WVP, SCL, CLD and
   SNW),
-  one for the 3 native 60m bands, plus the 10m&20m bands, except B8,
   resampled to 60m, and L2A specific bands (AOT, WVP, SCL, CLD and
   SNW),
-  one for a preview of the R,G,B bands at a 320m resolution

All tiles of same resolution and projection are mosaiced together. If a
product spans over several UTM zones, they will be exposed as separate
subdatasets.

Metadata
--------

Metadata of the main metadata .xml file is available in the general
metadata domain. The whole XML file is also accessible through the
xml:SENTINEL2 metadata domain.

Subdatasets are based on the VRT format, so the definition of this VRT
can be obtained by querying the xml:VRT metadata domain.

Performance issues for L1C and L2A
----------------------------------

Due to the way Sentinel-2 products are structured, in particular because
of the number of JPEG2000 files involved, zoom-out operations can be
very slow for products made of many tiles. For interactive display, it
can be useful to generate overviews (can be a slow operation by itself).
This can be done with the gdaladdo utility on the subdataset name. The
overview file is created next to the main metadata .xml file, with the
same name, but prefixed with \_XX_EPSG_YYYYY.tif.ovr where
XX=10m,20m,60m or PREVIEW and YYYYY is the EPSG code.

Trick: if the content of the zoom-out preview is not important for the
use case, blank overviews can be created instantaneously by using the
NONE resampling method ('-r none' as gdaladdo switch).

When converting a subdataset to another format like tiled GeoTIFF, if
using the JP2OpenJPEG driver, the recommended minimum value for the
:decl_configoption:`GDAL_CACHEMAX` configuration option is (subdataset_width \* 2048 \* 2 ) /
10000000 if generating a INTERLEAVE=BAND GeoTIFF, or that value
multiplied by the number of bands for the default INTERLEAVE=PIXEL
configuration. The current versions of the OpenJPEG libraries can also
consume a lot of memory to decode a JPEG2000 tile (up to 600MB), so you
might want to specify the :decl_configoption:`GDAL_NUM_THREADS` configuration option to a
reasonable number of threads if you are short of memory (the default
value is the total number of virtual CPUs).

Open options
------------

The driver can be passed the following open options:

-  **ALPHA**\ =YES/NO: whether to expose an alpha band. Defaults to NO.
   If set, an extra band is added after the Sentinel2 bands with an
   alpha channel. Its value are:

   -  0 on areas with no tiles, or when the tile data is set to the
      NODATA or SATURATED special values,
   -  4095 on areas with valid data.

Note: above open options can also be specified as configuration options,
by prefixing the open option name with SENTINEL2\_ (e.g.
SENTINEL2_ALPHA).

Examples
--------

-  Opening the main metadata file of a Sentinel2 product:

   ::

      $ gdalinfo S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml

   ::

      Driver: SENTINEL2/Sentinel 2
      Files: S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml
      Size is 512, 512
      Coordinate System is `'
      Metadata:
        CLOUD_COVERAGE_ASSESSMENT=0.0
        DATATAKE_1_DATATAKE_SENSING_START=2015-08-13T10:10:26.027Z
        DATATAKE_1_DATATAKE_TYPE=INS-NOBS
        DATATAKE_1_ID=GS2A_20150813T101026_000734_N01.03
        DATATAKE_1_SENSING_ORBIT_DIRECTION=DESCENDING
        DATATAKE_1_SENSING_ORBIT_NUMBER=22
        DATATAKE_1_SPACECRAFT_NAME=Sentinel-2A
        DEGRADED_ANC_DATA_PERCENTAGE=0
        DEGRADED_MSI_DATA_PERCENTAGE=0
        FOOTPRINT=POLYGON((11.583573986577191 46.02490454425771, 11.538730738326866 45.03757398414644, 12.93007028286133 44.99812645604949, 12.999359413660665 45.98408391203724, 11.583573986577191 46.02490454425771, 11.583573986577191 46.02490454425771))
        FORMAT_CORRECTNESS_FLAG=PASSED
        GENERAL_QUALITY_FLAG=PASSED
        GENERATION_TIME=2015-08-18T10:14:40.000283Z
        GEOMETRIC_QUALITY_FLAG=PASSED
        PREVIEW_GEO_INFO=BrowseImageFootprint
        PREVIEW_IMAGE_URL=https://pdmcdam2.sentinel2.eo.esa.int/s2pdgs_geoserver/geo_service.php?service=WMS&version=1.1.0&request=GetMap&layers=S2A_A000022_N0103:S2A_A000022_N0103&styles=&bbox=11.538730738326866,44.99812645604949,12.999359413660665,46.02490454425771&width=1579&height=330&srs=EPSG:4326&format=image/png&time=2015-08-13T10:24:06.0Z/2015-08-13T10:24:06.0Z
        PROCESSING_BASELINE=01.03
        PROCESSING_LEVEL=Level-1C
        PRODUCT_START_TIME=2015-08-13T10:24:06.637Z
        PRODUCT_STOP_TIME=2015-08-13T10:24:06.637Z
        PRODUCT_TYPE=S2MSI1C
        QUANTIFICATION_VALUE=1000
        RADIOMETRIC_QUALITY_FLAG=PASSED
        REFERENCE_BAND=B1
        REFLECTANCE_CONVERSION_U=0.973195961910065
        SENSOR_QUALITY_FLAG=PASSED
        SPECIAL_VALUE_NODATA=1
        SPECIAL_VALUE_SATURATED=0
      Subdatasets:
        SUBDATASET_1_NAME=SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:10m:EPSG_32632
        SUBDATASET_1_DESC=Bands B2, B3, B4, B8 with 10m resolution, UTM 32N
        SUBDATASET_2_NAME=SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:20m:EPSG_32632
        SUBDATASET_2_DESC=Bands B5, B6, B7, B8A, B11, B12 with 20m resolution, UTM 32N
        SUBDATASET_3_NAME=SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:60m:EPSG_32632
        SUBDATASET_3_DESC=Bands B1, B9, B10 with 60m resolution, UTM 32N
        SUBDATASET_4_NAME=SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:PREVIEW:EPSG_32632
        SUBDATASET_4_DESC=RGB preview, UTM 32N
      Corner Coordinates:
      Upper Left  (    0.0,    0.0)
      Lower Left  (    0.0,  512.0)
      Upper Right (  512.0,    0.0)
      Lower Right (  512.0,  512.0)
      Center      (  256.0,  256.0)

-  Opening the .zip file directly:

   ::

      $ gdalinfo S2A_OPER_PRD_MSIL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.zip

-  Opening the 10 meters resolution bands of a L1C subdataset:

   ::

      $ gdalinfo SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:10m:EPSG_32632

   ::

      Driver: SENTINEL2/Sentinel 2
      Files: S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml
             ./GRANULE/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_N01.03/S2A_OPER_MTD_L1C_TL_MTI__20150813T201603_A000734_T32TQR.xml
             ./GRANULE/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_B04.jp2
             ./GRANULE/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_B03.jp2
             ./GRANULE/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_B02.jp2
             ./GRANULE/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_N01.03/IMG_DATA/S2A_OPER_MSI_L1C_TL_MTI__20150813T201603_A000734_T32TQR_B08.jp2
      Size is 10980, 10980
      Coordinate System is:
      PROJCS["WGS 84 / UTM zone 32N",
          GEOGCS["WGS 84",
              DATUM["WGS_1984",
                  SPHEROID["WGS 84",6378137,298.257223563,
                      AUTHORITY["EPSG","7030"]],
                  AUTHORITY["EPSG","6326"]],
              PRIMEM["Greenwich",0,
                  AUTHORITY["EPSG","8901"]],
              UNIT["degree",0.0174532925199433,
                  AUTHORITY["EPSG","9122"]],
              AUTHORITY["EPSG","4326"]],
          PROJECTION["Transverse_Mercator"],
          PARAMETER["latitude_of_origin",0],
          PARAMETER["central_meridian",9],
          PARAMETER["scale_factor",0.9996],
          PARAMETER["false_easting",500000],
          PARAMETER["false_northing",0],
          UNIT["metre",1,
              AUTHORITY["EPSG","9001"]],
          AXIS["Easting",EAST],
          AXIS["Northing",NORTH],
          AUTHORITY["EPSG","32632"]]
      Origin = (699960.000000000000000,5100060.000000000000000)
      Pixel Size = (10.000000000000000,-10.000000000000000)
      Metadata:
      [... same as above ...]
      Image Structure Metadata:
        COMPRESSION=JPEG2000
      Corner Coordinates:
      Upper Left  (  699960.000, 5100060.000) ( 11d35' 0.87"E, 46d 1'29.66"N)
      Lower Left  (  699960.000, 4990260.000) ( 11d32'19.43"E, 45d 2'15.27"N)
      Upper Right (  809760.000, 5100060.000) ( 12d59'57.69"E, 45d59' 2.70"N)
      Lower Right (  809760.000, 4990260.000) ( 12d55'48.25"E, 44d59'53.26"N)
      Center      (  754860.000, 5045160.000) ( 12d15'46.56"E, 45d30'48.07"N)
      Band 1 Block=128x128 Type=UInt16, ColorInterp=Red
        Description = B4, central wavelength 665 nm
        Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
        Metadata:
          BANDNAME=B4
          BANDWIDTH=30
          BANDWIDTH_UNIT=nm
          SOLAR_IRRADIANCE=1512.79
          SOLAR_IRRADIANCE_UNIT=W/m2/um
          WAVELENGTH=665
          WAVELENGTH_UNIT=nm
        Image Structure Metadata:
          NBITS=12
      Band 2 Block=128x128 Type=UInt16, ColorInterp=Green
        Description = B3, central wavelength 560 nm
      [...]
      Band 3 Block=128x128 Type=UInt16, ColorInterp=Blue
        Description = B2, central wavelength 490 nm
      [...]
      Band 4 Block=128x128 Type=UInt16, ColorInterp=Undefined
        Description = B8, central wavelength 842 nm
      [...]

-  Conversion of a L1C subdataset to tiled GeoTIFF

   ::

      $ gdal_translate SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:10m:EPSG_32632 \
                       10m.tif \
                       -co TILED=YES --config GDAL_CACHEMAX 1000 --config GDAL_NUM_THREADS 2

-  Generating blank overviews for a L1C subdataset:

   ::

      $ gdaladdo -r NONE SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:10m:EPSG_32632 4

-  Creating a VRT file from the subdataset (can be convenient to have
   the subdatasets as files):

   ::

      $ python -c "import sys; from osgeo import gdal; ds = gdal.Open(sys.argv[1]); open(sys.argv[2], 'wb').write(ds.GetMetadata('xml:VRT')[0].encode('utf-8'))" \
               SENTINEL2_L1C:S2A_OPER_MTD_SAFL1C_PDMC_20150818T101440_R022_V20150813T102406_20150813T102406.xml:10m:EPSG_32632 10m.vrt

-  Opening the 10 meters resolution bands of a L1B subdataset:

   ::

      $ gdalinfo SENTINEL2_L1B:S2A_OPER_MTD_L1B_GR_SGS__20151024T023555_S20151024T011315_D02.xml:10m

   ::

      Driver: SENTINEL2/Sentinel 2
      Files: S2A_OPER_MTD_L1B_GR_SGS__20151024T023555_S20151024T011315_D02.xml
             IMG_DATA/S2A_OPER_MSI_L1B_GR_SGS__20151024T023555_S20151024T011315_D02_B04.jp2
             IMG_DATA/S2A_OPER_MSI_L1B_GR_SGS__20151024T023555_S20151024T011315_D02_B03.jp2
             IMG_DATA/S2A_OPER_MSI_L1B_GR_SGS__20151024T023555_S20151024T011315_D02_B02.jp2
             IMG_DATA/S2A_OPER_MSI_L1B_GR_SGS__20151024T023555_S20151024T011315_D02_B08.jp2
      Size is 2552, 2304
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
      GCP[  0]: Id=, Info=
                (0,0) -> (134.635194391036,-21.4282083310724,0)
      GCP[  1]: Id=, Info=
                (0,2304) -> (134.581480136827,-21.6408640426055,0)
      GCP[  2]: Id=, Info=
                (2552,2304) -> (134.833308274251,-21.686125031254,0)
      GCP[  3]: Id=, Info=
                (2552,0) -> (134.886750925145,-21.4734274382519,0)
      GCP[  4]: Id=, Info=
                (1276,1152) -> (134.734115530986,-21.5571457404287,0)
      Metadata:
        CLOUDY_PIXEL_PERCENTAGE=0
        DATASTRIP_ID=S2A_OPER_MSI_L1B_DS_SGS__20151024T023555_S20151024T011312_N01.04
        DATATAKE_1_DATATAKE_SENSING_START=2015-10-24T01:13:12.027Z
        DATATAKE_1_DATATAKE_TYPE=INS-NOBS
        DATATAKE_1_ID=GS2A_20151024T011312_001758_N01.04
        DATATAKE_1_SENSING_ORBIT_DIRECTION=DESCENDING
        DATATAKE_1_SENSING_ORBIT_NUMBER=45
        DATATAKE_1_SPACECRAFT_NAME=Sentinel-2A
        DEGRADED_ANC_DATA_PERCENTAGE=0
        DEGRADED_MSI_DATA_PERCENTAGE=0
        DETECTOR_ID=02
        DOWNLINK_PRIORITY=NOMINAL
        FOOTPRINT=POLYGON((134.635194391036 -21.4282083310724, 134.581480136827 -21.6408640426055, 134.833308274251 -21.686125031254, 134.886750925145 -21.4734274382519, 134.635194391036 -21.4282083310724))
        FORMAT_CORRECTNESS_FLAG=PASSED
        GENERAL_QUALITY_FLAG=PASSED
        GENERATION_TIME=2015-11-12T10:55:12.000947Z
        GEOMETRIC_QUALITY_FLAG=PASSED
        GRANULE_ID=S2A_OPER_MSI_L1B_GR_SGS__20151024T023555_S20151024T011315_D02_N01.04
        PREVIEW_GEO_INFO=BrowseImageFootprint
        PREVIEW_IMAGE_URL=https://pdmcdam2.sentinel2.eo.esa.int/s2pdgs_geoserver/geo_service.php?service=WMS&version=1.1.0&request=GetMap&layers=S2A_A000045_N0104:S2A_A000045_N0104&styles=&bbox=133.512786023161,-25.3930035889714,137.184847290108,-21.385906922696&width=1579&height=330&srs=EPSG:4326&format=image/png&time=2015-10-24T01:13:15.0Z/2015-10-24T01:14:13.0Z
        PROCESSING_BASELINE=01.04
        PROCESSING_LEVEL=Level-1B
        PRODUCT_START_TIME=2015-10-24T01:13:15.497656Z
        PRODUCT_STOP_TIME=2015-10-24T01:14:13.70431Z
        PRODUCT_TYPE=S2MSI1B
        RADIOMETRIC_QUALITY_FLAG=PASSED
        SENSING_TIME=2015-10-24T01:13:15.497656Z
        SENSOR_QUALITY_FLAG=PASSED
        SPECIAL_VALUE_NODATA=1
        SPECIAL_VALUE_SATURATED=0
      Corner Coordinates:
      Upper Left  (    0.0,    0.0)
      Lower Left  (    0.0, 2304.0)
      Upper Right ( 2552.0,    0.0)
      Lower Right ( 2552.0, 2304.0)
      Center      ( 1276.0, 1152.0)
      Band 1 Block=128x128 Type=UInt16, ColorInterp=Red
        Description = B4, central wavelength 665 nm
        Overviews: 1276x1152, 638x576, 319x288, 160x144
        Metadata:
          BANDNAME=B4
          BANDWIDTH=30
          BANDWIDTH_UNIT=nm
          WAVELENGTH=665
          WAVELENGTH_UNIT=nm
        Image Structure Metadata:
          NBITS=12
      Band 2 Block=128x128 Type=UInt16, ColorInterp=Green
        Description = B3, central wavelength 560 nm
      [...]
      Band 3 Block=128x128 Type=UInt16, ColorInterp=Blue
        Description = B2, central wavelength 490 nm
      [...]
      Band 4 Block=128x128 Type=UInt16, ColorInterp=Undefined
        Description = B8, central wavelength 842 nm
      [...]

See Also
--------

-  `Sentinels Scientific Data Hub <https://scihub.esa.int/>`__
-  `Sentinel 2 User
   guide <https://sentinels.copernicus.eu/web/sentinel/user-guides/sentinel-2-msi>`__
-  `Sentinel 2 User
   Handbook <https://sentinels.copernicus.eu/web/sentinel/user-guides/document-library/-/asset_publisher/xlslt4309D5h/content/sentinel-2-user-handbook>`__

Credits
-------

This driver has been developed by `Spatialys <http://spatialys.com>`__
with funding from `Centre National d'Etudes Spatiales
(CNES) <https://cnes.fr>`__
