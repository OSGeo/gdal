.. _vector.wfs:

WFS - OGC WFS service
=====================

.. shortname:: WFS

.. build_dependencies:: libcurl

This driver can connect to a OGC WFS service. It supports WFS 1.0, 1.1
and 2.0 protocols. GDAL/OGR must be built with Curl support in order for
the WFS driver to be compiled. Usually WFS requests return results in
GML format, so the GML driver should generally be set-up for read
support (thus requiring GDAL/OGR to be built with Xerces or Expat
support). It is sometimes possible to use alternate underlying formats
when the server supports them (such as OUTPUTFORMAT=json).

The driver supports read-only services, as well as transactional ones
(WFS-T).

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a WFS datasource is :
*WFS:http://path/to/WFS/service* or
*http://path/to/WFS/service?SERVICE=WFS*

Additional optional parameters can be specified such as *TYPENAME*,
*VERSION*, *MAXFEATURES* (WFS < 2) or *COUNT* (WFS > 2) as specified in WFS specification.

The name provided to the TYPENAME parameter must be exactly the layer
name reported by OGR, in particular with its namespace prefix when its
exists. Note: several type names can be provided and separated by comma.

It is also possible to specify the name of an XML file whose content
matches the following syntax (the <OGRWFSDataSource> element must be the
first bytes of the file):

::

   <OGRWFSDataSource>
       <URL>http://path/to/WFS/service[?OPTIONAL_PARAMETER1=VALUE[&amp;OPTIONAL_PARAMETER2=VALUE]]</URL>
   </OGRWFSDataSource>

Note: the URL must be XML-escaped, for example the *&* character must be
written as *&amp;*

At the first opening, the content of the result of the *GetCapabilities*
request will be appended to the file, so that it can be cached for later
openings of the dataset. The same applies for the *DescribeFeatureType*
request issued to discover the field definition of each layer.

The service description file has the following additional elements as
immediate children of the ``OGRWFSDataSource`` element that may be
optionally set.

-  **Timeout**: The timeout to use for remote service requests. If not
   provided, the libcurl default is used.
-  **UserPwd**: May be supplied with *userid:password* to pass a userid
   and password to the remote server.
-  **HttpAuth**: May be BASIC, NTLM or ANY to control the authentication
   scheme to be used.
-  **Version**: Set a specific WFS version to use (either 1.0.0 or
   1.1.0).
-  **PagingAllowed**: Set to ON if paging must be enabled. See "Request
   paging" section.
-  **PageSize**: Page size when paging is enabled. See "Request paging"
   section.
-  **BaseStartIndex**: Base of the start index when paging
   is enabled (0 or 1). See "Request paging" section.
-  **COOKIE**: HTTP cookies that are passed in HTTP requests, formatted
   as COOKIE1=VALUE1; COOKIE2=VALUE2... Starting with GDAL 2.3, additional
   HTTP headers can be sent by setting the GDAL_HTTP_HEADER_FILE configuration
   option to point to a filename of a text file with “key: value” HTTP headers.

Request paging
--------------

The WFS driver will read the GML content as a
stream instead as a whole file, which will improve interactivity and
help when the content cannot fit into memory. This can be turned off by
setting the OGR_WFS_USE_STREAMING configuration option to NO if this is
not desirable (for example, when iterating several times on a layer that
can fit into memory). When streaming is enabled, GZip compression is
also requested. It has been observed that some WFS servers, that cannot
do on-the-fly compression, will cache on their side the whole content to
be sent before sending the first bytes on the wire. To avoid this, you
can set the CPL_CURL_GZIP configuration option to NO.

Paging with WFS 2.0
+++++++++++++++++++

The WFS driver will automatically detect if server supports paging, when
requesting a WFS 2.0 server. The page size (number of features fetched in a
single request) is limited to 100 by default when not declared by the server.
It can be changed by setting the OGR_WFS_PAGE_SIZE configuration option, or by
specifying COUNT as a query parameter in the URL of the connection string.

If only the N first features must be downloaded and paging through the whole
layer is not desirable, the OGR_WFS_PAGING_ALLOWED configuration option should
be set to OFF.

Paging with WFS 1.0 or 1.1
++++++++++++++++++++++++++

Some servers (such as MapServer >= 6.0) support the use of STARTINDEX
that allows doing the requests per "page", and thus to avoid
downloading the whole content of the layer in a single request. Paging
was introduced in WFS 2.0.0 but servers may support it as an vendor
specific option also with WFS 1.0.0 and 1.1.0. The OGR WFS client will
use paging when the OGR_WFS_PAGING_ALLOWED configuration option is explicitly set
to ON. The page size (number of features fetched in a single request)
is limited to 100 by default when not declared by the server.
It can be changed by setting the OGR_WFS_PAGE_SIZE configuration option.

