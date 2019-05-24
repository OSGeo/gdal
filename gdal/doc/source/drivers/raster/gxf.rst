.. _raster.gxf:

================================================================================
GXF -- Grid eXchange File
================================================================================

.. shortname:: GXF

This is a raster exchange format propagated by Geosoft, and made a
standard in the gravity/magnetics field. GDAL supports reading (but not
writing) GXF-3 files, including support for georeferencing information,
and projections.

By default, the datatype returned for GXF datasets by GDAL is Float32.
From GDAL 1.8.0, you can specify the datatype by setting the
GXF_DATATYPE configuration option (Float64 supported currently)

Details on the supporting code, and format can be found on the
`GXF-3 <https://web.archive.org/web/20130730111701/http://home.gdal.org/projects/gxf/index.html>`__ page.

NOTE: Implemented as ``gdal/frmts/gxf/gxfdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

