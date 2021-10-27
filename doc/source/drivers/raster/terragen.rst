.. _raster.terragen:

================================================================================
Terragen -- Terragen™ Terrain File
================================================================================

.. shortname:: Terragen

.. built_in_by_default::

Terragen terrain files store 16-bit elevation values with optional
gridspacing (but not positioning). The file extension for Terragen
heightfields is "TER" or "TERRAIN" (which in the former case is the same
as Leveller, but the driver only recognizes Terragen files). The driver
ID is "Terragen". The dataset is file-based and has only one elevation
band. Void elevations are not supported. Pixels are considered points.


Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Reading
-------

   ``dataset::GetProjectionRef()`` returns a local coordinate system
   using meters.

   ``band::GetUnitType()`` returns meters.

   Elevations are ``Int16``. You must use the ``band::GetScale()`` and
   ``band::GetOffset()`` to convert them to meters.

   |  

Writing
-------

   Use the ``Create`` call. Set the ``MINUSERPIXELVALUE`` option (a
   float) to the lowest elevation of your elevation data, and
   ``MAXUSERPIXELVALUE`` to the highest. The units must match the
   elevation units you will give to ``band::SetUnitType()``.

   Call ``dataset::SetProjection()`` and ``dataset::SetGeoTransform()``
   with your coordinate system details. Otherwise, the driver will not
   encode physical elevations properly. Geographic (degree-based)
   coordinate systems will be converted to a local meter-based system.

   To maintain precision, a best-fit baseheight and scaling will be used
   to use as much of the 16-bit range as possible.

   Elevations are ``Float32``.

   |  

Roundtripping
-------------

   Errors per trip tend to be a few centimeters for elevations and up to
   one or two meters for ground extents if degree-based coordinate
   systems are written. Large degree-based DEMs incur unavoidable
   distortions since the driver currently only uses meters.

See Also
--------

-  Implemented as ``gdal/frmts/terragen/terragendataset.cpp``.
-  See `readme.txt <./readme.txt>`__ for installation and support
   information.
-  `Terragen Terrain File
   Specification <http://www.planetside.co.uk/terragen/dev/tgterrain.html>`__.
