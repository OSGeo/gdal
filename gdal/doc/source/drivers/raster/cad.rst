.. _raster.cad:

================================================================================
CAD -- AutoCAD DWG raster layer
================================================================================

.. shortname:: CAD

.. versionadded:: 2.2

.. build_dependencies:: (internal libopencad provided)

OGR DWG support is based on libopencad, so the list of supported DWG
(DXF) versions can be seen in libopencad documentation. All drawing
entities are separated into layers as they are in DWG file. The rasters
are usually a separate georeferenced files (GeoTiff, Jpeg, Png etc.)
which exist in DWG file as separate layers. The driver try to get
spatial reference and other methadata from DWG Image description and set
it to GDALDataset.

NOTE: Implemented as ``ogr/ogrsf_frmts/cad/gdalcaddataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
