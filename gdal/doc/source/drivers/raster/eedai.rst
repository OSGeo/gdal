.. _raster.eedai:

================================================================================
EEDAI - Google Earth Engine Data API Image
================================================================================

.. shortname:: EEDAI

.. versionadded:: 2.4

.. build_dependencies:: libcurl

The driver supports read-only operations to access image content, using
Google Earth Engine REST API.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a datasource is :

::

   EEDAI:[asset][:band_names]

where asset is something like
projects/earthengine-public/assets/COPERNICUS/S2/20170430T190351_20170430T190351_T10SEG,
and band_names a comma separated list of band names (typically indicated
by subdatasets on the main image)

Open options
------------

The following open options are available :

-  **ASSET**\ =string: To specify the asset if not specified in the
   connection string.
-  **BANDS**\ =bandname1[,bandnameX]*: Comma separated list of band
   names.
-  **PIXEL_ENCODING**\ =AUTO/PNG/JPEG/AUTO_JPEG_PNG/GEO_TIFF/NPY: Format
   in which to request pixels.
-  **BLOCK_SIZE**\ =integer: Size of a GDAL block, which is the minimum
   unit to query pixels. Default is 256.

Authentication methods
----------------------

The following authentication methods can be used:

-  Authentication Bearer header passed through the EEDA_BEARER or
   EEDA_BEARER_FILE configuration options.
-  Service account private key file, through the
   GOOGLE_APPLICATION_CREDENTIALS configuration option.
-  OAuth2 Service Account authentication through the EEDA_PRIVATE_KEY/
   EEDA_PRIVATE_KEY_FILE + EEDA_CLIENT_EMAIL configuration options.
-  Finally if none of the above method succeeds, the code will check if
   the current machine is a Google Compute Engine instance, and if so
   will use the permissions associated to it (using the default service
   account associated with the VM). To force a machine to be detected as
   a GCE instance (for example for code running in a container with no
   access to the boot logs), you can set CPL_MACHINE_IS_GCE to YES.

Configuration options
---------------------

The following configuration options are available :

-  **EEDA_BEARER**\ =value: Authentication Bearer value to pass to the
   API. This option is only useful when the token is computed by
   external code. The bearer validity is typically one hour from the
   time where it as been requested.
-  **EEDA_BEARER_FILE**\ =filename: Similar to EEDA_BEARER option,
   except than instead of passing the value directly, it is the filename
   where the value should be read.
-  **GOOGLE_APPLICATION_CREDENTIALS**\ =file.json: Service account
   private key file that contains a private key and client email
-  **EEDA_PRIVATE_KEY**\ =string: RSA private key encoded as a PKCS#8
   PEM file, with its header and footer. Used together with
   EEDA_CLIENT_EMAIL to use OAuth2 Service Account authentication.
   Requires GDAL to be built against libcrypto++ or libssl.
-  **EEDA_PRIVATE_KEY_FILE**\ =filename: Similar to EEDA_PRIVATE_KEY
   option, except than instead of passing the value directly, it is the
   filename where the key should be read.
-  **EEDA_CLIENT_EMAIL**\ =string: email to be specified together with
   EEDA_PRIVATE_KEY/EEDA_PRIVATE_KEY_FILE to use OAuth2 Service Account
   authentication.

Overviews
---------

The driver expose overviews, following a logic of decreasing power of 2
factors, until both dimensions of the smallest overview are lower than
256 pixels.

Subdatasets
-----------

When all bands don't have the same georeferencing, resolution, CRS or
image dimensions, the driver will expose subdatasets. Each subdataset
groups together bands of the same dimension, extent, resolution and CRS.

Metadata
--------

The driver will expose metadata reported in "properties" as
dataset-level or band-level metadata.

Pixel encoding
--------------

By default (PIXEL_ENCODING=AUTO), the driver will request pixels in a
format compatible of the number and data types of the bands. The PNG,
JPEG and AUTO_JPEG_PNG can only be used with bands of type Byte.

Examples
~~~~~~~~

Get metadata on an image:

::

   gdalinfo "EEDAI:" -oo ASSET=projects/earthengine-public/assets/COPERNICUS/S2/20170430T190351_20170430T190351_T10SEG --config EEDA_CLIENT_EMAIL "my@email" --config EEDA_PRIVATE_KEY_FILE my.pem

or

::

   gdalinfo "EEDAI:projects/earthengine-public/assets/COPERNICUS/S2/20170430T190351_20170430T190351_T10SEG" --config EEDA_CLIENT_EMAIL "my@email" --config EEDA_PRIVATE_KEY_FILE my.pem

See Also
--------

-  :ref:`Google Earth Engine Data API driver <vector.eeda>`