WFS 2.0.2 specification has clarified that the first feature in paging
is at index 0. But some server implementations of WFS paging have
considered that it was at index 1 (including MapServer <= 6.2).
The default base start index is 0, as mandated
by the specification. The OGR_WFS_BASE_START_INDEX configuration
option can however be set to 1 to be compatible with the server
implementations that considered the first feature to be at index 1.

Paging options
++++++++++++++

Those 3 options (OGR_WFS_PAGING_ALLOWED, OGR_WFS_PAGE_SIZE,
OGR_WFS_BASE_START_INDEX) can also be set in a WFS XML description
file with the elements of similar names (PagingAllowed, PageSize,
BaseStartIndex).

Filtering
---------

The driver will forward any spatial filter set with SetSpatialFilter()
to the server. It also makes its best effort to do the same for
attribute filters set with SetAttributeFilter() when possible (turning
OGR SQL language into OGC filter description). When this is not
possible, it will default to client-side only filtering, which can be a
slow operation because involving fetching all the features from the
servers.

The following spatial functions can be used :

-  the 8 spatial binary predicate: **ST_Equals, ST_Disjoint, ST_Touches,
   ST_Contains, ST_Intersects, ST_Within, ST_Crosses and ST_Overlaps**
   that take 2 geometry arguments. Typically the geometry column name,
   and a constant geometry such as built with ST_MakeEnvelope or
   ST_GeomFromText.
-  **ST_DWithin(geom1,geom2,distance_in_meters)**
-  **ST_Beyond(geom1,geom2,distance_in_meters)**
-  **ST_MakeEnvelope(xmin,ymin,xmax,ymax[,srs])**: to build an envelope.
   srs can be an integer (an EPSG code), or a string directly set as the
   srsName attribute of the gml:Envelope. GDAL will take care of needed
   axis swapping, so coordinates should be expressed in the "natural GIS
   order" (for example long,lat for geodetic systems)
-  **ST_GeomFromText(wkt,[srs])**: to build a geometry from its WKT
   representation.

Note that those spatial functions are only supported as server-side
filters.

Layer joins
-----------

For WFS 2.0 servers that support joins,
SELECT statements that involve joins will be run on server side. Spatial
joins can also be done by using the above mentioned spatial functions,
if the server supports spatial joins.

There might be restrictions set by server on the complexity of the
joins. The OGR WFS driver also restricts column selection to be column
names, potentially with aliases and type casts, but not expressions. The
ON and WHERE clauses must also be evaluated on server side, so no OGR
special fields are allowed for example. ORDER BY clauses are supported,
but the fields must belong to the primary table.

Example of valid statement :

::

   SELECT t1.id, t1.val1, t1.geom, t2.val1 FROM my_table AS t1 JOIN another_table AS t2 ON t1.id = t2.t1id

or

::

   SELECT * FROM my_table AS t1 JOIN another_table AS t2 ON ST_Intersects(t1.geom, t2.geom)

Write support / WFS-T
---------------------

The WFS-T protocol only enables the user to operate at feature level. No
datasource, layer or field creations are possible.

Write support is only enabled when the datasource is opened in update
mode.

The mapping between the operations of the WFS Transaction service and
the OGR concepts is the following:

-  OGRFeature::CreateFeature() <==> WFS insert operation
-  OGRFeature::SetFeature() <==> WFS update operation
-  OGRFeature::DeleteFeature() <==> WFS delete operation

Lock operations (LockFeature service) are not available at that time.

There are a few caveats to keep in mind. OGR feature ID (FID) is an
integer based value, whereas WFS/GML gml:id attribute is a string. Thus
it is not always possible to match both values. The WFS driver exposes
then the gml:id attribute of a feature as a 'gml_id' field.

When inserting a new feature with CreateFeature(), and if the command is
successful, OGR will fetch the returned gml:id and set the 'gml_id'
field of the feature accordingly. It will also try to set the OGR FID if
the gml:id is of the form layer_name.numeric_value. Otherwise the FID
will be left to its unset default value.

When updating an existing feature with SetFeature(), the OGR FID field
will be ignored. The request issued to the driver will only take into
account the value of the gml:id field of the feature. The same applies
for DeleteFeature().

Write support and OGR transactions
----------------------------------

The above operations are by default issued to the server synchronously
with the OGR API call. This however can cause performance penalties when
issuing a lot of commands due to many client/server exchanges.

