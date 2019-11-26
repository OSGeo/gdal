.. _raster.rda:

================================================================================
RDA (DigitalGlobe Raster Data Access)
================================================================================

.. shortname:: RDA

.. versionadded:: 2.3

.. build_dependencies:: libcurl

This driver can connect to DigitalGlobe RDA REST API. GDAL/OGR must be
built with Curl support in order for the RDA driver to be compiled.

The driver retrieves metadata on graphs and fetches the raster by tiles.
Data types byte, uint16, int16, uint32, int32, float32 and float64 are
supported.

Any valid graph or template is supported via the DigitalGlobe RDA REST
API.

There is no support for overviews.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a datasource is :

::

   {"graphId":"some_value", "nodeId": "another_value"}

OR

::

   {"templateId":"some_value", "parameters": { "someparam": "someparamval"}}

So a JSon serialized document with 2 attributes graph-id and node-id.

Those values can for example be retrieved from graphs built by
`GraphStudio <https://rda.geobigdata.io/>`__.

Connection String options (optional)
------------------------------------

-  ::

      "options": {"delete-on-close": false}

   can be added to the JSon document to request that cached tiles and
   metadata are not destroyed at dataset closing. The default, if not
   specified, is true.

-  ::

      "options": {"max-connections": 32}

   can be added to the JSon document to request that cached tiles be
   fetched using a maximum number of concurrent connections. The
   default, if not specified, is equal to 8 \* number of CPUs.

-  ::

      "options": {"advise-read": false}

   can be added to the JSon document to request advise read not be used
   when reading the dataset. The default, if not specified, is true.

Authentication
--------------

Access to the API requires an authentication token. For that, 2
parameters (username, password) must be provided to the driver. They can
be retrieved from the below configuration options, or from the
~/.gbdx-config file.

The access token will be cached in ~/.gdal/rda_cache/authentication.json
and reused from there until its expiration period is reached.

Configuration options
---------------------

The following configuration options are available :

-  **GBDX_AUTH_URL**\ =value: To specify the OAuth authentication
   endpoint. Defaults to https://geobigdata.io/auth/v1/oauth/token/. If
   not specified, the auth_url parameter from ~/.gbdx-config will be
   used if it exists.
-  **GBDX_RDA_API_URL**\ =value: To specify the RDA API endpoint.
   Defaults to https://rda.geobigdata.io/v1. If not specified, the
   rda_api_url parameter from ~/.gbdx-config will be used if it exists.
-  **GBDX_USERNAME**\ =value: To specify the OAuth user name needed to
   get to an authentication token. If not specified, the user_name
   parameter from ~/.gbdx-config must be set.
-  **GBDX_PASSWORD**\ =value: To specify the OAuth user name needed to
   get to an authentication token. If not specified, the password
   parameter from ~/.gbdx-config must be set.

~/.gbdx-config file
-------------------

This file may be created in the home directory of the user (value of the
$HOME environment variable on Unix, $USERPROFILE on Windows). It can
contain values from the above configuration options.

::

   [gbdx]
   auth_url = https://geobigdata.io/auth/v1/oauth/token/ (optional)
   rda_api_url = https://rda.geobigdata.io/v1 (optional)
   user_name = value (required)
   user_password = value (required)

Caching
-------

By default, the authentication token is cached in the ~/.gdal/rda_cache
directory. This directory may be changed with the RDA_CACHE_DIR
configuration option. By default, dataset metadata and tiles are
temporarily cached in ~/.gdal/rda_cache/{graph-id}/{node-id}, and
deleted on dataset closing, unless

::

   "options": {"delete-on-close": false}

is found in the dataset name.

Open Options
------------

By default, the number of concurrent downloads will be 8*number of CPUs
up to a maximum of 64. The maximum number of concurrent connections can
be configured by the *MAXCONNECT* option

Examples
~~~~~~~~

-  Display metadata, and keep it cached:

   ::

      gdalinfo '{"graphId":"832050eb7d271d8704c8889369ee0a8a1da82acdee1b20e1700b6d053e94d1fe","nodeId":"Orthorectify_hko89y", "options": {"delete-on-close": false}}'

   ::

      Driver: RDA/DigitalGlobe Raster Data Access driver
      Files: none associated
      Size is 9911, 7084
      Coordinate System is:
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
      Origin = (-84.183163638386631,33.835018117204456)
      Pixel Size = (0.000020885734819,-0.000020885734819)
      Metadata:
        ACQUISITION_DATE=2017-04-07T16:25:29.156Z
        CLOUD_COVER=0.0
        GSD=2.325 m
        SAT_AZIMUTH=163.7
        SAT_ELEVATION=58.3
        SENSOR_NAME=8-band (Coastal, Blue, Green, Yellow, Red, Red-edge, NIR1, NIR2) Multispectral
        SENSOR_PLATFORM_NAME=WV02
        SUN_AZIMUTH=143.5
        SUN_ELEVATION=58.6
      Image Structure Metadata:
        INTERLEAVE=PIXEL
      Corner Coordinates:
      Upper Left  ( -84.1831636,  33.8350181)
      Lower Left  ( -84.1831636,  33.6870636)
      Upper Right ( -83.9761651,  33.8350181)
      Lower Right ( -83.9761651,  33.6870636)
      Center      ( -84.0796644,  33.7610408)
      Band 1 Block=256x256 Type=UInt16, ColorInterp=Undefined
      Band 2 Block=256x256 Type=UInt16, ColorInterp=Blue
      Band 3 Block=256x256 Type=UInt16, ColorInterp=Green
      Band 4 Block=256x256 Type=UInt16, ColorInterp=Yellow
      Band 5 Block=256x256 Type=UInt16, ColorInterp=Red
      Band 6 Block=256x256 Type=UInt16, ColorInterp=Undefined
      Band 7 Block=256x256 Type=UInt16, ColorInterp=Undefined
      Band 8 Block=256x256 Type=UInt16, ColorInterp=Undefined

-  Extract a subwindow from a dataset:

   ::

      gdal_translate -srcwin 1000 2000 500 500 '{"graphId":"832050eb7d271d8704c8889369ee0a8a1da82acdee1b20e1700b6d053e94d1fe","nodeId":"Orthorectify_hko89y"}' out.tif

-  Materialize a dataset specifying a custom number of concurrent
   connections:

   ::

      gdal_translate -oo MAXCONNECT=96 '{"graphId":"832050eb7d271d8704c8889369ee0a8a1da82acdee1b20e1700b6d053e94d1fe","nodeId":"Orthorectify_hko89y"}' out.tif

-  Materialize a dataset from a template:

   ::

      gdal_translate '{"templateId": "sample", "parameters": { "imageId": "afa56b05-35ad-47d1-bc7f-3e23d220482d"}}' out.tif
