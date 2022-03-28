.. _vector.mongodbv3:

MongoDBv3
=========

.. versionadded:: 3.0

.. shortname:: MongoDBv3

.. build_dependencies:: Mongo CXX >= 3.4.0 client library

This driver can connect to the a MongoDB service.

The driver supports read, creation, update and delete operations of
documents/features and collections/layers. The MongoDB database must
exist before operating on it with OGR.

This driver requires the MongoDB C++ v3.4.0 client library.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

MongoDB vs OGR concepts
-----------------------

A MongoDB collection within a database is considered as a OGR layer. A
MongoDB document is considered as a OGR feature.

Dataset name syntax
-------------------

There are two main possible syntaxes:

-  One using `MongoDB
   URI <https://docs.mongodb.com/manual/reference/connection-string/index.html>`__
   prefixed with *MONGODBV3:*, such as
   MONGODBV3:mongodb://[usr:pwd@]host1[:port1]...[,hostN[:portN]]][/[db][?options]]
-  One using just MongoDBv3: as the name and open options to specify
   host, port, user, password, database, etc...

Note: the MONGODBV3: prefix before a URI starting with *mongodb://* is
required to make it recognize by this driver, instead of the legacy
driver. If the URI is starting with
*mongodb+srv://*, then it is not needed.

The open options available are :

-  **URI**\ =uri: `Connection
   URI <https://docs.mongodb.com/manual/reference/connection-string/index.html>`__
-  **HOST**\ =hostname: Server hostname. Default to localhost.
-  **PORT**\ =port. Server port. Default to 27017.
-  **DBNAME**\ =dbname. Database name. Should be specified when
   connecting to hosts with user authentication enabled.
-  **USER**\ =name. User name.
-  **PASSWORD**\ =password. User password.
-  **SSL_PEM_KEY_FILE**\ =filename. SSL PEM certificate/key filename.
-  **SSL_PEM_KEY_PASSWORD**\ =password. SSL PEM key password.
-  **SSL_CA_FILE**\ =filename. SSL Certification Authority filename.
-  **SSL_CRL_FILE**\ =filename. SSL Certification Revocation List
   filename.
-  **SSL_ALLOW_INVALID_CERTIFICATES**\ =YES/NO. Whether to allow
   connections to servers with invalid certificates. Defaults to NO.
-  **BATCH_SIZE**\ =number. Number of features to retrieve per batch.
   For most queries, the first batch returns 101 documents or just
   enough documents to exceed 1 megabyte. Subsequent batch size is 4
   megabytes.
-  **FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN**\ =number. Number of
   features to retrieve to establish feature definition. -1 = unlimited.
   Defaults to 100.
-  **JSON_FIELD**\ =YES/NO. Whether to include a field called "_json"
   with the full document as JSON. Defaults to NO.
-  **FLATTEN_NESTED_ATTRIBUTE**\ =YES/NO. Whether to recursively explore
   nested objects and produce flatten OGR attributes. Defaults to YES.
-  **FID**\ =name. Field name, with integer values, to use as FID.
   Defaults to ogc_fid.
-  **USE_OGR_METADATA**\ =YES/NO. Whether to use the \_ogr_metadata
   collection to read layer metadata. Defaults to YES.
-  **BULK_INSERT**\ =YES/NO. Whether to use bulk insert for feature
   creation. Defaults to YES.

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server when a "2d" or "2dsphere" spatial index is available on
the geometry field.

However, in the current state, SQL attribute filters set with
SetAttributeFilter() are evaluated only on client-side. To enable
server-side filtering, the string passed to SetAttributeFilter() must be
a JSon object in the `MongoDB filter
syntax <https://docs.mongodb.com/manual/reference/method/db.collection.find/index.html>`__.

Paging
------

Features are retrieved from the server by chunks of 101 documents or
just enough documents to exceed 1 megabyte. Subsequent batch size is 4
megabytes. This can be altered with the BATCH_SIZE open option.

Schema
------

When reading a MongoDB collection, OGR must establish the schema of
attribute and geometry fields, since, contrary to MongoDB collections
which are schema-less, OGR has a fixed schema concept.

In the general case, OGR will read the first 100 documents (can be
altered with the FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN open option) of
the collection and build the schema that best fit to the found fields
and values.

If the collection/layer has been previously created with OGR, a
\_ogr_metadata special collection contains the OGR schema, in which case
it will be directly used. It might be possible to ignore the schema
written in \_ogr_metadata by setting the USE_OGR_METADATA=NO open
option.

