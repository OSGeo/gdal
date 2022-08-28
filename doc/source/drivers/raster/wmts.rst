.. _raster.wmts:

================================================================================
WMTS -- OGC Web Map Tile Service
================================================================================

.. shortname:: WMTS

.. versionadded:: 2.1

.. build_dependencies:: libcurl

Access to WMTS layers is possible with the GDAL WMTS
client driver (needs Curl support). It support both RESTful and KVP
protocols.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Open syntax
-----------

The WMTS driver can open :

-  a local service description XML file, whose syntax is described in
   the below section :

   ::

      gdalinfo gdal_wmts.xml

-  the content of a description XML file provided as filename :

   ::

      gdalinfo "<GDAL_WMTS><GetCapabilitiesUrl>http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml</GetCapabilitiesUrl><Layer>lb</Layer></GDAL_WMTS>"

-  a local GetCapabilities response of a WMTS service :

   ::

      gdalinfo WMTSCapabilities.xml

-  the URL to the GetCapabilities response of a WMTS service:

   ::

      gdalinfo "http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml"

-  the URL to the GetCapabilities response of a WMTS service, prefixed
   with *WMTS:*, and possibly with optional layer, tilematrixset,
   tilematrix/zoom_level, style and extendbeyonddateline parameters,
   with the following syntax
   *WMTS:url[,layer=layer_id][,tilematrixset=tms_id][,tilematrix=tm_id|,zoom_level=level][,style=style_id][,extendbeyonddateline=yes/no]*.

   ::

      gdalinfo "WMTS:http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml"

   ::

      gdalinfo "WMTS:http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml,layer=lb"

-  the *WMTS:* prefix with open options

   ::

      gdalinfo WMTS: -oo URL=http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml -oo LAYER=lb

In any of the above syntaxes, if several layers are present and no layer
disambiguation was done with the layer parameter/open option, or if a
layer has more than one style or a tile matrix set, a list of
subdatasets will be returned. If there is only one layer, it will be
opened on the default style and the first tile matrix set listed.

Open options
------------

The following open options are available:

- **URL**: URL (or filename for local files) to GetCapabilities response document.
  Required if not specified in the connection string (e.g if using "WMTS:" only)

- **LAYER**: Layer identifier

- **TILEMATRIXSET**: Tile Matrix Set identifier, which determines the CRS into
  which the layer will be exposed. Must be one of the listed tile matrix
  for the layer.

- **TILEMATRIX**: Tile Matrix identifier. Must be one of the listed tile matrix of
  the select tile matrix set for the layer. Mutually exclusive with ZOOM_LEVEL.
  If not specified the last tile matrix, i.e. the one with the best resolution,
  is selected.

- **ZOOM_LEVEL**: Index of the maximum zoom level tile matrix to use for the
  full resolution GDAL dataset (lower zoom levels will be used for overviews).
  The first one (ie the one of lower resolution) is indexed 0.
  Mutually exclusive with TILEMATRIX.
  If not specified the last tile matrix, i.e. the one with the best resolution,
  is selected.

- **STYLE**: Style identifier. Must be one of layer.

- **EXTENDBEYONDDATELINE** = YES/NO.  Whether to make the extent go over dateline
  and warp tile requests. See ExtendBeyondDateLine parameter of the local service
  description XML file described below for more details.

- **EXTENT_METHOD** = AUTO/LAYER_BBOX/TILE_MATRIX_SET/MOST_PRECISE_TILE_MATRIX.
  GDAL needs to retrieve an extent for the layer. Different sources are possible.
  WGS84BoundingBox element at the Layer level, BoundingBox elements with potentially
  several CRS at the Layer level, BoundingBox of the TileMatrixSet definitions
  shared by all layers, and TileMatrixLimit definitions at the Layer level.
  By default (AUTO), GDAL will try first with a WGS84BoundingBox/BoundingBox corresponding
  to the CRS implied by the select TileMatrixSet. If not available, if will
  fallback to a BoundingBox in another CRS and reproject it to the selected CRS.
  If not available, it will fallback to the most precise tile matrix of the
  selected TileMatrixSet and will clip it with the bounding box implied by the
  most precise zoom level of the TileMatrixLimit of the layer.
  If LAYER_BBOX is specified, only WGS84BoundingBox/BoundingBox elements are
  considered.
  If TILE_MATRIX_SET is specified, the BoundingBox element of the selected
  TileMatrixSet will be used.
  If MOST_PRECISE_TILE_MATRIX is specified, the implit extent of the
  most precise tile matrix will be used.

