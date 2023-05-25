.. _vector.eeda:

Google Earth Engine Data API
============================

.. versionadded:: 2.4

.. shortname:: EEDA

.. build_dependencies:: libcurl

The driver supports read-only operations to list images and their
metadata as a vector layer, using Google Earth Engine REST API.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a datasource is:

::

   EEDA:[collection]

where collection is something like
projects/earthengine-public/assets/COPERNICUS/S2.

Open options
------------

The following open options are available:

-  .. oo:: COLLECTION

      To specify the collection if not specified
      in the connection string.

Authentication methods
----------------------

The following authentication methods can be used:

-  Authentication Bearer header passed through the EEDA_BEARER or
   :config:`EEDA_BEARER_FILE` configuration options.
-  Service account private key file, through the
   :config:`GOOGLE_APPLICATION_CREDENTIALS` configuration option.
-  OAuth2 Service Account authentication through the
   :config:`EEDA_PRIVATE_KEY`/
   :config:`EEDA_PRIVATE_KEY_FILE` +
   :config:`EEDA_CLIENT_EMAIL` configuration options.
-  Finally if none of the above method succeeds, the code will check if
   the current machine is a Google Compute Engine instance, and if so
   will use the permissions associated to it (using the default service
   account associated with the VM). To force a machine to be detected as
   a GCE instance  you can set :config:`CPL_MACHINE_IS_GCE` to YES.

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are
available:

-  :copy-config:`EEDA_BEARER`

-  :copy-config:`EEDA_BEARER_FILE`

-  :copy-config:`GOOGLE_APPLICATION_CREDENTIALS`

-  :copy-config:`EEDA_PRIVATE_KEY`

-  :copy-config:`EEDA_PRIVATE_KEY_FILE`

-  :copy-config:`EEDA_CLIENT_EMAIL`

-  .. config:: EEDA_PAGE_SIZE
      :default: 1000

      Features are retrieved from the server by chunks
      of 1000 by default (and this is the maximum value accepted by the server). This number
      can be altered with this configuration option.


Attributes
----------

The layer field definition is built by requesting a single image from
the collection and guessing the schema from its "properties" element.
The "eedaconf.json" file from the GDAL configuration will also be read
to check if the collection schema is described in it, in which case the
above mentioned schema guessing will not done.

The following attributes will always be present:

.. table::
    :widths: 15, 10, 30, 20

    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | Field name          | Type      | Meaning                                                      | Server-side filter compatible |
    +=====================+===========+==============================================================+===============================+
    | name                | String    | Image name (e.g. projects/earthengine-public/                | No                            |
    |                     |           | assets/COPERNICUS/S2/20170430T190351\_                       |                               |
    |                     |           | 20170430T190351_T10SEG)                                      |                               |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | id                  | String    | Image ID; equivalent to name without the                     | No                            |
    |                     |           | "projects/\*/assets/" prefix (e.g. users/USER/ASSET)         |                               |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | path                | String    | (Deprecated) Image path; equivalent to id                    | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | gdal_dataset        | String    | GDAL dataset name (e.g.                                      | No                            |
    |                     |           | EEDAI:projects/earthengine-public/                           |                               |
    |                     |           | assets/COPERNICUS/S2/                                        |                               |
    |                     |           | 20170430T190351_20170430T190351\_                            |                               |
    |                     |           | T10SEG) that can be opened with the :ref:`raster.eedai`      |                               |
    |                     |           | driver                                                       |                               |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | startTime           | DateTime  | Acquisition start date                                       | **Yes** (restricted to >=     |
    |                     |           |                                                              | comparison on top level)      |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | endTime             | DateTime  | Acquisition end date                                         | **Yes** (restricted to <=     |
    |                     |           |                                                              | comparison on top level)      |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | updateTime          | DateTime  | Update date                                                  | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | sizeBytes           | Integer64 | File size in bytes                                           | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | band_count          | Integer   | Number of bands                                              | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | band_max_width      | Integer   | Maximum width among bands                                    | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | band_max_height     | Integer   | Maximum height among bands                                   | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | band_min_pixel_size | Real      | Minimum pixel size among bands                               | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | band_upper_left_x   | Real      | X origin (only set if equal among all bands)                 | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | band_upper_left_y   | Real      | Y origin (only set if equal among all bands)                 | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | band_crs            | String    | CRS as EPSG:XXXX or WKT (only set if equal among all bands)  | No                            |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+
    | other_properties    | String    | Serialized JSon dictionary with key/value pairs where key is | No                            |
    |                     |           | not a standalone field                                       |                               |
    +---------------------+-----------+--------------------------------------------------------------+-------------------------------+

"Server-side filter compatible" means that when this field is included
in an attribute filter, it is forwarded to the server (otherwise only
client-side filtering is done).

Geometry
~~~~~~~~

The footprint of each image is reported as a MultiPolygon with a
longitude/latitude WGS84 coordinate system (EPSG:4326).

Filtering
~~~~~~~~~

The driver will forward any spatial filter set with SetSpatialFilter()
to the server. It also makes the same for simple attribute filters set
with SetAttributeFilter(). The 3 boolean operators (AND, OR, NOT) and
the comparison operators (=, <>, <, <=, > and >=) are supported.

Paging
~~~~~~

Features are retrieved from the server by chunks of 1000 by default (and
this is the maximum value accepted by the server). This number can be
altered with the :config:`EEDA_PAGE_SIZE` configuration option.

Extent and feature count
~~~~~~~~~~~~~~~~~~~~~~~~

The reported extent and feature count will always be respectively
(-180,-90,180,90) and -1, given there is no way to get efficient answer
to those queries from the server.

Examples
~~~~~~~~

Listing all images available:

::

   ogrinfo -ro -al "EEDA:" -oo COLLECTION=projects/earthengine-public/assets/COPERNICUS/S2 --config EEDA_CLIENT_EMAIL "my@email" --config EEDA_PRIVATE_KEY_FILE my.pem

or

::

   ogrinfo -ro -al "EEDA:projects/earthengine-public/assets/COPERNICUS/S2" --config EEDA_CLIENT_EMAIL "my@email" --config EEDA_PRIVATE_KEY_FILE my.pem

Listing all images under a point of (lat,lon)=(40,-100) :

::

   ogrinfo -ro -al "EEDA:projects/earthengine-public/assets/COPERNICUS/S2" -spat -100 40 -100 40 --config EEDA_CLIENT_EMAIL "my@email" --config EEDA_PRIVATE_KEY_FILE my.pem

Listing all images available matching criteria :

::

   ogrinfo -ro -al "EEDA:projects/earthengine-public/assets/COPERNICUS/S2" -where "startTime >= '2015/03/26 00:00:00' AND endTime <= '2015/06/30 00:00:00' AND CLOUDY_PIXEL_PERCENTAGE < 10" --config EEDA_CLIENT_EMAIL "my@email" --config EEDA_PRIVATE_KEY_FILE my.pem

See Also:
---------

-  :ref:`Google Earth Engine Data API Image driver <raster.eedai>`
