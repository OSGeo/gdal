.. _raster.rik:

================================================================================
RIK -- Swedish Grid Maps
================================================================================

.. shortname:: RIK

.. build_dependencies:: (internal zlib is used if necessary)

Supported by GDAL for read access. This format is used in maps issued by
the swedish organization Lantmäteriet. Supports versions 1, 2 and 3 of
the RIK format, but only 8 bits per pixel.

This driver is based on the work done in the
`TRikPanel <http://sourceforge.net/projects/trikpanel/>`__ project.

NOTE: Implemented as ``gdal/frmts/rik/rikdataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
