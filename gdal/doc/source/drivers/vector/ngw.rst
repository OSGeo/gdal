.. _vector.ngw:

NGW -- NextGIS Web
==================

.. versionadded:: 2.4

.. shortname:: NGW

.. build_dependencies:: libcurl

NextGIS Web - is a server GIS, which allows to store and edit geodata
and to display maps in web browser. Also NextGIS Web can share geodata
with other NextGIS software.

NextGIS Web has the following features:

-  Display maps in a web browser (different maps with different layers
   and styles)
-  Flexible permissions management
-  Load geodata from PostGIS or import from GIS formats (ESRI Shape,
   GeoJSON or GeoTIFF)
-  Load vector geodata in the following formats: GeoJSON, CSV, ESRI
   Shape, Mapinfo tab
-  Import map styles from QGIS project or set them manually
-  Act as a server for TMS, WMS, MVT, WFS
-  Act as a client for WMS
-  User can add photos to records, change record attributes via web
   interface or WFS-T protocol

NextGIS Web - is an open source software (license GPL v2+, see `GNU
General Public License, version
2 <https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html>`__).

Driver capabilities
-------------------

.. supports_georeferencing::

Driver
------

The driver can connect to the services implementing the NextGIS Web REST API.
NGW driver requires cURL support in GDAL. The driver supports read and write
operations.

Dataset name syntax
-------------------

The minimal syntax to open a NGW datasource is: NGW:[NextGIS Web
URL][/resource/][resource identifier]

