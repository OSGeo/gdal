.. _vector.openair:

OpenAir - OpenAir Special Use Airspace Format
=============================================

.. shortname:: OpenAir

This driver reads files describing Special Use Airspaces in the OpenAir
format

Airspace are returned as features of a single layer called 'airspaces',
with a geometry of type Polygon and the following fields: CLASS, NAME,
FLOOR, CEILING.

Airspace geometries made of arcs will be tessellated. Styling
information when present is returned at the feature level.

An extra layer called 'labels' will contain a feature for each label (AT
element). There can be multiple AT records for a single airspace
segment. The fields are the same as the 'airspaces' layer.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `Description of OpenAir
   format <http://www.winpilot.com/UsersGuide/UserAirspace.asp>`__
