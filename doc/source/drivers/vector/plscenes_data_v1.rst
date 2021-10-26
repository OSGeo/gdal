.. _vector.plscenes_data_v1:

PLScenes (Planet Labs Scenes), Data V1 API
==========================================

.. versionadded:: 2.2

The driver supports read-only operations to list scenes and their
metadata as a vector layer per item-types: "PSOrthoTile", "REOrthoTile",
"PSScene3Band", "PSScene4Band", "REScene", "Landsat8L1G",
"Sentinel2L1C". It can also access raster scenes.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset name syntax
-------------------

The minimal syntax to open a datasource is :

::

   PLScenes:[options]

Additional optional parameters can be specified after the ':' sign.
Currently the following one is supported :

-  **version**\ =data_v1: To specify the API version to request.
-  **api_key**\ =value: To specify the Planet API KEY. It is mandatory,
   unless it is supplied through the open option API_KEY, or the
   configuration option PL_API_KEY.
-  **follow_links**\ =YES/NO: Whether assets links should be followed
   for each scene (vector). Getting assets links require a HTTP request
   per scene, which might be costly when enumerating through a lot of
   products. Defaults to NO.
-  **scene**\ =scene_id: To specify the scene ID, when accessing raster
   data. Optional for vector layer access.
-  **itemtypes**\ =name: To specify the item types name. Optional for
   vector layer access. Mandatory for raster accessI.
-  **asset**\ =value: To specify the asset type (for raster fetching).
   Default is "visual". Optional for vector layer access. If the option
   is not specified and the 'visual' asset category does not exist for
   the scene (or if the value is set to 'list'), the returned dataset
   will have subdatasets for the available asset categories.
-  **medata**\ =YES/NO: (Raster only) Whether scene metadata should be
   fetch from the API and attached to the raster dataset. Defaults to
   YES.

If several parameters are specified, they must be separated by a comma.

Open options
------------

The following open options are available :

-  **VERSION**\ =data_v1: To specify the API version to request.
-  **API_KEY**\ =value: To specify the Planet API KEY.
-  **FOLLOW_LINKS**\ =YES/NO: Whether assets links should be followed
   for each scene (vector). Getting assets links require a HTTP request
   per scene, which might be costly when enumerating through a lot of
   products. Defaults to NO.
-  **SCENE**\ =scene_id: To specify the scene ID, when accessing raster
   data. Optional for vector layer access.
-  **ITEMTYPES**\ =name: To specify the item types name. Optional for
   vector layer access. Mandatory for raster access.
-  **ASSET**\ =value: To specify the asset type (for raster fetching).
   Default is "visual". Optional for vector layer access. If the option
   is not specified and the 'visual' asset category does not exist for
   the scene (or if the value is set to 'list'), the returned dataset
   will have subdatasets for the available asset categories.
-  **RANDOM_ACCESS**\ =YES/NO: Whether raster should be accessed in
   random access mode (but with potentially not optimal throughput). If
   NO, in-memory ingestion is done. Default is YES.
-  **ACTIVATION_TIMEOUT**\ =int: Number of seconds during which to wait
   for asset activation (raster). Default is 3600.
-  **METADATA**\ =YES/NO: (Raster only) Whether scene metadata should be
   fetched from the API and attached to the raster dataset. Defaults to
   YES.

Configuration options
---------------------

The following configuration options are available :

-  **PL_API_KEY**\ =value: To specify the Planet API KEY.

Attributes
----------

The layer field definition is built from the "plscensconf.json" file in
the GDAL configuration. The links to downloadable products are in
*asset_XXXXX_location* attributes where XXXXX is the asset category id,
when they are active. Otherwise they should be activated by sending a
POST request to the URL in the *asset_XXXXX_activate_link* attribute
(what the raster driver does automatically)

Geometry
~~~~~~~~

The footprint of each scene is reported as a MultiPolygon with a
longitude/latitude WGS84 coordinate system (EPSG:4326).

Filtering
~~~~~~~~~

The driver will forward any spatial filter set with SetSpatialFilter()
to the server. It also makes the same for simple attribute filters set
with SetAttributeFilter(). Note that not all attributes support all
comparison operators. Refer to comparator column in `Metadata
properties <https://www.planet.com/docs/v0/scenes/#metadata>`__

Paging
~~~~~~

Features are retrieved from the server by chunks of 250 by default (and
this is the maximum value accepted by the server). This number can be
altered with the PLSCENES_PAGE_SIZE configuration option.

Vector layer (scene metadata) examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Listing all scenes available (with the rights of the account) :

::

   ogrinfo -ro -al "PLScenes:" -oo API_KEY=some_value

or

::

   ogrinfo -ro -al "PLScenes:api_key=some_value"

or

::

   ogrinfo -ro -al "PLScenes:" --config PL_API_KEY some_value

Listing all scenes available on PSOrthoTile item types, under a point of
(lat,lon)=(40,-100) :

::

   ogrinfo -ro -al "PLScenes:" -oo API_KEY=some_value PSOrthoTile -spat -100 40 -100 40

Listing all scenes available within a bounding box (lat,lon)=(40,-100)
to (lat,lon)=(39,-99)

::

   ogrinfo -ro -al "PLScenes:" -oo API_KEY=some_value -spat -100 40 -99 39

Listing all scenes available matching criteria :

::

   ogrinfo -ro -al "PLScenes:" -oo API_KEY=some_value PSOrthoTile -where "acquired >= '2015/03/26 00:00:00' AND cloud_cover < 10"

List all downloadable scenes:

::

   ogrinfo -ro -al -q "PLScenes:" -oo API_KEY=some_value PSOrthoTile -where "permissions='assets:download'"

Raster access
-------------

Scenes can be accessed as raster datasets, provided that the scene ID is
specified with the 'scene' parameter / SCENE open option. The
'itemtypes' parameter / ITEMTYPES open option must also be specified.
The asset type (visual, analytic, ...) can be specified with the 'asset'
parameter / ASSET open option. The scene id is the content of the value
of the 'id' field of the features.

If the product is not already generated on the server, it will be
activated, and the driver will wait for it to be available. The length
of this retry can be configured with the ACTIVATION_TIMEOUT open option.

Raster access examples
~~~~~~~~~~~~~~~~~~~~~~

Displaying raster metadata :

::

   gdalinfo "PLScenes:scene=scene_id,itemtypes=itemypes,asset=analytic" -oo API_KEY=some_value

or

::

   gdalinfo "PLScenes:" -oo API_KEY=some_value -oo ITEMTYPES=itemtypes -oo SCENE=scene_id -oo ASSET=analytic

Converting/downloading a whole file:

::

   gdal_translate "PLScenes:" -oo API_KEY=some_value -oo SCENE=scene_id \
                   -oo ITEMTYPES=itemtypes -oo ASSET=analytic -oo RANDOM_ACCESS=NO out.tif

See Also
--------

-  :ref:`General documentation page for PLScenes
   driver <vector.plscenes>`
-  `Documentation of Planet Scenes Data API
   v1 <https://developers.planet.com/docs/apis/data/>`__
-  :ref:`Raster PLMosaic / Planet Mosaics API driver <raster.plmosaic>`
