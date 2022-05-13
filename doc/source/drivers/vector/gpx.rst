.. _vector.gpx:

GPX - GPS Exchange Format
=========================

.. shortname:: GPX

.. build_dependencies:: (read support needs libexpat) 

GPX (the GPS Exchange Format) is a light-weight XML data format for the
interchange of GPS data (waypoints, routes, and tracks) between
applications and Web services on the Internet.

OGR has support for GPX reading (if GDAL is build with *expat* library
support) and writing.

Version supported are GPX 1.0 and 1.1 for reading, GPX 1.1 for writing.

The OGR driver supports reading and writing of all the GPX feature types
:

-  *waypoints* : layer of features of OGR type wkbPoint
-  *routes* : layer of features of OGR type wkbLineString
-  *tracks* : layer of features of OGR type wkbMultiLineString

It also supports reading of route points and track points in standalone
layers (*route_points* and *track_points*), so that their own attributes
can be used by OGR.

| In addition to its GPX attributes, each route point of a route has a
  *route_fid* (foreign key to the FID of its belonging route) and a
  *route_point_id* which is its sequence number in the route.
| The same applies for track points with *track_fid*, *track_seg_id* and
  *track_seg_point_id*. All coordinates are relative to the WGS84 datum
  (EPSG:4326).

If the environment variable GPX_ELE_AS_25D is set to YES, the elevation
element will be used to set the Z coordinates of waypoints, route points
and track points.

The OGR/GPX reads and writes the GPX attributes for the waypoints,
routes and tracks.

By default, up to 2 *<link>* elements can be taken into account by
feature. This default number can be changed with the GPX_N_MAX_LINKS
environment variable.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Encoding issues
---------------

Expat library supports reading the following built-in encodings :

-  US-ASCII
-  UTF-8
-  UTF-16
-  ISO-8859-1
-  Windows-1252

The content returned by OGR will be encoded in UTF-8, after the
conversion from the encoding mentioned in the file header is.

| If your GPX file is not encoded in one of the previous encodings, it
  will not be parsed by the GPX driver. You may convert it into one of
  the supported encoding with the *iconv* utility for example and change
  accordingly the *encoding* parameter value in the XML header.

When writing a GPX file, the driver expects UTF-8 content to be passed
in.

Extensions element reading
--------------------------

If the *<extensions>* element is detected in a GPX file, OGR will expose
the content of its sub elements as fields. Complex content of sub
elements will be exposed as an XML blob.

The following sequence GPX content :

::

       <extensions>
           <navaid:name>TOTAL RF</navaid:name>
           <navaid:address>BENSALEM</navaid:address>
           <navaid:state>PA</navaid:state>
           <navaid:country>US</navaid:country>
           <navaid:frequencies>
           <navaid:frequency type="CTAF" frequency="122.900" name="CTAF"/>
           </navaid:frequencies>
           <navaid:runways>
           <navaid:runway designation="H1" length="80" width="80" surface="ASPH-G">
           </navaid:runway>
           </navaid:runways>
           <navaid:magvar>12</navaid:magvar>
       </extensions>

will be interpreted in the OGR SF model as :

::

     navaid_name (String) = TOTAL RF
     navaid_address (String) = BENSALEM
     navaid_state (String) = PA
     navaid_country (String) = US
     navaid_frequencies (String) = <navaid:frequency type="CTAF" frequency="122.900" name="CTAF" ></navaid:frequency>
     navaid_runways (String) = <navaid:runway designation="H1" length="80" width="80" surface="ASPH-G" ></navaid:runway>
     navaid_magvar (Integer) = 12

| 
| Note : the GPX driver will output content of the extensions element
  only if it is found in the first records of the GPX file. If
  extensions appear later, you can force an explicit parsing of the
  whole file with the :decl_configoption:`GPX_USE_EXTENSIONS` configuration 
  option.

Creation Issues
---------------

On export all layers are written to a single GPX file. Update of
existing files is not currently supported.

If the output file already exits, the writing will not occur. You have
to delete the existing file first.

Supported geometries :

-  Features of type wkbPoint/wkbPoint25D are written in the *wpt*
   element.
-  Features of type wkbLineString/wkbLineString25D are written in the
   *rte* element.
-  Features of type wkbMultiLineString/wkbMultiLineString25D are written
   in the *trk* element.
-  Other type of geometries are not supported.

For route points and tracks points, if there is a Z coordinate, it is
used to fill the elevation element of the corresponding points.

If a layer is named "track_points" with
wkbPoint/wkbPoint25D geometries, the tracks in the GPX file will be
built from the sequence of features in that layer. This is the way of
setting GPX attributes for each track point, in addition to the raw
coordinates. Points belonging to the same track are identified thanks to
the same value of the 'track_fid' field (and it will be broken into
track segments according to the value of the 'track_seg_id' field). They
must be written in sequence so that track objects are properly
reconstructed. The 'track_name' field can be set on the first track
point to fill the <name> element of the track. Similarly, if a layer is
named "route_points" with wkbPoint/wkbPoint25D geometries, the routes in
the GPX file will be built from the sequence of points with the same
value of the 'route_fid' field. The 'route_name' field can be set on the
first track point to fill the <name> element of the route.

Layer creation options
----------------------

-  **FORCE_GPX_TRACK**: By default when writing a layer whose features
   are of type wkbLineString, the GPX driver chooses to write them as
   routes.
   If FORCE_GPX_TRACK=YES is specified, they will be written as tracks.
