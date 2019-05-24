.. _vector.ngw:

NGW -- NextGIS Web
==================

.. versionadded:: 2.4

.. shortname:: NGW

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
   Shape
-  Import map styles from QGIS project or set them manually
-  Act as a server for TMS, WMS, WFS
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

This driver can
connect to the services implementing the NextGIS Web API. GDAL/OGR must
be built with Curl support in order for the NGW driver to be compiled.
The driver supports read and write operations.

Dataset name syntax
-------------------

The minimal syntax to open a NGW datasource is: NGW:[NextGIS Web
URL][/resource/][resource identifier]

-  **NextGIS Web URL** may be an url to nextgis.com cloud service (for
   example, https://demo.nextgis.com), or some other url including port
   and additional path (for example, http://192.168.1.1:8000/test).
-  **resource** is mandatory keyword dividing resource identifier from
   the rest of URL.
-  **resource identifier** this is positive number from 0 and above.
   This may be a resource group, vector, PostGIS or raster layer, style.

If identifier is resource group, all vector layers, PostGIS, raster
layers, styles will listed as child resources. In other case this will
be a separate layer.

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

Authentication
--------------

Any operations (read, write, get metadata, change properties, etc.) may
require an authenticated access. Authenticated access is obtained by
specifying user name and password in open, create or configuration
options.

Feature
-------

If the NATIVE_DATA open option is set to YES, the *extensions* json
object will be stored as a serialized json object in the NativeData
property of the OGRFeature object (and "application/json" in the
NativeMediaType property). On write, if a OGRFeature to be written has
its NativeMediaType property set to "application/json" and its
NativeData property set to a string that is a serialized json object,
then members of this object will be set to *extensions* json object of
feature.

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
(available since NextGIS Web 3.1). This chunk size can be altered with
the NGW_PAGE_SIZE configuration option or PAGE_SIZE open option.

Write support
-------------

Datasource and layers creation and deletion is possible. Write support
is only enabled when the datasource is opened in update mode and user
has permissions. Vector and PostGIS layers insert and update operations
may be cached if BATCH_SIZE is greater 0. Delete operation executes
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
domain use the *SetMetadataItem* function and appropriate suffix. During
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

Filters
-------

Vector and PostGIS layers support SetIgnoredFields method. Any cached
features will be freed then this method executed.

Vector and PostGIS layers support SetAttributeFilter and
SetSpatialFilter methods. The SetAttributeFilter method accepts only
field equal value condition and AND operator without brackets. For
example,

::

   FIELD_1 = 'Value 1'

::

   FIELD_1 = 'Value 1' AND FIELD_2 = 'Value 2'

In other cases attribute filter will be evaluated on client side.

You can set attribute filter using NextGIS Web native format. For
example,

::

   NGW:fld_FIELD_1=Value 1&fld_FIELD_2=Value 2

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

Read datasource contensts (1730 is resource group identifier):

::

       ogrinfo -ro NGW:https://demo.nextgis.com/resource/1730

Read layer details (1730 is resource group identifier):

::

       ogrinfo -ro -so NGW:https://demo.nextgis.com/resource/1730 Parks

Creating and populating a vector layer from a shapefile in resource
group with identifier 1730. NEw vector layer name will be "myshapefile":

::

       ogr2ogr -f NGW "NGW:https://demo.nextgis.com/resource/1730/myshapefile" myshapefile.shp

See also
--------

-  :ref:`Raster side of the driver <raster.ngw>`
-  `NextGIS Web
   documentation <http://docs.nextgis.com/docs_ngweb/source/toc.html>`__
-  `NextGIS Web for
   developers <http://docs.nextgis.com/docs_ngweb_dev/doc/toc.html>`__
