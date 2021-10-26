.. _vector.sdts:

SDTS
====

.. shortname:: SDTS

.. built_in_by_default::

SDTS TVP (Topological Vector Profile) and Point Profile datasets are
supported for read access. Each primary attribute, node (point), line
and polygon module is treated as a distinct layer.

To select an SDTS transfer, the name of the catalog file should be used.
For instance ``TR01CATD.DDF`` where the first four characters are all
that typically varies.

SDTS coordinate system information is properly supported for most
coordinate systems defined in SDTS.

There is no update or creation support in the SDTS driver.

Note that in TVP datasets the polygon geometry is formed from the
geometry in the line modules. Primary attribute module attributes should
be properly attached to their related node, line or polygon features,
but can be accessed separately as their own layers.

This driver has no support for raster (DEM) SDTS datasets.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `SDTS Abstraction
   Library <https://web.archive.org/web/20130730111701/http://home.gdal.org/projects/sdts/index.html>`__: The base
   library used to implement this driver.
-  `http://mcmcweb.er.usgs.gov/sdts <http://mcmcweb.er.usgs.gov/sdts/>`__:
   Main USGS SDTS web page.
