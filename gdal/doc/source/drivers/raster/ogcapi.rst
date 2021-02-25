.. _raster.ogcapi:

================================================================================
OGCAPI -- OGC API Tiles / Maps
================================================================================

.. versionadded:: 3.2

.. shortname:: OGCAPI

.. build_dependencies:: libcurl

Access to server implementing OGC API - Tiles, OGC API - Maps or OGC API - Coverages.
This driver has raster and vector capabilities.

.. warning::

    This driver is experimental, and has been developed to demonstrate work
    related to the "Modular OGC API Workflows" initiative.
    It implements non-finalized versions of OGC API - Tiles, - Maps and - Coverages.
    Its interface may change at any time, or it might be removed.
    It might also be eventually merged with the OGC API - Features driver.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset opening
---------------

The driver supports opening by:

- passing a filename (with .moaw extension) containing a JSON document, like
  the following, specifying a deferred processing

  .. code-block:: json

        {
        "id" : "SampleRenderMapExecution",
        "process" : "https://maps.ecere.com/ogcapi/processes/RenderMap",
        "inputs" : [
            { "id" : "transparent", "value" : false },
            { "id" : "background", "value" : "navy" },
            {
                "id" : "layers",
                "list" : [
                    { "collection" : "https://maps.ecere.com/ogcapi/collections/NaturalEarth:physical:bathymetry" },
                    { "collection" : "https://maps.ecere.com/ogcapi/collections/SRTM_ViewFinderPanorama" }
                ]
            }
        ]
        }

- passing a string "OGCAPI:{url}" where {url} is the URL to a OGC API landing page
  In that case the driver will return subdatasets with the different collections.

- passing a string "OGCAPI:{url}" where {url} is the URL to a OGC API collection description


When the driver opens a collection, for raster, it will look if tiles or maps
API are advertized for it. It will use tiles API by default, and fallback to maps
API when not available. It will also look at the image formats, and will prefer
PNG When available.

For vector collections, this driver handles the tiles API, with GeoJSON or
Mapbox Vector tiles.

When using the tiles API, the driver will use by default the WorldCRS84Quad tile
matrix set when available

Open options
------------

The following open options are available:

- **API=AUTO/MAPS/TILES/COVERAGE/ITEMS**: Which API to use for data acquisition. Default is
  AUTO. In AUTO mode, for raster access, coverage is used if available, and
  fallback first to tiles and finally maps otherwise.
  In AUTO mode, for vector access, tiles is used if available, and fallback to
  GeoJSON items otherwise.

- **IMAGE_FORMAT=AUTO/PNG/PNG_PREFERRED/JPEG/JPEG_PREFERRED**:
  Which format to use for pixel acquisition, for tiles or map API.
  Defaults to AUTO, which means
  that PNG will be used if available, and fallback to JPEG otherwise.
  If specifying PNG or JPEG, they must be available, otherwise the driver will
  return an error. If specifying the one of the PNG_PREFERRED or JPEG_PREFERRED
  value, the specified format will be used if available, and the driver will
  fallback to the other format otherwise.

- **VECTOR_FORMAT=AUTO/GEOJSON/GEOJSON_PREFERRED/MVT/MVT_PREFERRED**:
  Which format to use for vector data acquisition. Defaults to AUTO, which means
  that MVT (Mapbox Vector Tiles) will be used if available, and fallback to GEOJSON otherwise.
  If specifying MVT or GEOJSON, they must be available, otherwise the driver will
  return an error. If specifying the one of the MVT_PREFERRED or GEOJSON_PREFERRED
  value, the specified format will be used if available, and the driver will
  fallback to the other format otherwise.

- **TILEMATRIXSET=id**:
  Identifier of the required tile matrix set. Only used with the tiles API.
  If this tile matrix set is not available, the driver will fail.
  If this option is not specified, the driver will automatically select one of
  the available tile matrix sets.
  TILEMATRIXSET and PREFERRED_TILEMATRIXSET options are mutually exclusive.

- **PREFERRED_TILEMATRIXSET=id**:
  Identifier of the preferred tile matrix set. Only used with the tiles API.
  If this tile matrix set is not available, or if this option is not specified,
  the driver will automatically select one of the available tile matrix sets.
  TILEMATRIXSET and PREFERRED_TILEMATRIXSET options are mutually exclusive.

- **TILEMATRIX=id**: Identifier of a particular tile matrix (zoom level) of
  the select tile matrix set. If not specified, all available tile matrix are
  returned as overviews (for raster data), or layers (for vector data)
  
- **CACHE=YES/NO**: Whether to enable block/tile caching. Only for tiles API,
  and with raster data. Defaults to YES.
  
- **MAX_CONNECTIONS=number**: Maximum number of connections for parallel tile
  downloading. Only for tiles API, and with raster data. Defaults to 5.
  
- **MINX/MINY/MAXX/MAXY=number**: Bounds in SRS of TileMatrixSet to which to
  restrict the exposed dataset/layers.
