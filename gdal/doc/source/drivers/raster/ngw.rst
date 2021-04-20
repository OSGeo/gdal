.. _raster.ngw:

NGW -- NextGIS Web
==================

.. versionadded:: 2.4

.. shortname:: NGW

.. build_dependencies:: libcurl

NextGIS Web - is a server GIS, which allows storing and editing geodata
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

NextGIS Web supports several raster types:

-  Raster style
-  Vector style
-  WMS layer
-  WMS Service
-  Web map as combination of raster and vector styles

Each NextGIS Web raster layer can have one or more raster styles.
Each NextGIS Web vector or PostGIS layer can have one or more vector
styles (QGIS qml or MapServer xml).
WMS layers from external WMS service have no styles.
WMS Service is usual WMS protocol implementation.

NGW driver supports only raster and vector styles and WMS layers.
You can get raster data as tiles or image (only tiles are supported
now).

The driver supports read and copy from existing source dataset
operations on rasters.

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
layers, styles will be listed as child resources. In other case this
will be a separate raster.

Configuration options
---------------------

The following configuration options are available:

-  **NGW_USERPWD**: User name and password separated with colon.
   Optional and can be set using open options.
-  **NGW_CACHE_EXPIRES**: Time in seconds cached files will stay valid.
   If cached file expires it is deleted when maximum size of cache is
   reached. Also expired file can be overwritten by the new one from
   web. Defaults to 604800 (7 days).
-  **NGW_CACHE_MAX_SIZE**: The cache maximum size in bytes. If cache
   reached maximum size, expired cached files will be deleted. Defaults
   to 67108864 (64Mb).
-  **NGW_JSON_DEPTH**: The depth of json response that can be parsed. If
   depth is greater than this value, parse error occurs.

Authentication
--------------

Any operations (read, write, get metadata, change properties, etc.) may
require an authenticated access. Authenticated access is obtained by
specifying user name and password in open, create or configuration
options.

Open options
------------

The following open options are available:

-  USERPWD - Username and password, separated by colon.
-  CACHE_EXPIRES=604800 - Time in seconds cached files will stay valid.
   If cached file expires it is deleted when maximum size of cache is
   reached. Also expired file can be overwritten by the new one from
   web. Defaults to 604800 (7 days).
-  CACHE_MAX_SIZE=67108864 - The cache maximum size in bytes. If cache
   reached maximum size, expired cached files will be deleted. Defaults
   to 67108864 (64Mb).
-  JSON_DEPTH=32 - The depth of json response that can be parsed. If
   depth is greater than this value, parse error occurs.

Create copy options
-------------------

NextGIS Web supports only GeoTIFF file format. Prior version 3.1 supported only
3 (RGB) or 4 (RGBA) bands rasters with datatype Byte. In CreateCopy function if
source dataset has GeoTIFF file format it will copy as is. For other formats the
additional transformation to temporary GeoTIFF file will execute.

The following copy options are available:

-  KEY - Key value. Must be unique in whole NextGIS Web instance. Optional.
-  DESCRIPTION - Resource description. Optional.
-  RASTER_STYLE_NAME - Raster style name. Optional. Default is same as raster
   layer name.
-  RASTER_QML_PATH - Path to QGIS QML raster style file. Optional for RGB/RGBA,
   for other bands count/pixel types is mandatory.
-  USERPWD - Username and password, separated by colon.
-  CACHE_EXPIRES=604800 - Time in seconds cached files will stay valid.
   If cached file expires it is deleted when maximum size of cache is
   reached. Also expired file can be overwritten by the new one from
   web. Defaults to 604800 (7 days).
-  CACHE_MAX_SIZE=67108864 - The cache maximum size in bytes. If cache
   reached maximum size, expired cached files will be deleted. Defaults
   to 67108864 (64Mb).
-  JSON_DEPTH=32 - The depth of json response that can be parsed. If
   depth is greater than this value, parse error occurs.

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

Examples
--------

Read datasource contensts (1730 is resource group identifier):

::

       gdalinfo NGW:https://demo.nextgis.com/resource/1730

Read raster details (1734 is raster layer identifier):

::

       gdalinfo NGW:https://demo.nextgis.com/resource/1734

See also
--------

-  :ref:`Vector side of the driver <vector.ngw>`
-  `NextGIS Web
   documentation <http://docs.nextgis.com/docs_ngweb/source/toc.html>`__
-  `NextGIS Web for
   developers <http://docs.nextgis.com/docs_ngweb_dev/doc/toc.html>`__
