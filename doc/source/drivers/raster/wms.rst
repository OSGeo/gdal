.. _raster.wms:

================================================================================
WMS -- Web Map Services
================================================================================

.. shortname:: WMS

.. build_dependencies:: libcurl

Accessing several different types of web image services is possible
using the WMS format in GDAL.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

XML description file
--------------------

Services are accessed by creating a local
service description XML file -- there are examples below for each of the
supported image services. It is important that there be no spaces or
other content before the ``<GDAL_WMS>`` element.

========================================================================== ===============================================================================================================================================================================================================================================================================================================================
<GDAL_WMS>
<Service name="WMS">                                                       Define what mini-driver to use, currently supported are: WMS, WorldWind, TileService, TMS, TiledWMS, VirtualEarth or AGS. (required)
<Version>1.1.1</Version>                                                   WMS version. (optional, defaults to 1.1.1)
<ServerUrl>http://host.domain.com/wms.cgi?</ServerUrl>                     WMS server URL. (required)
<SRS>EPSG:4326</SRS>                                                       Image projection (optional, defaults to EPSG:4326 in WMS and 102100 in AGS, WMS version 1.1.1 or below only and ArcGIS Server). For ArcGIS Server the spatial reference can be specified as either a well-known ID or as a `spatial reference json object <http://resources.arcgis.com/en/help/rest/apiref/geometry.html#sr>`__
<CRS>CRS:83</CRS>                                                          Image projection (optional, defaults to EPSG:4326, WMS version 1.3.0 or above only)
<ImageFormat>image/jpeg</ImageFormat>                                      Format in which to request data. Paletted formats like image/gif will be converted to RGB. (optional, defaults to image/jpeg)
<Transparent>FALSE</Transparent>                                           Set to TRUE to include "transparent=TRUE" in the WMS GetMap request (optional defaults to FALSE).  The request format and BandsCount need to support alpha.
<Layers>modis%2Cglobal_mosaic</Layers>                                     A URL encoded, comma separated string of layers (required, except for TiledWMS)
<TiledGroupName>Clementine</TiledGroupName>                                Comma separated list of layers. (required for TiledWMS)
<Styles></Styles>                                                          Comma separated list of styles. (optional)
<BBoxOrder>xyXY</BBoxOrder>                                                Reorder bbox coordinates arbitrarily. May be required for version 1.3 servers. (optional)
                                                                           x - low X coordinate, y - low Y coordinate, X - high X coordinate, Y - high Y coordinate
