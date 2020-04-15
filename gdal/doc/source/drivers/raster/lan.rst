.. _raster.lan:

================================================================================
LAN -- Erdas 7.x .LAN and .GIS
================================================================================

.. shortname:: LAN

.. built_in_by_default::

GDAL supports reading and writing Erdas 7.x .LAN and .GIS raster files.
Currently 4bit, 8bit and 16bit pixel data types are supported for
reading and 8bit and 16bit for writing.

GDAL does read the map extents (geotransform) from LAN/GIS files, and
attempts to read the coordinate system information. However, this format
of file does not include complete coordinate system information, so for
state plane and UTM coordinate systems a LOCAL_CS definition is returned
with valid linear units but no other meaningful information.

The .TRL, .PRO and worldfiles are ignored at this time.

NOTE: Implemented as ``gdal/frmts/raw/landataset.cpp``

Development of this driver was financially supported by Kevin Flanders
of (`PeopleGIS <http://www.peoplegis.com>`__).

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
