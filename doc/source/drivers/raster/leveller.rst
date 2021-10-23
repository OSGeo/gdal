.. _raster.leveller:

================================================================================
Leveller -- Daylon Leveller Heightfield
================================================================================

.. shortname:: Leveller

.. built_in_by_default::

Leveller heightfields store 32-bit elevation values. Format versions 4
through 9 are supported with various caveats (see below). The file
extension for Leveller heightfields is "TER" (which is the same as
Terragen, but the driver only recognizes Leveller files).

Blocks are organized as pixel-high scanlines (rows), with the first
scanline at the top (north) edge of the DEM, and adjacent pixels on each
line increasing from left to right (west to east).

The band type is always Float32, even though format versions 4 and 5
physically use 16.16 fixed-point. The driver auto-converts them to
floating-point.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Reading
-------

``dataset::GetProjectionRef()`` will return only a local coordinate
system for file versions 4 through 6.

``dataset::GetGeoTransform()`` returns a simple world scaling with a
centered origin for formats 4 through 6. For versions 7 and higher, it
returns a real-world transform except for rotations. The identity
transform is not considered an error condition; Leveller documents often
use them.

``band::GetUnitType()`` will report the measurement units used by the
file instead of converting unusual types to meters. A list of unit types
is in the ``levellerdataset.cpp`` module.

``band::GetScale()`` and ``band::GetOffset()`` will return the
physical-to-logical (i.e., raw to real-world) transform for the
elevation data.

Writing
-------

The ``dataset::Create()`` call is supported, but for version 7 files
only.

``band::SetUnitType()`` can be set to any of the unit types listed in
the ``levellerdataset.cpp`` module.

``dataset::SetGeoTransform()`` should not include rotation data.

As with the Terragen driver, the ``MINUSERPIXELVALUE`` option must be
specified. This lets the driver correctly map from logical (real-world)
elevations to physical elevations.

Header information is written out on the first call to
``band::IWriteBlock``.

See Also:
---------

-  Implemented as ``gdal/frmts/leveller/levellerdataset.cpp``.
-  Visit `Daylon Graphics <http://www.daylongraphics.com>`__ for the
   Leveller SDK, which documents the Leveller format.