It is also possible to set the JSON_FIELD=YES open option so that a
\_json special field is added to the OGR schema. When reading MongoDB
documents as OGR features, the full JSon version of the document will be
stored in the \_json field. This might be useful in case of complex
documents or with data types that do not translate well in OGR data
types. On creation/update of documents, if the \_json field is present
and set, its content will be used directly (other fields will be
ignored).

Feature ID
----------

MongoDB have a special \_id field that contains the unique ID of the
document. This field is returned as an OGR field, but cannot be used as
the OGR special FeatureID field, which must be of integer type. By
default, OGR will try to read a potential 'ogc_fid' field to set the OGR
FeatureID. The name of this field to look up can be set with the FID
open option. If the field is not found, the FID returned by OGR will be
a sequential number starting at 1, but it is not guaranteed to be stable
at all.

ExecuteSQL() interface
----------------------

If specifying "MongoDB" as the dialect of ExecuteSQL(), a JSon string
with a serialized `MongoDB
command <https://docs.mongodb.com/manual/reference/command/index.html>`__
can be passed. The result will be returned as a JSon string in a single
OGR feature.

Standard SQL requests will be executed on client-side.

Write support
-------------

Layer/collection creation and deletion is possible.

Write support is only enabled when the datasource is opened in update
mode.

When inserting a new feature with CreateFeature(), and if the command is
successful, OGR will fetch the returned \_id and use it for the
SetFeature() operation.

Layer creation options
----------------------

The following layer creation options are supported:

-  **OVERWRITE**\ =YES/NO. Whether to overwrite an existing collection
   with the layer name to be created. Defaults to NO.
-  **GEOMETRY_NAME**\ =name. Name of geometry column. Defaults to
   'geometry'.
-  **SPATIAL_INDEX**\ =YES/NO. Whether to create a spatial index
   (2dsphere). Defaults to YES.
-  **FID**\ =string. Field name, with integer values, to use as FID.
   Defaults to 'ogc_fid'
-  **WRITE_OGR_METADATA**\ =YES/NO. Whether to create a description of
   layer fields in the \_ogr_metadata collection. Defaults to YES.
-  **DOT_AS_NESTED_FIELD**\ =YES/NO. Whether to consider dot character
   in field name as sub-document. Defaults to YES.
-  **IGNORE_SOURCE_ID**\ =YES/NO. Whether to ignore \_id field in
   features passed to CreateFeature(). Defaults to NO.

Examples
--------

Listing the tables of a MongoDB database:

::

   ogrinfo -ro mongodb+srv://user:password@cluster0-ox9uy.mongodb.net/test

Filtering on a MongoDB field:

::

   ogrinfo -ro mongodb+srv://user:password@cluster0-ox9uy.mongodb.net/test -where '{ "field": 5 }'

Creating and populating a collection from a shapefile:

::

   ogr2ogr -update mongodb+srv://user:password@cluster0-ox9uy.mongodb.net/test shapefile.shp

Build instructions
------------------

GDAL/OGR must be built against the `MongoDB C++ driver client
library <https://github.com/mongodb/mongo-cxx-driver>`__, v3.4.0, in
order to the MongoDBv3 driver to be compiled.

You must first follow `MongoDB C++ driver client build
instructions <http://mongocxx.org/mongocxx-v3/installation/>`__.

Then:

-  On Linux/Unix, run ./configure --with-mongocxxv3 (potentially by
   overriding PKG_CONFIG_PATH to point to the
   {INSTALLATION_PREFIX_OF_MONGOCXX}/lib/pkgconfig
-  On Windows, uncomment and adapt the following in nmake.opt (or add in
   nmake.local):

   ::

      # Uncomment for MongoDBv3 support
      # Uncomment following line if plugin is preferred
      #MONGODBV3_PLUGIN = YES
      BOOST_INC=E:/boost_1_69_0
      MONGOCXXV3_CFLAGS = -IE:/dev/install-mongocxx-3.4.0/include/mongocxx/v_noabi -IE:/dev/install-mongocxx-3.4.0/include/bsoncxx/v_noabi
      MONGOCXXV3_LIBS = E:/dev/install-mongocxx-3.4.0/lib/mongocxx.lib E:/dev/install-mongocxx-3.4.0/lib/bsoncxx.lib

See Also
--------

-  `MongoDB C++ Driver <https://github.com/mongodb/mongo-cxx-driver>`__
-  `MongoDB Manual <https://docs.mongodb.com/manual/>`__
