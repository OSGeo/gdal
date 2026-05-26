.. _raster_info:

================================================================================
Getting information from raster datasets
================================================================================

From now, we assume you have a GDAL-enabled shell, and the current working
directory is the one where you have downloaded the sample datasets, as indicated
in the :ref:`install` section.

Utilities demonstrated
----------------------

- `gdal dataset identify <https://gdal.org/en/stable/programs/gdal_dataset_identify.html>`__
- `gdal raster info <https://gdal.org/en/stable/programs/gdal_vector_info.html>`__


Scanning a folder for GDAL datasets
-----------------------------------

Let's use `gdal dataset identify <https://gdal.org/en/stable/programs/gdal_dataset_identify.html>`__
in recursive mode in the current directory:

::

    $ gdal dataset identify -r .

**Output:**

::

    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml: SENTINEL2
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/INSPIRE.xml: GML
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_DETFOO_B12.jp2: JP2OpenJPEG                                  
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_DETFOO_B05.jp2: JP2OpenJPEG                                  
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_QUALIT_B8A.jp2: JP2OpenJPEG                                  
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_QUALIT_B05.jp2: JP2OpenJPEG                                  
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_DETFOO_B07.jp2: JP2OpenJPEG                                  
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_QUALIT_B08.jp2: JP2OpenJPEG                                  
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_DETFOO_B02.jp2: JP2OpenJPEG                                  
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/QI_DATA/MSK_DETFOO_B09.jp2: JP2OpenJPEG     
    [ ... snip ... ]
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/HTML/banner_2.png: PNG
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/HTML/star_bg.jpg: JPEG
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/HTML/banner_1.png: PNG
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/HTML/banner_3.png: PNG
    ./S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/manifest.safe: GML
    [ ... snip ... ]

    
We see 3 main Sentinel 2 products, the JPEG-2000 files with the data, and a bunch of auxiliary files, most of them being "noise".

Raster dataset with subdatasets
-------------------------------

Let's get information on one of the Sentinel 2 dataset

::

    $ gdal raster info S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml

**Output:**

Driver short name and long name. It is generally a good idea to consult the
driver documentation page to learn about the peculiarities of each format. Here
we are using `Sentinel-2 <https://gdal.org/en/stable/drivers/raster/sentinel2.html>`__.

::

    Driver: SENTINEL2/Sentinel 2

Metadata that applies at the dataset level

