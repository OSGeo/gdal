.. _vector.lvbag:

================================================================================
Dutch Kadaster LV BAG 2.0 Extract
================================================================================
.. versionadded:: 3.2


.. shortname:: LVBAG

.. build_dependencies:: libexpat

This driver can read XML files in the LV BAG 2.0 extract format as provided by
the Dutch Kadaster BAG products. All LV BAG 2.0 extract products are supported.
The driver supports all BAG layers including thorse introduced in BAG 2.0.

The driver is only available if GDAL/OGR is compiled against the Expat
library.

Each extract XML file is presented as a single OGR layer. The layers are
georeferenced in their native (EPSG:28992) SRS.

More information about the LV BAG 2.0 can be found at https://zakelijk.kadaster.nl/bag-2.0

LV BAG model definitions are available at https://zakelijk.kadaster.nl/documents/20838/87954/XSD%27s+BAG+2.0+Extract

Note 1 : the earlier BAG 1.0 extract is not supported by this driver.

Note 2 : the driver will only read ST (Standaard Levering) extract files. Mutation
ML (Mutatie Levering) files are not supported.

Driver capabilities
-------------------

.. supports_virtualio::

Example
-------

The ogr2ogr utility can be used to dump the results of a LV BAG extract
to WGS84 in GeoJSON:

::

   ogr2ogr -t_srs EPSG:4326 -f GeoJSON output.json 9999PND01012020_000001.xml

How to dump contents of extract file as OGR sees it:

::

   ogrinfo -ro 9999PND01012020_000001.xml