-  **FORCE_GPX_ROUTE**: By default when writing a layer whose features
   are of type wkbMultiLineString, the GPX driver chooses to write them
   as tracks.
   If FORCE_GPX_ROUTE=YES is specified, they will be written as routes,
   provided that the multilines are composed of only one single line.

Dataset creation options
------------------------

-  **GPX_USE_EXTENSIONS**: By default, the GPX driver will discard
   attribute fields that do not match the GPX XML definition (name, cmt,
   etc...).
   If GPX_USE_EXTENSIONS=YES is specified, extra fields will be written
   inside the\ *<extensions>* tag.
-  **GPX_EXTENSIONS_NS**: Only used if GPX_USE_EXTENSIONS=YES and
   GPX_EXTENSIONS_NS_URL is set.
   The namespace value used for extension tags. By default, "ogr".
-  **GPX_EXTENSIONS_NS_URL**: Only used if GPX_USE_EXTENSIONS=YES and
   GPX_EXTENSIONS_NS is set.
   The namespace URI. By default, "http://osgeo.org/gdal".
-  **LINEFORMAT**: By default files are created with
   the line termination conventions of the local platform (CR/LF on
   win32 or LF on all other systems). This may be overridden through use
   of the LINEFORMAT layer creation option which may have a value of
   **CRLF** (DOS format) or **LF** (Unix format).

Waypoints, routes and tracks must be written into that order to be valid
against the XML Schema.

When translating from a source dataset, it may be necessary to rename
the field names from the source dataset to the expected GPX attribute
names, such as <name>, <desc>, etc... This can be done with a :ref:`OGR
VRT <vector.vrt>` dataset, or by using the "-sql" option of the
ogr2ogr utility.

Issues when translating to Shapefile
------------------------------------

-  When translating the *track_points* layer to a Shapefile, the field
   names "track_seg_id" and "track_seg_point_id" are truncated to 10
   characters in the .DBF file, thus leading to duplicate names.

   To avoid this, you can define the
   :decl_configoption:`GPX_SHORT_NAMES` configuration option to TRUE to 
   make them be reported
   respectively as "trksegid" and "trksegptid", which will allow them to
   be unique once translated to DBF. The "route_point_id" field of
   *route_points* layer will also be renamed to "rteptid". But note that
   no particular processing will be done for any extension field names.

   To translate the track_points layer of a GPX file to a set of
   shapefiles :

   ::

          ogr2ogr --config GPX_SHORT_NAMES YES out input.gpx track_points

-  Shapefile does not support fields of type DateTime. It only supports
   fields of type Date. So by default, you will lose the
   hour:minute:second part of the *Time* elements of a GPX file.

   You can use the OGR SQL CAST operator to
   convert the *time* field to a string :

   ::

          ogr2ogr out input.gpx -sql "SELECT ele, CAST(time AS character(32)) FROM waypoints"

   There is a more convenient way to select
   all fields and ask for the conversion of the ones of a given type to
   strings:

   ::

          ogr2ogr out input.gpx -fieldTypeToString DateTime

VSI Virtual File System API support
-----------------------------------

The driver supports reading and writing to files managed by VSI Virtual
File System API, which include "regular" files, as well as files in the
/vsizip/ (read-write) , /vsigzip/ (read-write) , /vsicurl/ (read-only)
domains.

Writing to /dev/stdout or /vsistdout/ is also supported.

Example
-------

The ogrinfo utility can be used to dump the content of a GPX datafile :

::

   ogrinfo -ro -al input.gpx

| 

The ogr2ogr utility can be used to do GPX to GPX translation :

::

   ogr2ogr -f GPX output.gpx input.gpx waypoints routes tracks

| 
| Note : in the case of GPX to GPX translation, you need to specify the
  layer names, in order to discard the route_points and track_points
  layers.

| 

Use of the *<extensions>* tag for output :

::

   ogr2ogr -f GPX  -dsco GPX_USE_EXTENSIONS=YES output.gpx input

which will give an output like the following one :

.. code-block:: XML

       <?xml version="1.0"?>
       <gpx version="1.1" creator="GDAL 1.5dev"
       xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
       xmlns:ogr="http://osgeo.org/gdal"
       xmlns="http://www.topografix.com/GPX/1/1"
       xsi:schemaLocation="http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd">
       <wpt lat="1" lon="2">
       <extensions>
           <ogr:Primary_ID>PID5</ogr:Primary_ID>
           <ogr:Secondary_ID>SID5</ogr:Secondary_ID>
       </extensions>
       </wpt>
       <wpt lat="3" lon="4">
       <extensions>
           <ogr:Primary_ID>PID4</ogr:Primary_ID>
           <ogr:Secondary_ID>SID4</ogr:Secondary_ID>
       </extensions>
       </wpt>
       </gpx>

Use of -sql option to remap field names to the ones allowed by the GPX
schema:

::

   ogr2ogr -f GPX output.gpx input.shp -sql "SELECT field1 AS name, field2 AS desc FROM input"

FAQ
---

How to solve "ERROR 6: Cannot create GPX layer XXXXXX with unknown
geometry type" ?

This error happens when the layer to create does not expose a precise
geometry type, but just a generic wkbUnknown type. This is for example
the case when using ogr2ogr with a SQL request to a PostgreSQL
datasource. You must then explicitly specify -nlt POINT (or LINESTRING
or MULTILINESTRING).

See Also
--------

-  `Home page for GPX format <http://www.topografix.com/gpx.asp>`__
-  `GPX 1.1 format documentation <http://www.topografix.com/GPX/1/1/>`__