::

    Metadata:
      AOT_QUANTIFICATION_VALUE=1000.0
      AOT_QUANTIFICATION_VALUE_UNIT=none
      AOT_RETRIEVAL_ACCURACY=0.0
      AOT_RETRIEVAL_METHOD=SEN2COR_DDV
      BOA_QUANTIFICATION_VALUE=10000
      BOA_QUANTIFICATION_VALUE_UNIT=none
      CAST_SHADOW_PERCENTAGE=0.020978
      CLOUDY_PIXEL_OVER_LAND_PERCENTAGE=13.652931
      CLOUD_COVERAGE_ASSESSMENT=13.622445
      CLOUD_SHADOW_PERCENTAGE=4.0E-6
      DATATAKE_1_DATATAKE_SENSING_START=2026-04-23T09:40:29.024Z
      DATATAKE_1_DATATAKE_TYPE=INS-NOBS
      DATATAKE_1_ID=GS2B_20260423T094029_047681_N05.12
      DATATAKE_1_SENSING_ORBIT_DIRECTION=DESCENDING
      DATATAKE_1_SENSING_ORBIT_NUMBER=36
      DATATAKE_1_SPACECRAFT_NAME=Sentinel-2B
      DEGRADED_ANC_DATA_PERCENTAGE=0.0
      DEGRADED_MSI_DATA_PERCENTAGE=0
      FOOTPRINT=POLYGON((20.815513868354657 45.0636441557092, 20.738388385926616 45.0825860720989, 20.7383279976147 45.08243722604747, 20.73829168579047 45.08244616120928, 20.737841954045535 45.081338607396354, 20.723616416649268 45.08479331137292, 20.723749295718594 45.08512101907749, 20.420208979129363 45.15414241241193, 20.420325912036265 45.15443445656253, 20.4201440484806 45.154474815657785, 20.42019397077344 45.15459941811133, 20.41993843797668 45.15465628545975, 20.420062047711312 45.154964985110986, 20.141998695588608 45.217031549439376, 20.141951168063247 45.21691119363821, 20.140959170987895 45.21713316759646, 20.14095913434592 45.217133074779504, 20.140956302668084 45.21713370924307, 20.14068749523566 45.21645339695486, 19.828665356777663 45.28183133788763, 19.82877859125412 45.282121770659984, 19.82873392014264 45.282130804492965, 19.828808772594694 45.28232269102338, 19.828661421501813 45.28235252832233, 19.828817354085736 45.282752449475936, 19.723826624134375 45.30404156688893, 19.70695108472353 46.0462596089832, 21.126167237224053 46.05350473573164, 21.124259208669503 45.193517377663945, 21.08029382801478 45.086817339340286, 21.071227534895563 45.06492790336747, 20.815513868354657 45.0636441557092))
      FORMAT_CORRECTNESS=PASSED
      GENERAL_QUALITY=PASSED
      GENERATION_TIME=2026-04-23T11:57:14.000000Z
      GEOMETRIC_QUALITY=PASSED
      GRANULE_MEAN_AOT=0.06598
      GRANULE_MEAN_WV=0.75046
      HIGH_PROBA_CLOUDS_PERCENTAGE=0.001836
      L2A_QUALITY=PASSED
      MEDIUM_PROBA_CLOUDS_PERCENTAGE=0.009661
      NODATA_PIXEL_PERCENTAGE=9.990737
      NOT_VEGETATED_PERCENTAGE=28.988278
      OZONE_SOURCE=AUX_ECMWFT
      OZONE_VALUE=416.167104
      PREVIEW_GEO_INFO=Not applicable
      PREVIEW_IMAGE_URL=Not applicable
      PROCESSING_BASELINE=05.12
      PROCESSING_LEVEL=Level-2A
      PRODUCT_DOI=https://doi.org/10.5270/S2_-znk9xsj
      PRODUCT_START_TIME=2026-04-23T09:40:29.024Z
      PRODUCT_STOP_TIME=2026-04-23T09:40:29.024Z
      PRODUCT_TYPE=S2MSI2A
      PRODUCT_URI=S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE
      RADIATIVE_TRANSFER_ACCURACY=0.0
      RADIOMETRIC_QUALITY=PASSED
      REFERENCE_BAND=B4
      REFLECTANCE_CONVERSION_U=0.991700831171221
      SATURATED_DEFECTIVE_PIXEL_PERCENTAGE=0.0
      SENSOR_QUALITY=PASSED
      SNOW_ICE_PERCENTAGE=0.0
      SPECIAL_VALUE_NODATA=0
      SPECIAL_VALUE_SATURATED=65535
      THIN_CIRRUS_PERCENTAGE=13.610949
      UNCLASSIFIED_PERCENTAGE=0.243549
      VEGETATION_PERCENTAGE=56.460464
      WATER_PERCENTAGE=0.664281
      WATER_VAPOUR_RETRIEVAL_ACCURACY=0.0
      WVP_QUANTIFICATION_VALUE=1000.0
      WVP_QUANTIFICATION_VALUE_UNIT=cm

And "subdatasets"

::

    Subdatasets:
      SUBDATASET_1_NAME=SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634
      SUBDATASET_1_DESC=Bands B2, B3, B4, B8, AOT, WVP with 10m resolution, UTM 34N
      SUBDATASET_2_NAME=SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:20m:EPSG_32634
      SUBDATASET_2_DESC=Bands B5, B6, B7, B8A, B11, B12, AOT, CLD, SCL, SNW, WVP with 20m resolution, UTM 34N
      SUBDATASET_3_NAME=SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:60m:EPSG_32634
      SUBDATASET_3_DESC=Bands B1, B9, AOT, CLD, SCL, SNW, WVP with 60m resolution, UTM 34N
      SUBDATASET_4_NAME=SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:TCI:EPSG_32634
      SUBDATASET_4_DESC=True color image, UTM 34N


Subdatasets can be thought as sub-products or raster layers of a container file.
The value of a ``SUBDATASET_xxx_NAME`` key can be used as valid GDAL dataset name.

Text information on a raster dataset
------------------------------------

So for example let's open the 10m resolution bands with:


::

    $ gdal raster info SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634

**Output:**

File(s) composing the dataset:

::

    Files: S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml
           S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/MTD_TL.xml
           S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_B04_10m.jp2
           S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_B03_10m.jp2
           S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_B02_10m.jp2
           S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_AOT_10m.jp2
           S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_WVP_10m.jp2

Dimensions in pixel: first value is the width (number of columns), second value is the height (number of rows):

::

    Size is 10980, 10980


