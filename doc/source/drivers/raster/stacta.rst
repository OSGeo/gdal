.. _raster.stacta:

================================================================================
STACTA - Spatio-Temporal Asset Catalog Tiled Assets
================================================================================

.. versionadded:: 3.3

.. shortname:: STACTA

.. built_in_by_default::

This driver supports opening JSON files following the
`Spatio-Temporal Asset Catalog Tiled Assets <https://github.com/stac-extensions/tiled-assets>`_
specification. Such JSON file references tiles (also called metatiles), split
from a potentially big dataset according to a tiling scheme, with several zoom
levels. The driver provides a single raster view, with overviews, of the dataset
described by the JSON file. The driver supports metatiles of arbitrary size.

The driver may use the `Electro-Optical Extension <https://github.com/stac-extensions/eo>`__
and, starting with GDAL 3.8.2, the `Raster Extension <https://github.com/stac-extensions/raster>`__
attached to an asset template.

Configuration options
---------------------

|about-config-options|
The following configuration options are
available:

-  .. config:: GDAL_STACTA_SKIP_MISSING_METATILE

      See :oo:`SKIP_MISSING_METATILE` open option.

Open syntax
-----------

STACTA datasets/subdatasets can be accessed with one of the following syntaxes:

* ``filename.json``: local file

* ``STACTA:"https://example.com/filename.json"``: remote file

* ``STACTA:"filename.json":my_asset``: specify an asset of a local/remote file

* ``STACTA:"filename.json":my_asset:my_tms``: specify an asset and tiling scheme of a local/remote file

The root of the JSON file must be of type ``Feature``.

Starting with GDAL 3.10, specifying the ``-if STACTA`` option to command line utilities
accepting it, or ``STACTA`` as the only value of the ``papszAllowedDrivers`` of
:cpp:func:`GDALOpenEx`, also forces the driver to recognize the passed
filename or URL.

Open options
------------

|about-open-options|
The following open options are supported:

-  .. oo:: WHOLE_METATILE
      :choices: YES, NO.

      If set to YES, metatiles will be entirely downloaded
      (into memory). Otherwise by default, if metatiles are bigger than a threshold,
      they will be accessed in a piece-wise way.

-  .. oo:: SKIP_MISSING_METATILE
      :choices: YES, NO

      If set to YES, metatiles that are missing
      will be skipped without error, and corresponding area in the dataset will be
      filled with the nodata value or zero if there is no nodata value. This setting
      can also be set with the :config:`GDAL_STACTA_SKIP_MISSING_METATILE`
      configuration option.

Subdatasets
-----------

If a STACTA JSON file contains several asset templates and/or tiling scheme,
the driver will return a list of subdataset names to open each of the possible
subdatasets.

Driver capabilities
-------------------

.. supports_virtualio::

See Also
--------

-  :ref:`raster.stacit` documentation page.