-  **NextGIS Web URL** may be an URL to nextgis.com cloud service (for
   example, https://demo.nextgis.com), or some other URL including port
   and additional path (for example, http://192.168.1.1:8000/test).
-  **resource** is mandatory keyword dividing resource identifier from
   the rest of URL.
-  **resource identifier** this is positive number from 0 and above.
   This may be a resource group, vector, PostGIS or raster layer, style.

All vector layers, PostGIS, raster layers, styles will list as child resources
if identifier is resource group. In other case this will be a separate layer.

Configuration options
---------------------

The following configuration options are available:

-  **NGW_USERPWD**: User name and password separated with colon.
   Optional and can be set using open options.
-  **NGW_BATCH_SIZE**: Size of feature insert and update operations
   cache before send to server. If batch size is -1 batch mode is
   disabled. Delete operation will execute immediately.
-  **NGW_PAGE_SIZE**: If supported by server, fetch features from remote
   server will use paging. The -1 value disables paging even it
   supported by server.
-  **NGW_NATIVE_DATA**: Whether to store the json *extensions* key in
   feature native data.
-  **NGW_JSON_DEPTH**: The depth of json response that can be parsed. If
   depth is greater than this value, parse error occurs.
-  **NGW_EXTENSIONS**: Comma separated extensions list. Available values are 
   `description` and `attachment`. This needed to fill native data.

Authentication
--------------

Any operations (read, write, get metadata, change properties, etc.) may
require an authenticated access. Authenticated access is obtained by
specifying user name and password in open, create or configuration
options.

Feature
-------

If the NATIVE_DATA open option is set to YES, the *extensions* json
object will store as a serialized json object in the NativeData
property of the OGRFeature object (and "application/json" in the
NativeMediaType property). If writing OGRFeature has NativeMediaType property
set to "application/json" and its NativeData property set to serialized json
object the new NGW feature *extensions* json object will fill from this json
object.

Extensions json object structure see in `NextGIS Web API
documentation <http://docs.nextgis.comu/docs_ngweb_dev/doc/developer/resource.html#feature>`__

Geometry
--------

NextGIS Web supports only one geometry column. Default spatial reference
is Web Mercator (EPSG:3857). The following geometry types are available:

-  POINT
-  LINESTRING
-  POLYGON
-  MULTIPOINT
-  MULTILINESTRING
-  MULTIPOLYGON

Geometry with Z value also supported.

Field data types
----------------

NextWeb supports only following field types:

-  OFTInteger
-  OFTInteger64
-  OFTReal
-  OFTString
-  OFTDate
-  OFTTime
-  OFTDateTime

Paging
------

Features can retrieved from NextGIS Web by chunks if supported by server
(available since NextGIS Web 3.1). The chunk size can be altered with
the NGW_PAGE_SIZE configuration option or PAGE_SIZE open option.

Write support
-------------

Datasource and layers creation and deletion is possible. Write support
is only enabled when the datasource is opened in update mode and user
has appropriate permissions. Vector and PostGIS layers insert and update operations
are cached if BATCH_SIZE is greater 0. Delete operation executes
immediately.

Open options
------------

The following open options are available:

-  USERPWD - Username and password, separated by colon.
-  PAGE_SIZE=-1 - Limit feature count while fetching from server.
   Default value is -1 - no limit.
-  BATCH_SIZE=-1 - Size of feature insert and update operations cache
   before send to server. If batch size is -1 batch mode is disabled.
   Default value is -1.
-  NATIVE_DATA=NO - Whether to store the json *extensions* key in
   feature native data. Default value is NO.
-  JSON_DEPTH=32 - The depth of json response that can be parsed. If
   depth is greater than this value, parse error occurs.
-  EXTENSIONS - Comma separated extensions list. Available values are 
   `description` and `attachment`. This needed to fill native data.

Dataset creation options
------------------------

The following dataset/datasource creation options are available:

-  KEY - Key value. Must be unique in whole NextGIS Web instance.
   Optional.
-  DESCRIPTION - Resource description. Optional.
-  USERPWD - Username and password, separated by colon.
-  PAGE_SIZE=-1 - Limit feature count while fetching from server.
   Default value is -1 - no limit.
-  BATCH_SIZE=-1 - Size of feature insert and update operations cache
   before send to server. If batch size is -1 batch mode is disable.
   Default value is -1.
-  NATIVE_DATA=NO - Whether to store the json *extensions* key in
   feature native data. Default value is NO.
-  JSON_DEPTH=32 - The depth of json response that can be parsed. If
   depth is greater than this value, parse error occurs.
-  EXTENSIONS - Comma separated extensions list. Available values are 
   `description` and `attachment`. This needed to fill native data.

Layer creation options
----------------------

The following layer creation options are available:

-  OVERWRITE - Whether to overwrite an existing table with the layer
   name to be created. The resource will delete and new one will
   created. This leads that resource identifier will change. Defaults to
   NO. Optional.
-  KEY - Key value. Must be unique in whole NextGIS Web instance.
   Optional.
-  DESCRIPTION - Resource description. Optional.

Metadata
--------

NextGIS Web metadata are supported in datasource, vector, PostGIS,
raster layers and styles. Metadata are stored at specific domain "NGW".
NextGIS Web supported metadata are strings and numbers. Metadata keys
with decimal numbers will have suffix **.d** and for real numbers -
**.f**. To create new metadata item, add new key=value pair in NGW
domain using the *SetMetadataItem* function and appropriate suffix. During
transferring to NextGIS Web, suffix will be omitted. You must ensure
that numbers correctly transform from string to number.

Resource description and key map to appropriate *description* and
*keyname* metadata items in default domain. Changing those metadata
items will cause an update of resource properties.

Resource creation date, type and parent identifier map to appropriate
read-only metadata items *creation_date*, *resource_type* and
*parent_id* in default domain.

Vector layer field properties (alias, identifier, label field, grid
visibility) map to layer metadata the following way:

-  field alias -> FIELD_{field number}_ALIAS (for example FIELD_0_ALIAS)
-  identifier -> FIELD_{field number}_ID (for example FIELD_0_ID)
-  label field -> FIELD_{field number}_LABEL_FIELD (for example
   FIELD_0_LABEL_FIELD)
-  grid visibility -> FIELD_{field number}_GRID_VISIBILITY (for example
   FIELD_0_GRID_VISIBILITY)

Starting from GDAL 3.3 field alias can be set/get via `SetAlternativeName`
and `GetAlternativeNameRef`.

Filters
-------

Vector and PostGIS layers support SetIgnoredFields method. When this method
executes any cached features will be freed.

Vector and PostGIS layers support SetAttributeFilter and
SetSpatialFilter methods. The attribute filter will evaluate at server side
if condition is one of following comparison operators:

 - greater (>)
 - lower (<)
 - greater or equal (>=)
 - lower or equal (<=)
 - equal (=)
 - not equal (!=)
 - LIKE SQL statement (for strings compare)
 - ILIKE SQL statement (for strings compare)

Also only AND operator without brackets supported between comparison. For example,

::

   FIELD_1 = 'Value 1'

::

   FIELD_1 = 'Value 1' AND FIELD_2 > Value 2

In other cases attribute filter will evaluate on client side.

You can set attribute filter using NextGIS Web native format. For
example,

::

   NGW:fld_FIELD_1=Value 1&fld_FIELD_2__gt=Value 2

Don't forget to add 'NGW:' perefix to where clause and 'fld\_' prefix to
field name.

Dataset supports ExecuteSQL method. Only the following queries are
supported:

-  DELLAYER: layer_name; - delete layer with layer_name.
-  DELETE FROM layer_name; - delete any features from layer with
   layer_name.
-  DROP TABLE layer_name; - delete layer with layer_name.
-  ALTER TABLE src_layer RENAME TO dst_layer; - rename layer.
-  SELECT field_1,field_2 FROM src_layer WHERE field_1 = 'Value 1' AND
   field_2 = 'Value 2';

In SELECT statement field list or asterisk can be provided. The WHERE
clause has same limitations as SetAttributeFilter method input.

Examples
--------

Read datasource contents (1730 is resource group identifier):

::

       ogrinfo -ro NGW:https://demo.nextgis.com/resource/1730

Read layer details (`1730` is resource group identifier, `Parks` is vecror layer
name):

::

       ogrinfo -ro -so NGW:https://demo.nextgis.com/resource/1730 Parks

Creating and populating a vector layer from a shapefile in existing resource
group with identifier 1730. New vector layer name will be "some new name":

::

       ogr2ogr -f NGW -nln "some new name" -update -doo "BATCH_SIZE=100" -t_srs EPSG:3857 "NGW:https://demo.nextgis.com/resource/1730" myshapefile.shp

.. warning::
   The `-update` key is mandatory, otherwise the destination datasource will
   silently delete. The `-t_srs EPSG:3857` key is mandatory because vector
   layers spatial reference in NextGIS Web can be only in EPSG:3857.

.. note::
   The `-doo "BATCH_SIZE=100"` key is recommended for speed up feature transferring.

Creating and populating a vector layer from a shapefile in new resource
group with name "new group" and parent identifier 1730. New vector layer name
will be "some new name":

::

       ogr2ogr -f NGW -nln "Название на русском языке" -dsco "BATCH_SIZE=100" -t_srs EPSG:3857 "NGW:https://demo.nextgis.com/resource/1730/new group" myshapefile.shp

See also
--------

-  :ref:`Raster side of the driver <raster.ngw>`
-  `NextGIS Web
   documentation <http://docs.nextgis.com/docs_ngweb/source/toc.html>`__
-  `NextGIS Web for
   developers <http://docs.nextgis.com/docs_ngweb_dev/doc/toc.html>`__
