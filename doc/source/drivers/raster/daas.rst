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
   respectively with the API_KEY open option (or :decl_configoption:`GDAL_DAAS_API_KEY`
   configuration option) and the CLIENT_ID open option (or
   :decl_configoption:`GDAL_DAAS_CLIENT_Id` configuration option). In that case, the driver will
   authenticate against the authentication endpoint to get an access
   token.
-  Directly providing the access token with the ACCESS_TOKEN open option
   (or :decl_configoption:`GDAL_DAAS_ACCESS_TOKEN` configuration option).

In both cases, the X_FORWARDED_USER open option (or
:decl_configoption:`GDAL_DAAS_X_FORWARDED_USER` configuration option) can be specified to
fill the HTTP X-Forwarded-User header in requests sent to the DAAS
service endpoint with the user from which the request originates from.

See https://api.oneatlas.airbus.com/guides/g-authentication/ for further
details

Open options
------------

The following open options are available :

-  **GET_METADATA_URL**\ =value: URL to the GetImageMetadata endpoint.
   Required if not specified in the connection string.
-  **API_KEY**\ =value: API key for authentication. If specified, must
   be used together with the CLIENT_ID option. Can be specified also
   through the GDAL_DAAS_API_KEY configuration option.
-  **CLIENT_ID**\ =value: Client id for authentication. If specified,
   must be used together with the API_KEY option. Can be specified also
   through the GDAL_DAAS_CLIENT_ID configuration option.
-  **ACCESS_TOKEN**\ =value: Access token. Can be specified also through
   the GDAL_DAAS_ACCESS_TOKEN configuration option. Exclusive of
   API_KEY/CLIENT_ID
-  **X_FORWARDED_USER**\ =value: User from which the request originates
   from. Can be specified also through the GDAL_DAAS_X_FORWARDED_USER
   configuration option.
-  **BLOCK_SIZE**\ =value. Size of a block in pixels requested to the
   server. Defaults to 512 pixels. Between 64 and 8192.
-  **PIXEL_ENCODING**\ =value. Format in which pixels are queried.
   Defaults to

   -  **AUTO**: for 1, 3 or 4-band images of type Byte, resolves to PNG.
      Otherwise to RAW
   -  **RAW**: compatible of all images. Pixels are requested in a
      uncompressed raw format.
   -  **PNG**: compatible of 1, 3 or 4-band images of type Byte
   -  **JPEG**: compatible of 1 or 3-band images of type Byte
   -  **JPEG2000**: compatible of all images. Requires GDAL to be built
      with one of its JPEG2000-capable drivers.

-  **MASKS**\ =YES/NO. Whether to expose mask bands. Default to YES.