</Service>
<DataWindow>                                                               Define size and extents of the data. (required, except for TiledWMS and VirtualEarth)
<UpperLeftX>-180.0</UpperLeftX>                                            X (longitude) coordinate of upper-left corner. (optional, defaults to -180.0, except for VirtualEarth)
<UpperLeftY>90.0</UpperLeftY>                                              Y (latitude) coordinate of upper-left corner. (optional, defaults to 90.0, except for VirtualEarth)
<LowerRightX>180.0</LowerRightX>                                           X (longitude) coordinate of lower-right corner. (optional, defaults to 180.0, except for VirtualEarth)
<LowerRightY>-90.0</LowerRightY>                                           Y (latitude) coordinate of lower-right corner. (optional, defaults to -90.0, except for VirtualEarth)
<SizeX>2666666</SizeX>                                                     Image size in pixels.
<SizeY>1333333</SizeY>                                                     Image size in pixels.
<TileX>0</TileX>                                                           Added to tile X value at highest resolution. (ignored for WMS, tiled image sources only, optional, defaults to 0)
<TileY>0</TileY>                                                           Added to tile Y value at highest resolution. (ignored for WMS, tiled image sources only, optional, defaults to 0)
<TileLevel>0</TileLevel>                                                   Tile level at highest resolution. (tiled image sources only, optional, defaults to 0)
<TileCountX>0</TileCountX>                                                 Can be used to define image size, SizeX = TileCountX \* BlockSizeX \* 2\ :sup:`TileLevel`. (tiled image sources only, optional, defaults to 0)
<TileCountY>0</TileCountY>                                                 Can be used to define image size, SizeY = TileCountY \* BlockSizeY \* 2\ :sup:`TileLevel`. (tiled image sources only, optional, defaults to 0)
<YOrigin>top</YOrigin>                                                     Can be used to define the position of the Y origin with respect to the tile grid. Possible values are 'top', 'bottom', and 'default', where the default behavior is mini-driver-specific. (TMS mini-driver only, optional, defaults to 'bottom' for TMS)
</DataWindow>
<Projection>EPSG:4326</Projection>                                         Image projection (optional, defaults to value reported by mini-driver or EPSG:4326)
<IdentificationTolerance>2</IdentificationTolerance>                       Identification tolerance (optional, defaults to 2)
<BandsCount>3</BandsCount>                                                 Number of bands/channels, 1 for grayscale data, 3 for RGB, 4 for RGBA. (optional, defaults to 3)
<DataType>Byte</DataType>                                                  Band data type, one of: Byte, Int16, UInt16, Int32, UInt32, Float32, Float64, etc.. (optional, defaults to Byte)
<DataValues NoData="0 0 0" min="1 1 1" max="255 255 255" />                Define NoData and/or minimum and/or maximum value for bands. nodata_values, min_values, max_values can be one single value, or a value per band, with a space separator between value
<BlockSizeX>1024</BlockSizeX>                                              Block size in pixels. (optional, defaults to 1024, except for VirtualEarth)
<BlockSizeY>1024</BlockSizeY>                                              Block size in pixels. (optional, defaults to 1024, except for VirtualEarth)
<OverviewCount>10</OverviewCount>                                          Count of reduced resolution layers each having 2 times lower resolution. (optional, default is calculated at runtime)
<Cache>                                                                    Enable local disk cache. Allows for offline operation. (optional but present in the autogenerated service file, cache is disabled when not present or if it is overriden with GDAL_ENABLE_WMS_CACHE=NO)
<Path>./gdalwmscache</Path>                                                Location where to store cache files. It is safe to use same cache path for different data sources. /vsimem/ paths are supported allowing for temporary in-memory cache. (optional, defaults to ./gdalwmscache if GDAL_DEFAULT_WMS_CACHE_PATH configuration option is not specified)
<Depth>2</Depth>                                                           Number of directory layers. 2 will result in files being written as cache_path/A/B/ABCDEF... (optional, defaults to 2)
<Extension>.jpg</Extension>                                                Append to cache files. (optional, defaults to none)
<Type>file</Type>                                                          Cache type. Now supported only 'file' type. In 'file' cache type files are stored in file system folders. (optional, defaults to 'file')
<Expires>604800</Expires>                                                  Time in seconds cached files will stay valid. If cached file expires it is deleted when maximum size of cache is reached. Also expired file can be overwritten by the new one from web. Default value is 7 days (604800s).
<MaxSize>67108864</MaxSize>                                                The cache maximum size in bytes. If cache reached maximum size, expired cached files will be deleted. Default value is 64 Mb (67108864 bytes).
<CleanTimeout>120</CleanTimeout>                                           Clean Thread Run Timeout in seconds. How often to run the clean thread, which finds and deletes expired cached files. Default value is 120s. Use value of 0 to disable the Clean Thread (effectively unlimited cache size). If you intend to use very large cache size you might want to disable the cache clean or to use a much longer timeout as the time that takes to scan the cache files for expired cache files might be long. ("disabled" was the only option for GDAL <= 2.2; "120s" was the only option for 2.3 <= GDAL <= 3.1). 
<Unique>True</Unique>                                                      If set to true the path will appended with md5 hash of ServerURL. Default value is true.
</Cache>
<MaxConnections>2</MaxConnections>                                         Maximum number of simultaneous connections. (optional, defaults to 2). Can also be set with the :decl_configoption:`GDAL_MAX_CONNECTIONS` configuration option (GDAL >= 3.2)
<Timeout>300</Timeout>                                                     Connection timeout in seconds. (optional, defaults to 300)
<OfflineMode>true</OfflineMode>                                            Do not download any new images, use only what is in cache. Useful only with cache enabled. (optional, defaults to false)
<AdviseRead>true</AdviseRead>                                              Enable AdviseRead API call - download images into cache. (optional, defaults to false)
<VerifyAdviseRead>true</VerifyAdviseRead>                                  Open each downloaded image and do some basic checks before writing into cache. Disabling can save some CPU cycles if server is trusted to always return correct images. (optional, defaults to true)
<ClampRequests>false</ClampRequests>                                       Should requests, that otherwise would be partially outside of defined data window, be clipped resulting in smaller than block size request. (optional, defaults to true)
<UserAgent>GDAL WMS driver (http://www.gdal.org/frmt_wms.html)</UserAgent> HTTP User-agent string. Some servers might require a well-known user-agent such as "Mozilla/5.0" (optional, defaults to "GDAL WMS driver (http://www.gdal.org/frmt_wms.html)"). When used with some servers, like OpenStreetMap ones, it is highly recommended to put a custom user agent to avoid being blocked if the default user agent had to be blocked.
<Accept>mimetype>/Accept>                                                  HTTP Accept header to specify the MIME type of the expected output of the server. Empty by default
<UserPwd>user:password</UserPwd>                                           User and Password for HTTP authentication (optional).
<UnsafeSSL>true</UnsafeSSL>                                                Skip SSL certificate verification. May be needed if server is using a self signed certificate (optional, defaults to false).
<Referer>http://example.foo/</Referer>                                     HTTP Referer string. Some servers might require it (optional).
<ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes>                           Comma separated list of HTTP response codes that will be interpreted as a 0 filled image (i.e. black for 3 bands, and transparent for 4 bands) instead of aborting the request. (optional, defaults to 204)
<ZeroBlockOnServerException>true</ZeroBlockOnServerException>              Whether to treat a Service Exception returned by the server as a 0 filled image instead of aborting the request. (optional, defaults to false)
</GDAL_WMS>
\
========================================================================== ===============================================================================================================================================================================================================================================================================================================================

Starting with GDAL 2.3, additional HTTP headers can be sent by setting the GDAL_HTTP_HEADER_FILE configuration option to point to a filename of a text file with “key: value” HTTP headers.

Minidrivers
-----------

The GDAL WMS driver has support for several internal 'minidrivers',
which allow access to different web mapping services. Each of these
services may support a different set of options in the Service block.

WMS
~~~

Communications with an OGC WMS server. Has support for both tiled and
untiled requests.

WMS layers can be queried (through a
GetFeatureInfo request) with the gdallocationinfo utility, or with a
GetMetadataItem("Pixel_iCol_iLine", "LocationInfo") call on a band
object.

::

   gdallocationinfo "WMS:http://demo.opengeo.org/geoserver/gwc/service/wms?SERVICE=WMS&VERSION=1.1.1&
                               REQUEST=GetMap&LAYERS=og%3Abugsites&SRS=EPSG:900913&
                               BBOX=-1.15841845090625E7,5479006.186718751,-1.1505912992109375E7,5557277.703671876&
                               FORMAT=image/png&TILESIZE=256&OVERVIEWCOUNT=25&MINRESOLUTION=0.0046653459640220&TILED=true"
                              -geoloc -11547071.455 5528616 -xml -b 1


Output:

::

   Report pixel="248595" line="191985">
     <BandReport band="1">
       <LocationInfo>
         <wfs:FeatureCollection xmlns="http://www.opengis.net/wfs"
                                   xmlns:wfs="http://www.opengis.net/wfs"
                                   xmlns:gml="http://www.opengis.net/gml"
                                   xmlns:og="http://opengeo.org"
                                   xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                                   xsi:schemaLocation="http://opengeo.org http://demo.opengeo.org/geoserver/wfs?service=WFS&version=1.0.0&request=DescribeFeatureType&typeName=og%3Abugsites http://www.opengis.net/wfs http://demo.opengeo.org/geoserver/schemas/wfs/1.0.0/WFS-basic.xsd">
           <gml:boundedBy>
             <gml:Box srsName="http://www.opengis.net/gml/srs/epsg.xml#26713">
               <gml:coordinates xmlns:gml="http://www.opengis.net/gml" decimal="." cs="," ts=" ">601228,4917635 601228,4917635</gml:coordinates>
             </gml:Box>
           </gml:boundedBy>
           <gml:featureMember>
             <og:bugsites fid="bugsites.40946">
               <gml:boundedBy>
                 <gml:Box srsName="http://www.opengis.net/gml/srs/epsg.xml#26713">
                   <gml:coordinates xmlns:gml="http://www.opengis.net/gml" decimal="." cs="," ts=" ">601228,4917635 601228,4917635</gml:coordinates>
                 </gml:Box>
               </gml:boundedBy>
               <og:cat>86</og:cat>
               <og:str1>Beetle site</og:str1>
               <og:the_geom>
                 <gml:Point srsName="http://www.opengis.net/gml/srs/epsg.xml#26713">
                   <gml:coordinates xmlns:gml="http://www.opengis.net/gml" decimal="." cs="," ts=" ">601228,4917635</gml:coordinates>
                 </gml:Point>
               </og:the_geom>
             </og:bugsites>
           </gml:featureMember>
         </wfs:FeatureCollection>
       </LocationInfo>
       <Value>255</Value>
     </BandReport>
   </Report>


TileService
~~~~~~~~~~~

Service to support talking to a WorldWind
`TileService <http://www.worldwindcentral.com/wiki/TileService>`__.
Access is always tile based.

WorldWind
~~~~~~~~~

Access to web-based WorldWind tile services. Access is always tile
based.

TMS
~~~

The TMS Minidriver is designed primarily to support the users of the
`TMS
Specification <http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification>`__.
This service supports only access by tiles.

Because TMS is similar to many other 'x/y/z' flavored services on the
web, this service can also be used to access these services. To use it
in this fashion, you can use replacement variables, of the format ${x},
${y}, etc.

Supported variables (name is case sensitive) are :

-  ${x} -- x position of the tile
-  ${y} -- y position of the tile. This can be either from the top or
   the bottom of the tileset, based on whether the YOrigin parameter is
   set to true or false.
-  ${z} -- z position of the tile -- zoom level
-  ${version} -- version parameter, set in the config file. Defaults to
   1.0.0.
-  ${format} -- format parameter, set in the config file. Defaults to
   'jpg'.
-  ${layer} -- layer parameter, set in the config file. Defaults to
   nothing.

| A typical ServerURL might look like:
| ``http://tilecache.osgeo.org/wms-c/Basic.py/${version}/${layer}/${z}/${x}/${y}.${format}``
| In order to better suit TMS users, any URL that does not contain "${"
  will automatically have the string above (after "Basic.py/") appended
  to their URL.

The TMS Service has 3 XML configuration elements that are different from
other services: ``Format`` which defaults to ``jpg``, ``Layer`` which
has no default, and ``Version`` which defaults to ``1.0.0``.

Additionally, the TMS service respects one additional parameter, at the
DataWindow level, which is the YOrigin element. This element should be
one of ``bottom`` (the default in TMS) or ``top``, which matches
OpenStreetMap and many other popular tile services.

Two examples of usage of the TMS service are included in the examples
below.

OnEarth Tiled WMS
~~~~~~~~~~~~~~~~~

The OnEarth Tiled WMS minidriver supports the Tiled WMS specification
implemented for the JPL OnEarth driver per the specification at
http://web.archive.org/web/20130511182803/http://onearth.jpl.nasa.gov/tiled.html.

Only the ServerUrl and the TiledGroupName are required, most of the required information 
is automatically fetched from the remote server using the GetTileService method at open time.

A typical OnEarth Tiled WMS configuration file might look like:

::

   <GDAL_WMS>
       <Service name="TiledWMS">
       <ServerUrl>https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?</ServerUrl>
       <TiledGroupName>MODIS Terra CorrectedReflectance TrueColor tileset</TiledGroupName>
       <Change key="${time}">2020-02-02</Change>
       </Service>
   </GDAL_WMS>

The TiledWMS minidriver can use the following open options :

-  TiledGroupName -- The value is a string that identifies one of the tiled services 
   available on the server
-  Change -- A <Key>:<Value> pair, which will be passed to the server. The key has to 
   match a change key that the server declares for the respective tiled group.
   This option can be used multiple times, for different keys.
   Example:
   -  Change=time:2020-02-02

These open options are only accepted if the corresponding XML element is not present in the 
configuration file.

VirtualEarth
~~~~~~~~~~~~

Access to web-based Virtual Earth tile services. Access is always tile
based.

The ${quadkey} variable must be found in the ServerUrl element.

The DataWindow element might be omitted. The default values are :

-  UpperLeftX = -20037508.34
-  UpperLeftY = 20037508.34
-  LowerRightX = 20037508.34
-  LowerRightY = -20037508.34
-  TileLevel = 21
-  OverviewCount = 20
-  SRS = EPSG:3857
-  BlockSizeX = 256
-  BlockSizeY = 256

ArcGIS REST API
~~~~~~~~~~~~~~~

Access to ArcGIS REST `map service
resource <http://resources.arcgis.com/en/help/rest/apiref/mapserver.html>`__
(untiled requests).

AGS layers can be
`queried <http://resources.arcgis.com/en/help/rest/apiref/identify.html>`__
(through a GetFeatureInfo request) with the gdallocationinfo utility, or
with a GetMetadataItem("Pixel_iCol_iLine", "LocationInfo") call on a
band object.

::

   gdallocationinfo -wgs84 "<GDAL_WMS><Service name=\"AGS\"><ServerUrl>http://sampleserver1.arcgisonline.com/ArcGIS/rest/services/Specialty/ESRI_StateCityHighway_USA/MapServer</ServerUrl><BBoxOrder>xyXY</BBoxOrder><SRS>3857</SRS></Service><DataWindow><UpperLeftX>-20037508.34</UpperLeftX><UpperLeftY>20037508.34</UpperLeftY><LowerRightX>20037508.34</LowerRightX><LowerRightY>-20037508.34</LowerRightY><SizeX>512</SizeX><SizeY>512</SizeY></DataWindow></GDAL_WMS>" -75.704 39.75


Internet Imaging Protocol (IIP) (GDAL 2.1 and later)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Access to images served through `IIP
protocol <https://en.wikipedia.org/wiki/Internet_Imaging_Protocol>`__.
The server must support the JTL (Retrieve a tile as a complete JFIF
image) extension of the IIP protocol.

If using the XML syntax, the ServerURL must contain the FIF parameter.

Otherwise it is also possible to use "IIP:http://foo.com/FIF=image_name"
syntax as connection string, to retrieve from the server information on
the full resolution dimension and the number of resolutions.

The XML definition can then be generated with "gdal_translate
IIP:http://foo.com/FIF=image_name out.xml -of WMS"

Examples
--------

-  | `onearth_global_mosaic.xml <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_onearth_global_mosaic.xml>`__
     - Landsat mosaic from a `OnEarth <http://onearth.jpl.nasa.gov/>`__
     WMS server

   ::

      gdal_translate -of JPEG -outsize 500 250 onearth_global_mosaic.xml onearth_global_mosaic.jpg

   ::

      gdal_translate -of JPEG -projwin -10 55 30 35 -outsize 500 250 onearth_global_mosaic.xml onearth_global_mosaic2.jpg

   *Note : this particular server does no longer accept regular WMS
   queries.*

-  `metacarta_wmsc.xml <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_metacarta_wmsc.xml>`__ - It is possible
   to configure a WMS Service conforming to a WMS-C cache by specifying
   a number of overviews and specifying the 'block size' as the tile
   size of the cache. The following example is a sample set up for a
   19-level "Global Profile" WMS-C cache.

   ::

      gdal_translate -of PNG -outsize 500 250 metacarta_wmsc.xml metacarta_wmsc.png

   .. only:: html

        .. image:: http://sydney.freeearthfoundation.com/gdalwms/metacarta_wmsc.png

-  | `tileservice_bmng.xml <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_tileservice_bmng.xml>`__ -
     TileService, Blue Marble NG (January)

   ::

      gdal_translate -of JPEG -outsize 500 250 tileservice_bmng.xml tileservice_bmng.jpg

   .. only:: html

        .. image:: http://sydney.freeearthfoundation.com/gdalwms/tileservice_bmng.jpg

-  | `tileservice_nysdop2004.xml <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_tileservice_nysdop2004.xml>`__
     - TileService, NYSDOP 2004

   ::

      gdal_translate -of JPEG -projwin -73.687030 41.262680 -73.686359 41.262345 -outsize 500 250 tileservice_nysdop2004.xml tileservice_nysdop2004.jpg

   .. only:: html

        .. image:: http://sydney.freeearthfoundation.com/gdalwms/tileservice_nysdop2004.jpg

-  | `OpenStreetMap TMS Service
     Example <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_openstreetmap_tms.xml>`__: Connect to
     OpenStreetMap tile service. Note that this file takes advantage of
     the tile cache; more information about configuring the tile cache
     settings is available above. Please also change the <UserAgent>, to avoid the
     default one being used, and potentially blocked by OSM servers in case a too
     big usage of it would be seen.
   | ``gdal_translate -of PNG -outsize 512 512 frmt_wms_openstreetmap_tms.xml openstreetmap.png``

-  | `MetaCarta TMS Layer Example <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_metacarta_tms.xml>`__,
     accessing the default MetaCarta TMS layer.
   | ``gdal_translate -of PNG -outsize 512 256 frmt_wms_metacarta_tms.xml metacarta.png``

-  `BlueMarble Amazon S3 Example <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_bluemarble_s3_tms.xml>`__
   accessed with the TMS minidriver.

-  `Google Maps <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_googlemaps_tms.xml>`__ accessed with the TMS
   minidriver.

-  `ArcGIS MapServer Tiles <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_arcgis_mapserver_tms.xml>`__
   accessed with the TMS minidriver.

-  OnEarth Tiled WMS `Clementine <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_twms_Clementine.xml>`__,
   `daily <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_twms_daily.xml>`__, and `srtm <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_twms_srtm.xml>`__
   examples.

-  `VirtualEarth Aerial Layer <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_virtualearth.xml>`__ accessed
   with the VirtualEarth minidriver.

-  `ArcGIS online sample server layer <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_ags_arcgisonline.xml>`__
   accessed with the ArcGIS Server REST API minidriver.

-  `IIP online sample server layer <https://github.com/OSGeo/gdal/blob/master/gdal/frmts/wms/frmt_wms_iip.xml>`__ accessed with
   the IIP minidriver.

Open syntax
-----------

The WMS driver can open :

-  a local service description XML file :

   ::

      gdalinfo description_file.xml

-  the content of a description XML file provided as filename :

   ::

      gdalinfo "<GDAL_WMS><Service name=\"TiledWMS\"><ServerUrl>https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?</ServerUrl><TiledGroupName>MODIS Terra CorrectedReflectance Bands367 tileset</TiledGroupName></Service></GDAL_WMS>"

-  the base URL of a WMS service, prefixed with *WMS:* :

   ::

      gdalinfo "WMS:http://wms.geobase.ca/wms-bin/cubeserv.cgi"

   A list of subdatasets will be returned, resulting from the parsing of
   the GetCapabilities request on that server.

-  a pseudo GetMap request, such as the subdataset name
   returned by the previous syntax :

   ::

      gdalinfo "WMS:http://wms.geobase.ca/wms-bin/cubeserv.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&LAYERS=DNEC_250K%3AELEVATION%2FELEVATION&SRS=EPSG:42304&BBOX=-3000000,-1500000,6000000,4500000"

-  the base URL of a Tiled WMS service, prefixed with
   *WMS:* and with request=GetTileService as GET argument:

   ::

      gdalinfo "WMS:https://gibs.earthdata.nasa.gov/twms/epsg4326/best/twms.cgi?request=GetTileService"

   A list of subdatasets will be returned, resulting from the parsing of
   the GetTileService request on that server.

-  the URL of a REST definition for a ArcGIS MapServer:

   ::

      gdalinfo "http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer?f=json&pretty=true"

-  (GDAL >= 2.1.0) the URL of a IIP image:

   ::

      gdalinfo "IIP:http://merovingio.c2rmf.cnrs.fr/fcgi-bin/iipsrv.fcgi?FIF=globe.256x256.tif"

Generation of WMS service description XML file
----------------------------------------------

The WMS service description XML file can be generated manually, or
created as the output of the CreateCopy() operation of the WMS driver,
only if the source dataset is itself a WMS dataset. Said otherwise, you
can use gdal_translate with as source dataset any of the above syntax
mentioned in "Open syntax" and as output an XML file. For example:

::

   gdal_translate "http://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer?f=json" wms.xml -of WMS

The generated file will come with default values that you may need to
edit.

See Also
--------

-  `OGC WMS Standards <http://www.opengeospatial.org/standards/wms>`__
-  `WMS Tiling Client Recommendation
   (WMS-C) <http://wiki.osgeo.org/index.php/WMS_Tiling_Client_Recommendation>`__
-  `WorldWind
   TileService <http://www.worldwindcentral.com/wiki/TileService>`__
-  `TMS
   Specification <http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification>`__
-  `OnEarth Tiled WMS
   specification <http://web.archive.org/web/20130511182803/http://onearth.jpl.nasa.gov/tiled.html>`__
-  `ArcGIS Server REST
   API <http://resources.arcgis.com/en/help/rest/apiref/>`__
-  :ref:`raster.wmts` driver page.
