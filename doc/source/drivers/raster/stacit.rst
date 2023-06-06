.. _raster.stacit:

================================================================================
STACIT - Spatio-Temporal Asset Catalog Items
================================================================================

.. versionadded:: 3.4

.. shortname:: STACIT

.. built_in_by_default::

This driver supports opening STAC API ItemCollections, with the input usually being a `STAC API search query <https://github.com/radiantearth/stac-api-spec/tree/main/item-search>`_ or the results saved as a JSON file. Items in the response must include projection information following the `Projection Extension Specification <https://github.com/stac-extensions/projection/>`_.
It builds a virtual mosaic from the items.

A STACIT dataset which has no subdatasets is actually a :ref:`raster.vrt` dataset.
Thus, translating it into VRT will result in a VRT file that directly references the items.

Note that `STAC API ItemCollections <https://github.com/radiantearth/stac-api-spec/blob/main/fragments/itemcollection/README.md>`_ are not the same as  `STAC Collections <https://github.com/radiantearth/stac-spec/tree/master/collection-spec>`_. STAC API ItemCollections are GeoJSON FeatureCollections enhanced with STAC entities.

Open syntax
-----------

STACIT datasets/subdatasets can be accessed with one of the following syntaxes:

* ``filename.json``: local file

* ``STACIT:"https://example.com/filename.json"``: remote file or query

* ``STACIT:"filename.json":asset=my_asset``: specify the name of the asset GDAL should read (i.e. "visual")

* ``STACIT:"filename.json":collection=my_collect,asset=my_asset``: limit to items in a given collection and specify asset to read

* ``STACIT:"filename.json":collection=my_collect,asset=my_asset,crs=my_crs``: specify a collection, asset, and limit to items in a given CRS

Open options
------------

The following open options are supported:

-  .. oo:: MAX_ITEMS
      :choices: <integer>
      :default: 1000

      Maximum number of items fetched. 0=unlimited.

-  .. oo:: COLLECTION

      Name of collection to filter items.

-  .. oo:: CRS

      Name of CRS to filter items.

-  .. oo:: ASSET

      Name of asset to read.

-  .. oo:: RESOLUTION
      :choices: AVERAGE, HIGHEST, LOWEST
      :default: AVERAGE

      Strategy to use to determine dataset resolution.

Subdatasets
-----------

If a STACIT JSON file contains several collections, assets or CRS,
the driver will return a list of subdataset names to open each of the possible
subdatasets.

Driver capabilities
-------------------

.. supports_virtualio::

Examples
--------

List the subdatasets associated to a `STAC search <https://github.com/radiantearth/stac-api-spec/tree/master/item-search>`_
on a given collection, bbox and starting from a datetime:

::

    gdalinfo "STACIT:\"https://planetarycomputer.microsoft.com/api/stac/v1/search?collections=naip&bbox=-100,40,-99,41&datetime=2019-01-01T00:00:00Z%2F..\""


Open a subdataset returned by the above request:

::

    gdalinfo "STACIT:\"https://planetarycomputer.microsoft.com/api/stac/v1/search?collections=naip&bbox=-100,40,-99,41&datetime=2019-01-01T00:00:00Z%2F..\":asset=image"


See Also
--------

-  :ref:`raster.stacta` documentation page.