It is possible to surround those operations between
OGRLayer::StartTransaction() and OGRLayer::CommitTransaction(). The
operations will be stored into memory and only executed at the time
CommitTransaction() is called.

The drawback for CreateFeature() is that the user cannot know which
gml:id have been assigned to the inserted features. A special SQL
statement has been introduced into the WFS driver to workaround this :
by issuing the "SELECT \_LAST_INSERTED_FIDS\_ FROM layer_name" (where
layer_name is to be replaced with the actual layer_name) command through
the OGRDataSource::ExecuteSQL(), a layer will be returned with as many
rows with a single attribute gml_id as the count of inserted features
during the last committed transaction.

Note : currently, only CreateFeature() makes use of OGR transaction
mechanism. SetFeature() and DeleteFeature() will still be issued
immediately.

Special SQL commands
--------------------

The following SQL / pseudo-SQL commands passed to
OGRDataSource::ExecuteSQL() are specific of the WFS driver :

-  "DELETE FROM layer_name WHERE expression" : this will result into a
   WFS delete operation. This can be a fast way of deleting one or
   several features. In particularly, this can be a faster replacement
   for OGRLayer::DeleteFeature() when the gml:id is known, but the
   feature has not been fetched from the server.

-  "SELECT \_LAST_INSERTED_FIDS\_ FROM layer_name" : see above
   paragraph.

Currently, any other SQL command will be processed by the generic layer,
meaning client-side only processing. Server side spatial and attribute
filtering must be done through the SetSpatialFilter() and
SetAttributeFilter() interfaces.

Special layer : WFSLayerMetadata
--------------------------------

A "hidden" layer called "WFSLayerMetadata" is filled with records with
metadata for each WFS layer.

Each record contains a "layer_name", "title" and "abstract" field, from
the document returned by GetCapabilities.

That layer is returned through GetLayerByName("WFSLayerMetadata").

Special layer : WFSGetCapabilities
----------------------------------

A "hidden" layer called "WFSGetCapabilities" is filled with the raw XML
result of the GetCapabilities request.

That layer is returned through GetLayerByName("WFSGetCapabilities").

Open options
------------

The following options are available:

-  **URL**\ =url: URL to the WFS server endpoint. Required when using
   the "WFS:" string as the connection string.
-  **TRUST_CAPABILITIES_BOUNDS**\ =YES/NO: Whether to trust layer bounds
   declared in GetCapabilities response, for faster GetExtent() runtime.
   Defaults to NO
-  **EMPTY_AS_NULL=YES/NO**: By default
   (EMPTY_AS_NULL=YES), fields with empty content will be reported as
   being NULL, instead of being an empty string. This is the historic
   behavior. However this will prevent such fields to be declared as
   not-nullable if the application schema declared them as mandatory. So
   this option can be set to NO to have both empty strings being report
   as such, and mandatory fields being reported as not nullable.
-  **INVERT_AXIS_ORDER_IF_LAT_LONG=YES/NO**: Whether to
   present SRS and coordinate ordering in traditional GIS order.
   Defaults to YES.
-  **CONSIDER_EPSG_AS_URN=YES/NO/AUTO**: Whether to
   consider srsName like EPSG:XXXX as respecting EPSG axis order.
   Defaults to AUTO.
-  **EXPOSE_GML_ID=YES/NO**: Whether to expose the gml:id
   attribute of a GML feature as the gml_id OGR field. Note that hiding
   gml_id will prevent WFS-T from working. Defaults to YES.

Examples
--------

Listing the types of a WFS server :

::

   ogrinfo -ro WFS:http://www2.dmsolutions.ca/cgi-bin/mswfs_gmap

Listing the types of a WFS server whose layer structures are cached in a
XML file:

::

   ogrinfo -ro mswfs_gmap.xml

Listing the features of the popplace layer, with a spatial filter :

::

   ogrinfo -ro WFS:http://www2.dmsolutions.ca/cgi-bin/mswfs_gmap popplace -spat 0 0 2961766.250000 3798856.750000

Retrieving the features of gml:id "world.2" and "world.3" from the
tows:world layer :

::

   ogrinfo "WFS:http://www.tinyows.org/cgi-bin/tinyows" tows:world -ro -al -where "gml_id='world.2' or gml_id='world.3'"

Display layer metadata:

::

   ogrinfo -ro -al "WFS:http://v2.suite.opengeo.org/geoserver/ows" WFSLayerMetadata

See Also
--------

-  `OGC WFS Standard <http://www.opengeospatial.org/standards/wfs>`__
-  :ref:`GML driver documentation <vector.gml>`
-  :ref:`OGC API - Features driver documentation <vector.oapif>`
