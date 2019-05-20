.. _vector.gft:

GFT - Google Fusion Tables
==========================

.. shortname:: GFT

This driver can connect to the Google Fusion Tables service. GDAL/OGR
must be built with Curl support in order to the GFT driver to be
compiled.

The driver supports read and write operations.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a GFT datasource is :

::

   GFT:

Additional optional parameters can be specified after the ':' sign such
as :

-  **tables=table_id1[,table_id2]\***: A list of table IDs. This is
   necessary when you need to access to public tables for example. If
   you want the table ID of a public table, or any other table that is
   not owned by the authenticated user, you have to visit the table in
   the Google Fusion Tables website and note the number at the end of
   the URL.
-  **protocol=https**: To use HTTPS protocol for all operations. By
   default, HTTP is used, except for the authentication operation where
   HTTPS is always used.
-  **auth=auth_key**: An authentication key as described below.
-  **access=access_token**: An access token as described below.
-  **refresh=refresh_token**: A refresh token as described below.

If several parameters are specified, they must be separated by a space.

Authentication
--------------

Most operations, in particular write operations, require a valid Google
account to provide authentication information to the driver. The only
exception is read-only access to public tables.

In order to create an authorization key, it is necessary to `login and
authorize <https://www.google.com/url?q=https%3A%2F%2Faccounts.google.com%2Fo%2Foauth2%2Fauth%3Fscope%3Dhttps%253A%252F%252Fwww.googleapis.com%252Fauth%252Ffusiontables%26state%3D%252Fprofile%26redirect_uri%3Durn%3Aietf%3Awg%3Aoauth%3A2.0%3Aoob%26response_type%3Dcode%26client_id%3D265656308688.apps.googleusercontent.com>`__
access to fusion tables for a Google (i.e. GMail) account. The resulting
authorization key can be turned into a refresh token for use OGR using
the gdal/swig/python/scripts/gdal_auth.py script distributed with GDAL
(available in GDAL/OGR >= 1.10.0). Note that auth tokens can only be
used once, while the resulting refresh token lasts indefinitely.

::

     gdal_auth.py auth2refresh auth_token

This refresh token can then be either set as a configuration option
(GFT_REFRESH_TOKEN) or included in the connection string (i.e.
GFT:refresh=\ *refresh_token*).

Generally OAuth2 credentials can be provided via these mechanisms:

-  Specifying an *access token* via the ``GFT_ACCESS_TOKEN``
   configuration/environment variable.
-  Specifying an *access token* via the ``access=`` clause in the GFT:
   connection string.
-  Specifying a *refresh token* via the ``GFT_REFRESH_TOKEN``
   configuration/environment variable.
-  Specifying an *refresh token* via the ``refresh=`` clause in the GFT:
   connection string.
-  Specifying a *auth key* via the ``GFT_AUTH``
   configuration/environment variable.
-  Specifying an *auth key* via the ``auth=`` clause in the GFT:
   connection string.

Geometry
--------

Geometries in GFT tables must be expressed in the geodetic WGS84 SRS.
GFT allows them to be encoded in different forms :

-  A single column with a "lat lon" or "lat,lon" string, where lat et
   lon are expressed in decimal degrees.
-  A single column with a KML string that is the representation of a
   Point, a LineString or a Polygon.
-  Two columns, one with the latitude and the other one with the
   longitude, both values expressed in decimal degrees.
-  A single column with an address known by the geocoding service of
   Google Maps.

Only the first 3 types are supported by OGR, not the last one.

Fusion tables can have multiple geometry columns per table. By default,
OGR will use the first geometry column it finds. It is possible to
select another column as the geometry column by specifying
*table_name(geometry_column_name)* as the layer name passed to
GetLayerByName().

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server. It also makes the same for attribute filters set with
SetAttributeFilter().

Paging
------

Features are retrieved from the server by chunks of 500 by default. This
number can be altered with the GFT_PAGE_SIZE configuration option.

Write support
-------------

Table creation and deletion is possible. Note that fields can only be
added to a table in which there are no features created yet.

Write support is only enabled when the datasource is opened in update
mode.

The mapping between the operations of the GFT service and the OGR
concepts is the following :

-  OGRFeature::CreateFeature() <==> INSERT operation
-  OGRFeature::SetFeature() <==> UPDATE operation
-  OGRFeature::DeleteFeature() <==> DELETE operation
-  OGRDataSource::CreateLayer() <==> CREATE TABLE operation
-  OGRDataSource::DeleteLayer() <==> DROP TABLE operation

When inserting a new feature with CreateFeature(), and if the command is
successful, OGR will fetch the returned rowid and use it as the OGR FID.
OGR will also automatically reproject its geometry into the geodetic
WGS84 SRS if needed (provided that the original SRS is attached to the
geometry).

Write support and OGR transactions
----------------------------------

The above operations are by default issued to the server synchronously
with the OGR API call. This however can cause performance penalties when
issuing a lot of commands due to many client/server exchanges.

It is possible to surround the CreateFeature() operation between
OGRLayer::StartTransaction() and OGRLayer::CommitTransaction(). The
operations will be stored into memory and only executed at the time
CommitTransaction() is called. Note that the GFT service only supports
up to 500 INSERTs and up to 1MB of content per transaction.

Note : only CreateFeature() makes use of OGR transaction mechanism.
SetFeature() and DeleteFeature() will still be issued immediately.

SQL
---

SQL commands provided to the OGRDataSource::ExecuteSQL() call are
executed on the server side, unless the OGRSQL dialect is specified. The
subset of SQL supported by the GFT service is described in the links at
the end of this page.

The SQL supported by the server understands only native table id, and
not the table names returned by OGR. For convenience, OGR will "patch"
your SQL command to replace the table name by the table id however.

Examples
--------

Listing the tables and views owned by the authenticated user:

::

   ogrinfo -ro "GFT:email=john.doe@example.com password=secret_password"

Creating and populating a table from a shapefile:

::

   ogr2ogr -f GFT "GFT:email=john.doe@example.com password=secret_password" shapefile.shp

Displaying the content of a public table with a spatial and attribute
filters:

::

   ogrinfo -ro "GFT:tables=224453" -al -spat 67 31.5 67.5 32 -where "'Attack on' = 'ENEMY'"

Getting the auth key:

::

   ogrinfo --config CPL_DEBUG ON "GFT:email=john.doe@example.com password=secret_password"

returns:

::

   HTTP: Fetch(https://www.google.com/accounts/ClientLogin)
   HTTP: These HTTP headers were set: Content-Type: application/x-www-form-urlencoded
   GFT: Auth key : A_HUGE_STRING_WITH_ALPHANUMERIC_AND_SPECIAL_CHARACTERS

Now, you can set the GFT_AUTH environment variable to that value and
simply use "GFT:" as the DSN.

See Also
--------

-  `Google Fusion Tables Developer's
   Guide <http://code.google.com/intl/fr/apis/fusiontables/docs/developers_guide.html>`__
-  `Google Fusion Tables Developer's
   Reference <http://code.google.com/intl/fr/apis/fusiontables/docs/developers_reference.html>`__
