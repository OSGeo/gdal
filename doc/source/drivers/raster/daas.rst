.. _raster.daas:

================================================================================
DAAS (Airbus DS Intelligence Data As A Service driver)
================================================================================

.. shortname:: DAAS

.. versionadded:: 3.0

.. build_dependencies:: libcurl

This driver can connect to the Airbus DS Intelligence Data As A Service
API. GDAL/OGR must be built with Curl support in order for the DAAS
driver to be compiled.

Orthorectified (with geotransform) and raw (with RPCs) images are
supported.

Overviews are supported.

The API is not publicly available but will be released soon. Further
information will be found here: https://api.oneatlas.airbus.com/

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The nominal syntax to open a datasource is :

::

   DAAS:https://example.com/path/to/image/metadata

A more minimal syntax can be used:

::

   DAAS:

provided that the GET_METADATA_URL open option is filled.

Authentication
--------------

Access to the API requires an authentication token. There are two
methods supported:

-  Authentication with an API key and a client id. They must be provided
   respectively with the :oo:`API_KEY` open option (or :config:`GDAL_DAAS_API_KEY`
   configuration option) and the :oo:`CLIENT_ID` open option (or
   :config:`GDAL_DAAS_CLIENT_ID` configuration option). In that case, the driver will
   authenticate against the authentication endpoint to get an access
   token.
-  Directly providing the access token with the :oo:`ACCESS_TOKEN` open option
   (or :config:`GDAL_DAAS_ACCESS_TOKEN` configuration option).

In both cases, the :oo:`X_FORWARDED_USER` open option (or
:config:`GDAL_DAAS_X_FORWARDED_USER` configuration option) can be specified to
fill the HTTP X-Forwarded-User header in requests sent to the DAAS
service endpoint with the user from which the request originates from.

See https://api.oneatlas.airbus.com/guides/g-authentication/ for further
details

Configuration options
---------------------

|about-config-options|
The following configuration options are available :

-  .. config:: GDAL_DAAS_API_KEY

      Equivalent of :oo:`API_KEY` open option.

-  .. config:: GDAL_DAAS_CLIENT_ID

      Equivalent of :oo:`CLIENT_ID` open option.

-  .. config:: GDAL_DAAS_ACCESS_TOKEN

      Equivalent of :oo:`ACCESS_TOKEN` open option.

-  .. config:: GDAL_DAAS_X_FORWARDED_USER

      Equivalent of :oo:`X_FORWARDED_USER` open option.

Open options
------------

|about-open-options|
The following open options are available :

-  .. oo:: GET_METADATA_URL

      URL to the GetImageMetadata endpoint.
      Required if not specified in the connection string.

-  .. oo:: API_KEY

      API key for authentication. If specified, must
      be used together with the :oo:`CLIENT_ID` option. Can be specified also
      through the :config:`GDAL_DAAS_API_KEY` configuration option.

-  .. oo:: CLIENT_ID

      Client id for authentication. If specified,
      must be used together with the :oo:`API_KEY` option. Can be specified also
      through the :config:`GDAL_DAAS_CLIENT_ID` configuration option.

-  .. oo:: ACCESS_TOKEN

      Access token. Can be specified also through
      the :config:`GDAL_DAAS_ACCESS_TOKEN` configuration option. Exclusive of
      :oo:`API_KEY`/:oo:`CLIENT_ID`.

-  .. oo:: X_FORWARDED_USER

      User from which the request originates
      from. Can be specified also through the :config:`GDAL_DAAS_X_FORWARDED_USER`
      configuration option.

-  .. oo:: BLOCK_SIZE
      :choices: 64-8192
      :default: 512

      Size of a block in pixels requested to the server.

-  .. oo:: PIXEL_ENCODING
      :choices: AUTO, RAW, PNG, JPEG, JPEG2000
      :default: AUTO

      Format in which pixels are queried:

      -  **AUTO**: for 1, 3 or 4-band images of type Byte, resolves to PNG.
         Otherwise to RAW
      -  **RAW**: compatible of all images. Pixels are requested in a
         uncompressed raw format.
      -  **PNG**: compatible of 1, 3 or 4-band images of type Byte
      -  **JPEG**: compatible of 1 or 3-band images of type Byte
      -  **JPEG2000**: compatible of all images. Requires GDAL to be built
         with one of its JPEG2000-capable drivers.

-  .. oo:: MASKS
      :choices: YES, NO
      :default: YES

      Whether to expose mask bands.
