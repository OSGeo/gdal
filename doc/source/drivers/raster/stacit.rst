.. _raster.stacit:

================================================================================
STACIT - Spatio-Temporal Asset Catalog Items
================================================================================

.. versionadded:: 3.4

.. shortname:: STACIT

.. built_in_by_default::

This driver supports opening JSON files, or usually the result of a remote query,
that are the ``items`` link of a
`STAC Collection <https://github.com/radiantearth/stac-api-spec/blob/master/stac-spec/collection-spec/collection-spec.md>`_,
and whose items also implement the
`Projection Extension Specification <https://github.com/stac-extensions/projection/>`_.
It builds a virtual mosaic from the items.

A STACIT dataset which has no subdatasets is actually a :ref:`raster.vrt` dataset.
Thus, translating it into VRT will result in a VRT file that directly references the items.

Open syntax
-----------

STACIT datasets/subdatasets can be accessed with one of the following syntaxes:

* ``filename.json``: local file

* ``STACIT:"https://example.com/filename.json"``: remote file

* ``STACIT:"filename.json":asset=my_asset``: specify an asset of a local/remote file

* ``STACIT:"filename.json":collection=my_collect,asset=my_asset``: specify a collection + asset of a local/remote file

* ``STACIT:"filename.json":collection=my_collect,asset=my_asset,crs=my_crs``: specify a collection + asset + CRS of a local/remote file

Open options
------------

The following open options are supported:

* ``MAX_ITEMS`` = number. Maximum number of items fetched. 0=unlimited. Default is 1000.

* ``COLLECTION`` = string. Name of collection to filter items.

* ``ASSET`` = string. Name of asset to filter items.

* ``CRS`` = string. Name of CRS to filter items.

* ``RESOLUTION`` = AVERAGE/HIGHEST/LOWEST. Strategy to use to determine dataset resolution. Default is AVERAGE.

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
