.. _vector.mongodb:

MongoDB
=======

.. versionadded:: 2.1

.. shortname:: MongoDB

.. build_dependencies:: Mongo C++ client legacy library

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_MONGODB
   message: You should consider using the MongoDBV3 driver instead.

This driver can connect to the a MongoDB service.

The driver supports read, creation, update and delete operations of
documents/features and collections/layers. The MongoDB database must
exist before operating on it with OGR.

This driver uses the legacy MongoDB C++ driver client library. To
connect to MongoDB 3.0 or later servers, starting with GDAL 3.0, use the
new :ref:`MongoDBv3 <vector.mongodbv3>` driver which uses the MongoDB C++
v3.4.0 client library. This driver will be eventually in favor of
MongoDBv3

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
   URI <http://docs.mongodb.org/v2.6/reference/connection-string/>`__,
   such as
   mongodb://[usr:pwd@]host1[:port1]...[,hostN[:portN]]][/[db][?options]]
-  One using just MongoDB: as the name and open options to specify host,
   port, user, password, database, etc...

The open options available are :

-  **URI**\ =uri: `Connection
   URI <http://docs.mongodb.org/v2.6/reference/connection-string/>`__
-  **HOST**\ =hostname: Server hostname. Default to localhost.
-  **PORT**\ =port. Server port. Default to 27017.
-  **DBNAME**\ =dbname. Database name. Should be specified when
   connecting to hosts with user authentication enabled.
-  **AUTH_DBNAME**\ =dbname. Authentication database name, in case it is
   different from the database to work onto.
-  **USER**\ =name. User name.
-  **PASSWORD**\ =password. User password.
-  **AUTH_JSON**\ =json_string. Authentication elements as JSon object.
   This is for advanced authentication. The JSon fields to put in the
   dictionary are :

   -  "mechanism": The string name of the sasl mechanism to use
      (MONGODB-CR, SCRAM-SHA-1 or DEFAULT). Mandatory.
   -  "user": The string name of the user to authenticate. Mandatory.
   -  "db": The database target of the auth command, which identifies
      the location of the credential information for the user. May be
      "$external" if credential information is stored outside of the
      mongo cluster. Mandatory.
   -  "pwd": The password data.
   -  "digestPassword": Boolean, set to true if the "pwd" is undigested
      (default)
   -  "pwd": The password data.
   -  "serviceHostname": The GSSAPI hostname to use. Defaults to the
      name of the remote host.

-  **SSL_PEM_KEY_FILE**\ =filename. SSL PEM certificate/key filename.
-  **SSL_PEM_KEY_PASSWORD**\ =password. SSL PEM key password.
-  **SSL_CA_FILE**\ =filename. SSL Certification Authority filename.
-  **SSL_CRL_FILE**\ =filename. SSL Certification Revocation List
   filename.
-  **SSL_ALLOW_INVALID_CERTIFICATES**\ =YES/NO. Whether to allow
   connections to servers with invalid certificates. Defaults to NO.
-  **SSL_ALLOW_INVALID_HOSTNAMES**\ =YES/NO. Whether to allow
   connections to servers with non-matching hostnames. Defaults to NO.
-  **FIPS_MODE**\ =YES/NO. Whether to activate FIPS 140-2 mode at
   startup. Defaults to NO.
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

Note: the SSL\_\* and FIPS_MODE options must be set to the same values
when opening multiple types MongoDB databases. This is a limitation of
the Mongo C++ driver.

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server when a "2d" or "2dsphere" spatial index is available on
the geometry field.

However, in the current state, SQL attribute filters set with
SetAttributeFilter() are evaluated only on client-side. To enable
server-side filtering, the string passed to SetAttributeFilter() must be
a JSon object in the `MongoDB filter
syntax <http://docs.mongodb.org/v2.6/reference/method/db.collection.find/>`__.

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
command <http://docs.mongodb.org/v2.6/reference/command/>`__ can be
passed. The result will be returned as a JSon string in a single OGR
feature.

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

   ogrinfo -ro mongodb://user:password@ds047612.mongolab.com:47612/gdalautotest

Filtering on a MongoDB field:

::

   ogrinfo -ro mongodb://user:password@ds047612.mongolab.com:47612/gdalautotest -where '{ "field": 5 }'

Creating and populating a collection from a shapefile:

::

   ogr2ogr -update mongodb://user:password@ds047612.mongolab.com:47612/gdalautotest shapefile.shp

Build instructions
------------------

GDAL/OGR must be built against the `MongoDB C++ driver client
library <https://github.com/mongodb/mongo-cxx-driver>`__, in its
"legacy" version (tested with 1.0.2), in order to the MongoDB driver to
be compiled.

You must first follow `MongoDB C++ driver client build
instructions <https://github.com/mongodb/mongo-cxx-driver/wiki/Download-and-Compile-the-Legacy-Driver>`__,
which require to have Boost libraries available.

Then:

-  On Linux/Unix, run ./configure
   --with-mongocxx=/path/to/installation/root (if the driver is already
   installed in /usr, this is not needed). If the Boost libraries are
   not found in the system paths, the path to the directory when the
   libraries are found can be specified
   --with-boost-lib-path=/path/to/boost/libs .
-  On Windows, uncomment and adapt the following in nmake.opt (or add in
   nmake.local):

   ::

      # Uncomment for MongoDB support
      # This configuration is valid for a libmongoclient built as a DLL with:
      # scons.bat --32 --dynamic-windows --sharedclient --prefix=c:\users\even\dev\mongo-client-install
      #           --cpppath=c:\users\even\dev\boost_1_55_0_32bit --libpath=c:\users\even\dev\boost_1_55_0_32bit\lib32-msvc-10.0 install

      # Uncomment if plugin is preferred
      #MONGODB_PLUGIN = YES

      MONGODB_INC = c:/users/even/dev/mongo-client-install/include
      # Boost library names must be edited to reflect the actual MSVC and Boost versions
      BOOST_INC = c:/users/even/dev/boost_1_55_0_32bit
      BOOST_LIB_PATH= c:\users\even\dev\boost_1_55_0_32bit\lib32-msvc-10.0
      MONGODB_LIBS = c:/users/even/dev/mongo-client-install/lib/mongoclient.lib \
                     $(BOOST_LIB_PATH)\libboost_thread-vc100-mt-1_55.lib \
                     $(BOOST_LIB_PATH)\libboost_system-vc100-mt-1_55.lib \
                     $(BOOST_LIB_PATH)\libboost_date_time-vc100-mt-1_55.lib \
                     $(BOOST_LIB_PATH)\libboost_chrono-vc100-mt-1_55.lib

See Also
--------

-  `MongoDB C++ Driver <https://github.com/mongodb/mongo-cxx-driver>`__
-  `MongoDB 2.6 Manual <http://docs.mongodb.org/v2.6/reference/>`__
