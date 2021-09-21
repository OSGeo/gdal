.. _vector.couchdb:

CouchDB - CouchDB/GeoCouch
==========================

.. shortname:: CouchDB

.. build_dependencies:: lilbcurl

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_COUCHDB

This driver can connect to the a CouchDB service, potentially enabled
with the GeoCouch spatial extension.

GDAL/OGR must be built with Curl support in order to the CouchDB driver
to be compiled.

The driver supports read and write operations.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

CouchDB vs OGR concepts
-----------------------

A CouchDB database is considered as a OGR layer. A CouchDB document is
considered as a OGR feature.

OGR handles preferably CouchDB documents following the GeoJSON
specification.

Dataset name syntax
-------------------

The syntax to open a CouchDB datasource is :

::

   couchdb:http://example.com[/layername]

where http://example.com points to the root of a CouchDB repository and,
optionally, layername is the name of a CouchDB database.

It is also possible to directly open a view :

::

   couchdb:http://example.com/layername/_design/adesigndoc/_view/aview[?include_docs=true]

The include_docs=true might be needed depending on the value returned by
the emit() call in the map() function.

Authentication
--------------

Some operations, in particular write operations, require authentication.
The authentication can be passed with the *COUCHDB_USERPWD* environment
variable set to user:password or directly in the URL.

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server when GeoCouch extension is available. It also makes the
same for (very simple) attribute filters set with SetAttributeFilter().
When server-side filtering fails, it will default back to client-side
filtering.

By default, the driver will try the following spatial filter function
"_design/ogr_spatial/_spatial/spatial", which is the valid spatial
filter function for layers created by OGR. If that filter function does
not exist, but another one exists, you can specify it with the
COUCHDB_SPATIAL_FILTER configuration option.

Note that the first time an attribute request is issued, it might
require write permissions in the database to create a new index view.

Paging
------

Features are retrieved from the server by chunks of 500 by default. This
number can be altered with the COUCHDB_PAGE_SIZE configuration option.

Write support
-------------

Table creation and deletion is possible.

Write support is only enabled when the datasource is opened in update
mode.

When inserting a new feature with CreateFeature(), and if the command is
successful, OGR will fetch the returned \_id and \_rev and use them.

Write support and OGR transactions
----------------------------------

The CreateFeature()/SetFeature() operations are by default issued to the
server synchronously with the OGR API call. This however can cause
performance penalties when issuing a lot of commands due to many
client/server exchanges.

It is possible to surround the CreateFeature()/SetFeature() operations
between OGRLayer::StartTransaction() and OGRLayer::CommitTransaction().
The operations will be stored into memory and only executed at the time
CommitTransaction() is called.

Layer creation options
----------------------

The following layer creation options are supported:

-  **UPDATE_PERMISSIONS** = LOGGED_USER|ALL|ADMIN|function(...)|DEFAULT
   : Update permissions for the new layer.

   -  If set to LOGGED_USER (the default), only logged users will be
      able to make changes in the layer.
   -  If set to ALL, all users will be able to make changes in the
      layer.
   -  If set to ADMIN, only administrators will be able to make changes
      in the layer.
   -  If beginning with "function(", the value of the creation option
      will be used as the content of the `validate_doc_update
      function <http://guide.couchdb.org/draft/validation.html>`__.
   -  Otherwise, all users will be allowed to make changes in non-design
      documents.

-  **GEOJSON** = YES|NO : Set to NO to avoid writing documents as
   GeoJSON documents. Default to YES.
-  **COORDINATE_PRECISION** = int_number : Maximum number of figures
   after decimal separator to write in coordinates. Default to 15.
   "Smart" truncation will occur to remove trailing zeros. Note: when
   opening a dataset in update mode, the
   OGR_COUCHDB_COORDINATE_PRECISION configuration option can be set to
   have a similar role.

Examples
--------

Listing the tables of a CouchDB repository:

::

   ogrinfo -ro "couchdb:http://some_account.some_couchdb_server.com"

Creating and populating a table from a shapefile:

::

   ogr2ogr -f couchdb "couchdb:http://some_account.some_couchdb_server.com" shapefile.shp

See Also
--------

-  `CouchDB reference <http://wiki.apache.org/couchdb/Reference>`__
-  `GeoCouch source code
   repository <http://github.com/couchbase/geocouch>`__
-  `Documentation for 'validate_doc_update'
   function <http://guide.couchdb.org/draft/validation.html>`__
