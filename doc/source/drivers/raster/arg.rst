.. _raster.arg:

================================================================================
ARG -- Azavea Raster Grid
================================================================================

.. shortname:: ARG

.. built_in_by_default::

Driver implementation for a raw format that is used in
`GeoTrellis <http://geotrellis.io/>`__ and called ARG. `ARG format
specification <http://geotrellis.io/documentation/0.9.0/geotrellis/io/arg/>`__.
Format is essentially a raw format, with a companion .JSON file.

.. warning::

    This driver is deprecated (https://github.com/OSGeo/gdal/issues/7920) and
    will be removed in GDAL 3.9. You are invited to convert any dataset in that
    format to another more common one.
    If you need this driver in future GDAL versions, create a ticket at
    https://github.com/OSGeo/gdal (look first for an existing one first)
    to explain how critical it is for you (but the GDAL project may still remove it)


NOTE: Implemented as :source_file:`frmts/arg/argdataset.cpp`.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