- **CLIP_EXTENT_WITH_MOST_PRECISE_TILE_MATRIX** = YES/NO (GDAL >= 3.4.2).
  Whether to use the implied bounds of the most precise TileMatrix to clip the
  layer extent (defaults to NO if the layer bounding box is used, YES otherwise)

- **CLIP_EXTENT_WITH_MOST_PRECISE_TILE_MATRIX_LIMITS** = YES/NO (GDAL >= 3.4.2).
  Whether to use the implied bounds of the most precise TileMatrixLimit to clip the
  layer extent (defaults to NO if the layer bounding box is used, YES otherwise)


Local service description XML file
----------------------------------

It is important that there be no spaces or other content before the
``<GDAL_WMTS>`` element.

+-----------------------------------+-----------------------------------+
| <GDAL_WMTS>                       |                                   |
+-----------------------------------+-----------------------------------+
| <GetCapabilitiesUrl>http://foo/WM | URL (or filename for local files) |
| TSCapabilities.xml</GetCapabiliti | to GetCapabilities response       |
| esUrl>                            | document (required). For a KVP    |
|                                   | only server, will be like         |
|                                   | http://end_point?SERVICE=WMTS&amp |
|                                   | ;REQUEST=GetCapabilities          |
|                                   | .                                 |
+-----------------------------------+-----------------------------------+
| <ExtraQueryParameters>foo=bar&amp;| URL query parameters to add to    |
|                                   | all requests (GetCapabilities,    |
|                                   | GetTile, GetFeatureInfo)          |
|                                   | (added in GDAL 3.5.1)             |
+-----------------------------------+-----------------------------------+
| <Layer>layer_id</Layer>           | Layer identifier (optional, but   |
|                                   | may be needed to disambiguate     |
|                                   | between several layers)           |
+-----------------------------------+-----------------------------------+
| <Style>style_id</Style>           | Style identifier. Must be one of  |
|                                   | the listed styles for the layer.  |
|                                   | (optional, but may be needed to   |
|                                   | disambiguate between several      |
|                                   | styles)                           |
+-----------------------------------+-----------------------------------+
| <TileMatrixSet>tile_matrix_set_id | Tile Matrix Set identifier. Must  |
| </TileMatrixSet>                  | be one of the listed tile matrix  |
|                                   | set for the layer. (optional, but |
|                                   | may be needed to disambiguate     |
|                                   | between several tile matrix sets) |
+-----------------------------------+-----------------------------------+
| <TileMatrix>tile_matrix_id</TileM | Tile Matrix identifier. Must be   |
| atrix>                            | one of the listed tile matrix of  |
|                                   | the select tile matrix set for    |
|                                   | the layer. (optional, GDAL >=     |
|                                   | 2.2. Exclusive with ZoomLevel. If |
|                                   | not specified the last tile       |
|                                   | matrix, ie the one with the best  |
|                                   | resolution, is selected)          |
+-----------------------------------+-----------------------------------+
| <ZoomLevel>int_value</ZoomLevel>  | Index of the maximum zoom level / |
|                                   | tile matrix to use. The first one |
|                                   | (ie the one of lower resolution)  |
|                                   | is indexed 0. (optional, GDAL >=  |
|                                   | 2.2. Exclusive with TileMatrix.   |
|                                   | If not specified the last tile    |
|                                   | matrix, ie the one with the best  |
|                                   | resolution, is selected)          |
+-----------------------------------+-----------------------------------+
| <Format>image/png</Format>        | Tile format, used by GetTile      |
|                                   | requests. Must be one of the      |
|                                   | listed Format for the layer.      |
|                                   | (optional, but may be needed to   |
|                                   | disambiguate between several      |
|                                   | Format)                           |
+-----------------------------------+-----------------------------------+
| <InfoFormat>application/xml</Info | Info format, used by              |
| Format>                           | GetFeatureInfo requests. Must be  |
|                                   | one of the listed InfoFormat for  |
|                                   | the layer. (optional, but may be  |
|                                   | needed to disambiguate between    |
|                                   | several InfoFormat)               |
+-----------------------------------+-----------------------------------+
| <DataWindow>                      | Define extents of the data.       |
|                                   | (optional, when not specified the |
|                                   | driver will query the declared    |
|                                   | extent of the layer, and if not   |
|                                   | present fallback to the extent of |
|                                   | the select tile matrix set,       |
|                                   | taking into account potential     |
|                                   | tile matrix set limits)           |
+-----------------------------------+-----------------------------------+
| <UpperLeftX>-180.0</UpperLeftX>   | X (longitude/easting) coordinate  |
|                                   | of upper-left corner, in the SRS  |
|                                   | of the tile matrix set. (required |
|                                   | if DataWindow is present)         |
+-----------------------------------+-----------------------------------+
| <UpperLeftY>90.0</UpperLeftY>     | Y (latitude/northing) coordinate  |
|                                   | of upper-left corner, in the SRS  |
|                                   | of the tile matrix set. (required |
|                                   | if DataWindow is present)         |
+-----------------------------------+-----------------------------------+
| <LowerRightX>180.0</LowerRightX>  | X (longitude/easting) coordinate  |
|                                   | of lower-right corner, in the SRS |
|                                   | of the tile matrix set. (required |
|                                   | if DataWindow is present)         |
+-----------------------------------+-----------------------------------+
| <LowerRightY>-90.0</LowerRightY>  | Y (latitude/northing) coordinate  |
|                                   | of lower-right corner, in the SRS |
|                                   | of the tile matrix set. (required |
|                                   | if DataWindow is present)         |
+-----------------------------------+-----------------------------------+
| </DataWindow>                     |                                   |
+-----------------------------------+-----------------------------------+
| <Projection>EPSG:4326</Projection | Declared projection, in case the  |
| >                                 | one of the TileMatrixSet is not   |
|                                   | desirable (optional, defaults to  |
|                                   | value of the TileMatrixSet)       |
+-----------------------------------+-----------------------------------+
| <BandsCount>4</BandsCount>        | Number of bands/channels, 1 for   |
|                                   | grayscale data, 3 for RGB, 4 for  |
|                                   | RGBA. (optional, defaults to 4)   |
+-----------------------------------+-----------------------------------+
| <DataType>Byte</DataType>         | Band data type, one of: Byte,     |
|                                   | Int16, UInt16, Int32, UInt32,     |
|                                   | Float32, Float64, etc..           |
|                                   | (optional, defaults to Byte)      |
+-----------------------------------+-----------------------------------+
| <ExtendBeyondDateLine>false</Exte | Whether to make the extent go     |
| ndBeyondDateLine>                 | over dateline and warp tile       |
|                                   | requests. Only appropriate when   |
|                                   | the 2 following conditions are    |
|                                   | met (optional, defaults to        |
|                                   | false):                           |
|                                   |                                   |
|                                   | -  for a geodetic SRS or          |
|                                   |    EPSG:3857, with tile matrix    |
|                                   |    sets such as the whole         |
|                                   |    [-180,180] range of longitude  |
|                                   |    is entirely covered by an      |
|                                   |    integral number of tiles (e.g. |
|                                   |    GoogleMapsCompatible).         |
|                                   | -  AND                            |
|                                   |                                   |
|                                   |    -  when the layer BoundingBox  |
|                                   |       in the SRS of the tile      |
|                                   |       matrix set covers the whole |
|                                   |       [-180,180] range of         |
|                                   |       longitude, and that there   |
|                                   |       is another BoundingBox in   |
|                                   |       another SRS that is         |
|                                   |       centered around longitude   |
|                                   |       180. If such alternate      |
|                                   |       BoundingBox is not present  |
|                                   |       in the GetCapabilities      |
|                                   |       document, DataWindow must   |
|                                   |       be explicitly specified     |
|                                   |    -  OR when the layer           |
|                                   |       BoundingBox in the SRS of   |
|                                   |       the tile matrix set extends |
|                                   |       beyond the dateline.        |
+-----------------------------------+-----------------------------------+
| <Cache>                           | Enable local disk cache. Allows   |
|                                   | for offline operation. (optional, |
|                                   | cache is disabled when absent,    |
|                                   | but it is present in the          |
|                                   | autogenerated XML, can be         |
|                                   | overridden with                   |
|                                   | GDAL_ENABLE_WMS_CACHE=NO          |
+-----------------------------------+-----------------------------------+
| <Path>./gdalwmscache</Path>       | Location where to store cache     |
|                                   | files. It is safe to use same     |
|                                   | cache path for different data     |
|                                   | sources. (optional, defaults to   |
|                                   | ./gdalwmscache if                 |
|                                   | GDAL_DEFAULT_WMS_CACHE_PATH       |
|                                   | configuration option is not       |
|                                   | specified)                        |
|                                   | /vsimem/ paths are supported      |
|                                   | allowing for temporary in-memory  |
|                                   | cache                             |
+-----------------------------------+-----------------------------------+
| <Type>file</Type>                 | Cache type. Now supported only    |
|                                   | 'file' type. In 'file'            |
|                                   | cache type files are stored in    |
|                                   | file system folders. (optional,   |
|                                   | defaults to 'file')               |
+-----------------------------------+-----------------------------------+
| <Depth>2</Depth>                  | Number of directory layers. 2     |
|                                   | will result in files being        |
|                                   | written as                        |
|                                   | cache_path/A/B/ABCDEF...          |
|                                   | (optional, defaults to 2)         |
+-----------------------------------+-----------------------------------+
| <Extension>.jpg</Extension>       | Append to cache files. (optional, |
|                                   | defaults to none)                 |
+-----------------------------------+-----------------------------------+
| </Cache>                          |                                   |
+-----------------------------------+-----------------------------------+
| <MaxConnections>2</MaxConnections | Maximum number of simultaneous    |
| >                                 | connections. (optional, defaults  |
|                                   | to 2)                             |
+-----------------------------------+-----------------------------------+
| <Timeout>300</Timeout>            | Connection timeout in seconds.    |
|                                   | (optional, defaults to 300)       |
+-----------------------------------+-----------------------------------+
| <OfflineMode>true</OfflineMode>   | Do not download any new images,   |
|                                   | use only what is in cache. Useful |
|                                   | only with cache enabled.          |
|                                   | (optional, defaults to false)     |
+-----------------------------------+-----------------------------------+
| <UserAgent>GDAL WMS driver        | HTTP User-agent string. Some      |
| (http://www.gdal.org/frmt_wms.htm | servers might require a           |
| l)</UserAgent>                    | well-known user-agent such as     |
|                                   | "Mozilla/5.0" (optional, defaults |
|                                   | to "GDAL WMS driver               |
|                                   | (http://www.gdal.org/frmt_wms.htm |
|                                   | l)").                             |
+-----------------------------------+-----------------------------------+
| <Accept>mimetype>/Accept>         | HTTP Accept header to specify the |
|                                   | MIME type of the expected output  |
|                                   | of the server. Empty by default.  |
|                                   | (added in GDAL 3.5.1)             |
+-----------------------------------+-----------------------------------+
| <UserPwd>user:password</UserPwd>  | User and Password for HTTP        |
|                                   | authentication (optional).        |
+-----------------------------------+-----------------------------------+
| <UnsafeSSL>true</UnsafeSSL>       | Skip SSL certificate              |
|                                   | verification. May be needed if    |
|                                   | server is using a self signed     |
|                                   | certificate (optional, defaults   |
|                                   | to false, but set to true in      |
|                                   | autogenerated XML).               |
+-----------------------------------+-----------------------------------+
| <Referer>http://example.foo/</Ref | HTTP Referer string. Some servers |
| erer>                             | might require it (optional).      |
+-----------------------------------+-----------------------------------+
| <ZeroBlockHttpCodes>204,404</Zero | Comma separated list of HTTP      |
| BlockHttpCodes>                   | response codes that will be       |
|                                   | interpreted as a 0 filled image   |
|                                   | (i.e. black for 3 bands, and      |
|                                   | transparent for 4 bands) instead  |
|                                   | of aborting the request.          |
|                                   | (optional, defaults to non set,   |
|                                   | but set to 204,404 in             |
|                                   | autogenerated XML)                |
+-----------------------------------+-----------------------------------+
| <ZeroBlockOnServerException>true< | Whether to treat a Service        |
| /ZeroBlockOnServerException>      | Exception returned by the server  |
|                                   | as a 0 filled image instead of    |
|                                   | aborting the request. (optional,  |
|                                   | defaults to false, but set to     |
|                                   | true in autogenerated XML)        |
+-----------------------------------+-----------------------------------+
| </GDAL_WMTS>                      |                                   |
+-----------------------------------+-----------------------------------+
|                                   |                                   |
+-----------------------------------+-----------------------------------+

Starting with GDAL 2.3, additional HTTP headers can be sent by setting the
GDAL_HTTP_HEADER_FILE configuration option to point to a filename of a text
file with “key: value” HTTP headers.

GetFeatureInfo request
----------------------

WMTS layers can be queried (through a GetFeatureInfo request) with the
gdallocationinfo utility, or with a GetMetadataItem("Pixel_iCol_iLine",
"LocationInfo") call on a band object.

::

   gdallocationinfo my_wmts.xml -geoloc -11547071.455 5528616 -xml -b 1

Generation of WMTS service description XML file
-----------------------------------------------

The WMTS service description XML file can be generated manually, or
created as the output of the CreateCopy() operation of the WMTS driver,
only if the source dataset is itself a WMTS dataset. Said otherwise, you
can use gdal_translate with as source dataset any of the above syntax
mentioned in "Open syntax" and as output an XML file. For example:

::

   gdal_translate "WMTS:http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml,layer=lb" wmts.xml -of WMTS

generates the following file:

.. code-block:: xml

   <GDAL_WMTS>
     <GetCapabilitiesUrl>http://maps.wien.gv.at/wmts/1.0.0/WMTSCapabilities.xml</GetCapabilitiesUrl>
     <Layer>lb</Layer>
     <Style>farbe</Style>
     <TileMatrixSet>google3857</TileMatrixSet>
     <DataWindow>
       <UpperLeftX>1800035.8827671</UpperLeftX>
       <UpperLeftY>6161931.622311067</UpperLeftY>
       <LowerRightX>1845677.148953537</LowerRightX>
       <LowerRightY>6123507.385072636</LowerRightY>
     </DataWindow>
     <BandsCount>4</BandsCount>
     <Cache />
     <UnsafeSSL>true</UnsafeSSL>
     <ZeroBlockHttpCodes>404</ZeroBlockHttpCodes>
     <ZeroBlockOnServerException>true</ZeroBlockOnServerException>
   </GDAL_WMTS>

The generated file will come with default values that you may need to
edit.

See Also
--------

-  `OGC WMTS Standard <http://www.opengeospatial.org/standards/wmts>`__
-  :ref:`raster.wms` driver page.