Information about the Coordinate Reference System (CRS), also often informally
called "projection":

::

    Coordinate Reference System:
      - name: WGS 84 / UTM zone 34N
      - ID: EPSG:32634
      - type: Projected
      - projection type: UTM zone 34N, Transverse Mercator
      - units: metre

Information how the axis of the dataset maps to the axis of the coordinate
reference system. Here the first axis of the data, which is always the X/width
dimension for GDAL rasters maps to the first axis of the CRS which is Eastings, and
the second axis of the raster, which is always the Y/height dimension maps to
the second axis of the CRS which is Northings.

::

    Data axis to CRS axis mapping: 1,2

Coordinates of the top-left corner of the raster, X value first, Y value second:

::

    Origin = (399960.000000000000000,5100000.000000000000000)


Size of a pixel in CRS units (here meters), with  X value first, Y value second:

::

    Pixel Size = (10.000000000000000,-10.000000000000000)

Image Structure Metadata is format dependent and gives information on the
compression method, internal organization with the ``INTERLEAVING`` keyword
(pixel versus band interleaving. cf https://gdal.org/en/stable/user/raster_data_model.html#multiband-pixel-organization-interleave-metadata-item), etc.

::

    Image Structure Metadata:
      COMPRESSION=JPEG2000

Coordinates of the corner and center. First tuple of values is expressed in CRS,
so here as eastings, northings as this is a projected CRS. Second tuple is
their corresponding value in the geographic CRS underlying the projected CRS,
here WGS 84 as longitude, latitude in degree, minute and decimal seconds.

::

    Corner Coordinates:
    Upper Left  (  399960.000, 5100000.000) ( 19d42'25.02"E, 46d 2'46.53"N)
    Lower Left  (  399960.000, 4990200.000) ( 19d43'45.90"E, 45d 3'29.49"N)
    Upper Right (  509760.000, 5100000.000) ( 21d 7'34.20"E, 46d 3'12.62"N)
    Lower Right (  509760.000, 4990200.000) ( 21d 7'26.31"E, 45d 3'54.69"N)
    Center      (  454860.000, 5045100.000) ( 20d25'17.86"E, 45d33'28.71"N)

Followed by band specific information

::

    Band 1 Block=128x128 Type=UInt16, ColorInterp=Red
      Description = B4, central wavelength 665 nm
      Overviews: 5490x5490, 2745x2745, 1373x1373
      Metadata:
        BANDNAME=B4
        BANDWIDTH=30
        BANDWIDTH_UNIT=nm
        WAVELENGTH=665
        WAVELENGTH_UNIT=nm
        SOLAR_IRRADIANCE=1512.79
        SOLAR_IRRADIANCE_UNIT=W/m2/um
        BOA_ADD_OFFSET=-1000
      Image Structure Metadata:
        NBITS=15
      Imagery:
        CENTRAL_WAVELENGTH_UM=0.665
        FWHM_UM=0.030

    [ ... snip ...]

Block corresponds to the smallest unit GDAL can access pixel values. It is typically
either a whole line or a set of few lines called "strip", or a rectangular
(almost always a square) called a "tile".

The data type ``UInt16`` is a 16-bit unsigned integer value, so for values between 0 and
65535, here actually restricted to 0-32767 given the ``NBITS=15`` metadata.

Overviews correspond to image pyramids, i.e. reduced resolution versions of the
full resolution raster. They are generally automatically used by GDAL for processing
occurring at a reduced resolution.

The `Imagery metadata domain <https://gdal.org/en/stable/user/raster_data_model.html#imagery-domain-remote-sensing>`__
contains a few metadata items whose meaning is normalized across drivers, whereas
the main (default) domain is driver specific and may contain anything.

``CENTRAL_WAVELENGTH_UM`` corresponds to the Central Wavelength in micrometers and
``FWHM_UM`` to the Full-width half-maximum (FWHM) value in micrometers.

There are various options that can be used to customize the default output of
``gdal raster info``. You can get them by asking for auto-completion suggestions
by adding dash dash and pressing the TAB key twice.

::

    $ gdal raster info SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 --<TAB><TAB>

**Output:**

::

    --approx-stats     --hist             --list-mdd         --no-ct            --no-mask          --open-option      --subdataset       
    --checksum         --input            --metadata-domain  --no-fl            --no-md            --output-format    
    --crs-format       --input-format     --min-max          --no-gcp           --no-nodata        --stats


For example statistics:

::

    $ gdal raster info SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 --stats

Answer "y" and validate.

**Output:**

::

    Band 1 Block=128x128 Type=UInt16, ColorInterp=Red
      Description = B4, central wavelength 665 nm
      Min=0.000 Max=17143.000 
      Minimum=0.000, Maximum=17143.000, Mean=1439.120, StdDev=582.568
      [... snip ... ]
      Metadata:
          [... snip ... ]
          STATISTICS_MINIMUM=0
          STATISTICS_MAXIMUM=17143
          STATISTICS_MEAN=1439.1200572908
          STATISTICS_STDDEV=582.56825507515
          STATISTICS_VALID_PERCENT=100


Here it would seem that all pixels are valid, but the Sentinel 2 driver does
not report a NoData / missing value for the dataset, hence if you open the
dataset with ``qgis S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml``
you will see that values at 0 actually correspond to a NoData value. Something
to keep in mind and take into account in further processing.


Hidden feature: symbolic links and subdatasets
----------------------------------------------

.. warning::

  Only works as a true symbolic link for Linux and MacOSX, sorry...

  But for Windows, you can almost have the same behavior by doing

  ::

      gdal raster convert <input> <shortcut.vrt>

As those Sentinel 2 filenames are quite long, you can of course create symbolic links
to them with:

::

  $ ln -s S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml s2_TDR.xml
  

But even better you can also use them to point to subdatasets, which are not
actual files:

::

  $ ln -s SENTINEL2_L2A:s2_TDR.xml:10m:EPSG_32634 s2_TDR_10m.xml

  $ gdal raster info s2_TDR_10m.xml

And that also works with QGIS

::

  $ qgis s2_TDR_10m.xml


Short version
-------------

Do you find ``gdal raster info`` too long ?

You can use ``gdal info`` or even ``gdal`` !

Beware those short versions only accept the ``--format`` option, which is common
between ``gdal raster info`` and ``gdal vector info``


Getting information as JSON
---------------------------

Text output is friendly for humans, but no so much for machine processing.

::

    $ gdal raster info SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 --format=json


.. collapse::  Output (trimmed to keep only one band)

  .. code-block:: json

      {
        "description":"SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634",
        "driverShortName":"SENTINEL2",
        "driverLongName":"Sentinel 2",
        "files":[
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml",
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/MTD_TL.xml",
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_B04_10m.jp2",
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_B03_10m.jp2",
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_B02_10m.jp2",
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_B08_10m.jp2",
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_AOT_10m.jp2",
          "S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/GRANULE/L2A_T34TDR_A047681_20260423T094113/IMG_DATA/R10m/T34TDR_20260423T094029_WVP_10m.jp2"
        ],
        "size":[
          10980,
          10980
        ],
        "coordinateSystem":{
          "wkt":"PROJCRS[\"WGS 84 / UTM zone 34N\",\n    BASEGEOGCRS[\"WGS 84\",\n        DATUM[\"World Geodetic System 1984\",\n            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n                LENGTHUNIT[\"metre\",1]]],\n        PRIMEM[\"Greenwich\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433]],\n        ID[\"EPSG\",4326]],\n    CONVERSION[\"UTM zone 34N\",\n        METHOD[\"Transverse Mercator\",\n            ID[\"EPSG\",9807]],\n        PARAMETER[\"Latitude of natural origin\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8801]],\n        PARAMETER[\"Longitude of natural origin\",21,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8802]],\n        PARAMETER[\"Scale factor at natural origin\",0.9996,\n            SCALEUNIT[\"unity\",1],\n            ID[\"EPSG\",8805]],\n        PARAMETER[\"False easting\",500000,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8806]],\n        PARAMETER[\"False northing\",0,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8807]]],\n    CS[Cartesian,2],\n        AXIS[\"easting\",east,\n            ORDER[1],\n            LENGTHUNIT[\"metre\",1]],\n        AXIS[\"northing\",north,\n            ORDER[2],\n            LENGTHUNIT[\"metre\",1]],\n    ID[\"EPSG\",32634]]",
          "dataAxisToSRSAxisMapping":[
            1,
            2
          ]
        },
        "geoTransform":[
          399960.0,
          10.0,
          0.0,
          5100000.0,
          0.0,
          -10.0
        ],
        "metadata":{
          "":{
            "AOT_QUANTIFICATION_VALUE":"1000.0",
            "AOT_QUANTIFICATION_VALUE_UNIT":"none",
            "AOT_RETRIEVAL_ACCURACY":"0.0",
            "AOT_RETRIEVAL_METHOD":"SEN2COR_DDV",
            "BOA_QUANTIFICATION_VALUE":"10000",
            "BOA_QUANTIFICATION_VALUE_UNIT":"none",
            "CAST_SHADOW_PERCENTAGE":"0.020978",
            "CLOUDY_PIXEL_OVER_LAND_PERCENTAGE":"13.652931",
            "CLOUD_COVERAGE_ASSESSMENT":"13.622445",
            "CLOUD_SHADOW_PERCENTAGE":"4.0E-6",
            "DATATAKE_1_DATATAKE_SENSING_START":"2026-04-23T09:40:29.024Z",
            "DATATAKE_1_DATATAKE_TYPE":"INS-NOBS",
            "DATATAKE_1_ID":"GS2B_20260423T094029_047681_N05.12",
            "DATATAKE_1_SENSING_ORBIT_DIRECTION":"DESCENDING",
            "DATATAKE_1_SENSING_ORBIT_NUMBER":"36",
            "DATATAKE_1_SPACECRAFT_NAME":"Sentinel-2B",
            "DEGRADED_ANC_DATA_PERCENTAGE":"0.0",
            "DEGRADED_MSI_DATA_PERCENTAGE":"0",
            "FORMAT_CORRECTNESS":"PASSED",
            "GENERAL_QUALITY":"PASSED",
            "GENERATION_TIME":"2026-04-23T11:57:14.000000Z",
            "GEOMETRIC_QUALITY":"PASSED",
            "GRANULE_MEAN_AOT":"0.06598",
            "GRANULE_MEAN_WV":"0.75046",
            "HIGH_PROBA_CLOUDS_PERCENTAGE":"0.001836",
            "L2A_QUALITY":"PASSED",
            "MEDIUM_PROBA_CLOUDS_PERCENTAGE":"0.009661",
            "NODATA_PIXEL_PERCENTAGE":"9.990737",
            "NOT_VEGETATED_PERCENTAGE":"28.988278",
            "OZONE_SOURCE":"AUX_ECMWFT",
            "OZONE_VALUE":"416.167104",
            "PREVIEW_GEO_INFO":"Not applicable",
            "PREVIEW_IMAGE_URL":"Not applicable",
            "PROCESSING_BASELINE":"05.12",
            "PROCESSING_LEVEL":"Level-2A",
            "PRODUCT_DOI":"https://doi.org/10.5270/S2_-znk9xsj",
            "PRODUCT_START_TIME":"2026-04-23T09:40:29.024Z",
            "PRODUCT_STOP_TIME":"2026-04-23T09:40:29.024Z",
            "PRODUCT_TYPE":"S2MSI2A",
            "PRODUCT_URI":"S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE",
            "RADIATIVE_TRANSFER_ACCURACY":"0.0",
            "RADIOMETRIC_QUALITY":"PASSED",
            "REFERENCE_BAND":"B4",
            "REFLECTANCE_CONVERSION_U":"0.991700831171221",
            "SATURATED_DEFECTIVE_PIXEL_PERCENTAGE":"0.0",
            "SENSOR_QUALITY":"PASSED",
            "SNOW_ICE_PERCENTAGE":"0.0",
            "SPECIAL_VALUE_NODATA":"0",
            "SPECIAL_VALUE_SATURATED":"65535",
            "THIN_CIRRUS_PERCENTAGE":"13.610949",
            "UNCLASSIFIED_PERCENTAGE":"0.243549",
            "VEGETATION_PERCENTAGE":"56.460464",
            "WATER_PERCENTAGE":"0.664281",
            "WATER_VAPOUR_RETRIEVAL_ACCURACY":"0.0",
            "WVP_QUANTIFICATION_VALUE":"1000.0",
            "WVP_QUANTIFICATION_VALUE_UNIT":"cm"
          },
          "IMAGE_STRUCTURE":{
            "COMPRESSION":"JPEG2000"
          }
        },
        "cornerCoordinates":{
          "upperLeft":[
            399960.0,
            5100000.0
          ],
          "lowerLeft":[
            399960.0,
            4990200.0
          ],
          "lowerRight":[
            509760.0,
            4990200.0
          ],
          "upperRight":[
            509760.0,
            5100000.0
          ],
          "center":[
            454860.0,
            5045100.0
          ]
        },
        "wgs84Extent":{
          "type":"Polygon",
          "coordinates":[
            [
              [
                19.7069511,
                46.0462596
              ],
              [
                19.7294164,
                45.0581917
              ],
              [
                21.1239745,
                45.0651927
              ],
              [
                21.1261672,
                46.0535047
              ],
              [
                19.7069511,
                46.0462596
              ]
            ]
          ]
        },
        "bands":[
          {
            "band":1,
            "block":[
              128,
              128
            ],
            "type":"UInt16",
            "colorInterpretation":"Red",
            "description":"B4, central wavelength 665 nm",
            "min":0.0,
            "max":17143.0,
            "overviews":[
              {
                "size":[
                  5490,
                  5490
                ]
              },
              {
                "size":[
                  2745,
                  2745
                ]
              },
              {
                "size":[
                  1373,
                  1373
                ]
              }
            ],
            "metadata":{
              "":{
                "BANDNAME":"B4",
                "BANDWIDTH":"30",
                "BANDWIDTH_UNIT":"nm",
                "WAVELENGTH":"665",
                "WAVELENGTH_UNIT":"nm",
                "SOLAR_IRRADIANCE":"1512.79",
                "SOLAR_IRRADIANCE_UNIT":"W/m2/um",
                "BOA_ADD_OFFSET":"-1000"
              },
              "IMAGE_STRUCTURE":{
                "NBITS":"15"
              },
              "IMAGERY":{
                "CENTRAL_WAVELENGTH_UM":"0.665",
                "FWHM_UM":"0.030"
              }
            }
          }
        ],
        "stac":{
          "proj:shape":[
            10980,
            10980
          ],
          "proj:wkt2":"PROJCRS[\"WGS 84 / UTM zone 34N\",\n    BASEGEOGCRS[\"WGS 84\",\n        DATUM[\"World Geodetic System 1984\",\n            ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n                LENGTHUNIT[\"metre\",1]]],\n        PRIMEM[\"Greenwich\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433]],\n        ID[\"EPSG\",4326]],\n    CONVERSION[\"UTM zone 34N\",\n        METHOD[\"Transverse Mercator\",\n            ID[\"EPSG\",9807]],\n        PARAMETER[\"Latitude of natural origin\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8801]],\n        PARAMETER[\"Longitude of natural origin\",21,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8802]],\n        PARAMETER[\"Scale factor at natural origin\",0.9996,\n            SCALEUNIT[\"unity\",1],\n            ID[\"EPSG\",8805]],\n        PARAMETER[\"False easting\",500000,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8806]],\n        PARAMETER[\"False northing\",0,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8807]]],\n    CS[Cartesian,2],\n        AXIS[\"easting\",east,\n            ORDER[1],\n            LENGTHUNIT[\"metre\",1]],\n        AXIS[\"northing\",north,\n            ORDER[2],\n            LENGTHUNIT[\"metre\",1]],\n    ID[\"EPSG\",32634]]",
          "proj:epsg":32634,
          "proj:projjson":{
            "$schema":"https://proj.org/schemas/v0.7/projjson.schema.json",
            "type":"ProjectedCRS",
            "name":"WGS 84 / UTM zone 34N",
            "base_crs":{
              "type":"GeographicCRS",
              "name":"WGS 84",
              "datum":{
                "type":"GeodeticReferenceFrame",
                "name":"World Geodetic System 1984",
                "ellipsoid":{
                  "name":"WGS 84",
                  "semi_major_axis":6378137,
                  "inverse_flattening":298.257223563
                }
              },
              "coordinate_system":{
                "subtype":"ellipsoidal",
                "axis":[
                  {
                    "name":"Geodetic latitude",
                    "abbreviation":"Lat",
                    "direction":"north",
                    "unit":"degree"
                  },
                  {
                    "name":"Geodetic longitude",
                    "abbreviation":"Lon",
                    "direction":"east",
                    "unit":"degree"
                  }
                ]
              },
              "id":{
                "authority":"EPSG",
                "code":4326
              }
            },
            "conversion":{
              "name":"UTM zone 34N",
              "method":{
                "name":"Transverse Mercator",
                "id":{
                  "authority":"EPSG",
                  "code":9807
                }
              },
              "parameters":[
                {
                  "name":"Latitude of natural origin",
                  "value":0,
                  "unit":"degree",
                  "id":{
                    "authority":"EPSG",
                    "code":8801
                  }
                },
                {
                  "name":"Longitude of natural origin",
                  "value":21,
                  "unit":"degree",
                  "id":{
                    "authority":"EPSG",
                    "code":8802
                  }
                },
                {
                  "name":"Scale factor at natural origin",
                  "value":0.9996,
                  "unit":"unity",
                  "id":{
                    "authority":"EPSG",
                    "code":8805
                  }
                },
                {
                  "name":"False easting",
                  "value":500000,
                  "unit":"metre",
                  "id":{
                    "authority":"EPSG",
                    "code":8806
                  }
                },
                {
                  "name":"False northing",
                  "value":0,
                  "unit":"metre",
                  "id":{
                    "authority":"EPSG",
                    "code":8807
                  }
                }
              ]
            },
            "coordinate_system":{
              "subtype":"Cartesian",
              "axis":[
                {
                  "name":"Easting",
                  "abbreviation":"",
                  "direction":"east",
                  "unit":"metre"
                },
                {
                  "name":"Northing",
                  "abbreviation":"",
                  "direction":"north",
                  "unit":"metre"
                }
              ]
            },
            "id":{
              "authority":"EPSG",
              "code":32634
            }
          },
          "proj:transform":[
            10.0,
            0.0,
            399960.0,
            0.0,
            -10.0,
            5100000.0
          ],
          "raster:bands":[
            {
              "data_type":"uint16"
            },
            {
              "data_type":"uint16"
            },
            {
              "data_type":"uint16"
            },
            {
              "data_type":"uint16"
            },
            {
              "data_type":"uint16"
            },
            {
              "data_type":"uint16"
            }
          ],
          "eo:bands":[
            {
              "name":"b1",
              "description":"B4, central wavelength 665 nm",
              "common_name":"red"
            },
            {
              "name":"b2",
              "description":"B3, central wavelength 560 nm",
              "common_name":"green"
            },
            {
              "name":"b3",
              "description":"B2, central wavelength 490 nm",
              "common_name":"blue"
            },
            {
              "name":"b4",
              "description":"B8, central wavelength 842 nm",
              "common_name":"nir"
            },
            {
              "name":"b5",
              "description":"AOT, Aerosol Optical Thickness map (at 550nm)"
            },
            {
              "name":"b6",
              "description":"WVP, Scene-average Water Vapour map"
            }
          ]
        }
      }


|

We can for example extract only the colour interpretation of the first
band by combining with the very powerful `jq <https://jqlang.org/>`__ JSON command line
processing utility

::

    $ gdal raster info SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 --format=json  | jq ".bands[0].colorInterpretation"


.. note::

    ``jq`` uses 0-based array indexing.

**Output:**

::

     "Red"


Exercise
--------

Extract the CRS code using ``jq``.

.. collapse:: (hint)

  .. hint:: Look at STAC related metadata in the JSON output.

|

==> :ref:`solution_raster_info`.


Invoking Algorithms from Python
-------------------------------

See `How to use gdal CLI algorithms from Python <https://gdal.org/en/stable/programs/gdal_cli_from_python.html>`__.

Start Python:

::

  $ python

And run:

.. code-block:: python

    from osgeo import gdal
    help(gdal.alg)


::

  NAME
      gdal.alg

  SUBMODULES
      dataset
      driver
      mdim
      raster
      vector
      vsi


.. code-block:: python

     help(gdal.alg.raster.info)


::

  Help on function info:

  info(
      input_format: Optional[Union[List[str], dict, str]] = None,
      open_option: Optional[Union[List[str], dict, str]] = None,
      input: Optional[Union[List[gdal.Dataset], List[str], List[os.PathLike[str]]]] = None,
      output_format: Optional[str] = None,
      min_max: Optional[bool] = None,
      stats: Optional[bool] = None,
      approx_stats: Optional[bool] = None,
      hist: Optional[bool] = None,
      no_gcp: Optional[bool] = None,
      no_md: Optional[bool] = None,
      no_ct: Optional[bool] = None,
      no_fl: Optional[bool] = None,
      checksum: Optional[bool] = None,
      list_mdd: Optional[bool] = None,
      metadata_domain: Optional[str] = None,
      no_nodata: Optional[bool] = None,
      no_mask: Optional[bool] = None,
      subdataset: Optional[int] = None,
      crs_format: Optional[str] = None,
      progress: Optional[Callable[[float, str, object], bool]] = None,
      **kwargs
  )
      Return information on a raster dataset.

      Consult https://gdal.org/programs/gdal_raster_info.html for more details.

      Parameters
      ----------
      input_format: Optional[Union[List[str], dict, str]]=None
          Input formats
      open_option: Optional[Union[List[str], dict, str]]=None
          Open options
      input: Union[List[gdal.Dataset], List[str], List[os.PathLike[str]]]
          Aliases: dataset
          Input raster dataset
      output_format: Optional[str]=None
          Aliases: format
          Output format
      min_max: Optional[bool]=None
          Compute minimum and maximum value
      stats: Optional[bool]=None
          Retrieve or compute statistics, using all pixels
      approx_stats: Optional[bool]=None
          Retrieve or compute statistics, using a subset of pixels
      hist: Optional[bool]=None
          Retrieve or compute histogram
      no_gcp: Optional[bool]=None
          Suppress ground control points list printing
      no_md: Optional[bool]=None
          Suppress metadata printing
      no_ct: Optional[bool]=None
          Suppress color table printing
      no_fl: Optional[bool]=None
          Suppress file list printing
      checksum: Optional[bool]=None
          Compute pixel checksum
      list_mdd: Optional[bool]=None
          Aliases: list_metadata_domains
          List all metadata domains available for the dataset
      metadata_domain: Optional[str]=None
          Report metadata for the specified domain. 'all' can be used to report metadata in all domains

    Output parameters
    -----------------
    output_string: str
        Output string, in which the result is placed


Let's try it:

::

  >>> gdal.alg.raster.info(input='SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634')

  <osgeo.gdal.Algorithm; proxy of <Swig Object of type 'GDALAlgorithmHS *' at 0x7feb83e38cb0> >


So that's return an Algorithm object. Let's extract its output:
  
.. code-block:: python

    from osgeo import gdal
    with gdal.alg.raster.info(input='SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634', min_max=True) as alg:
        print(alg.Output()['bands'][0])

After a few seconds:

::

  {'band': 1, 'block': [128, 128], 'type': 'UInt16', 'colorInterpretation': 'Red', 'description': 'B4, central wavelength 665 nm', 'computedMin': 0.0, 'computedMax': 17143.0, 'overviews': [{'size': [5490, 5490]}, {'size': [2745, 2745]}, {'size': [1373, 1373]}], 'metadata': {'': {'BANDNAME': 'B4', 'BANDWIDTH': '30', 'BANDWIDTH_UNIT': 'nm', 'WAVELENGTH': '665', 'WAVELENGTH_UNIT': 'nm', 'SOLAR_IRRADIANCE': '1512.79', 'SOLAR_IRRADIANCE_UNIT': 'W/m2/um', 'BOA_ADD_OFFSET': '-1000'}, 'IMAGE_STRUCTURE': {'NBITS': '15'}, 'IMAGERY': {'CENTRAL_WAVELENGTH_UM': '0.665', 'FWHM_UM': '0.030'}}}

To exit Python and return to the shell, type:

::

  exit()


Getting pixel value
-------------------

Use `gdal raster pixel-info <https://gdal.org/en/stable/programs/gdal_raster_pixel_info.html>`__

::

  gdal raster pixel-info <dataset> <X> <Y>

By default, the X and Y positional arguments are in column,row coordinate space
(``--position-crs=pixel``). It is also possible to specify them in the CRS of
the dataset (``--position-crs=dataset``), or an explicit CRS.

For example, getting pixel values at Timișoara center (45.7558° N, 21.2322° E):

::

  $ gdal raster pixel-info \
     SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
     21.2322 45.7558 --position-crs EPSG:4326


.. code-block:: json

  {
    "type":"FeatureCollection",
    "crs":{
      "type":"name",
      "properties":{
        "name":"urn:ogc:def:crs:EPSG::32634"
      }
    },
    "features":[
      {
        "type":"Feature",
        "properties":{
          "input_coordinate":[
            21.232199999999999,
            45.755800000000001
          ],
          "column":1807.8714361003731,
          "line":3305.8006131011061,
          "bands":[
            {
              "band_number":1,
              "raw_value":2490,
              "unscaled_value":2490.0,
              "files":[
                "S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE\/GRANULE\/L2A_T34TER_A047681_20260423T094113\/IMG_DATA\/R10m\/T34TER_20260423T094029_B04_10m.jp2"
              ]
            },
            "[... snip ...]",
            {
              "band_number":6,
              "raw_value":927,
              "unscaled_value":927.0,
              "files":[
                "S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE\/GRANULE\/L2A_T34TER_A047681_20260423T094113\/IMG_DATA\/R10m\/T34TER_20260423T094029_WVP_10m.jp2"
              ]
            }
          ]
        },
        "geometry":{
          "type":"Point",
          "coordinates":[
            518058.71436100372,
            5066941.9938689889
          ]
        }
      }
    ]
  }

