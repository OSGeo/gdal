.. _vector.sua:

SUA - Tim Newport-Peace's Special Use Airspace Format
=====================================================

.. shortname:: SUA

.. built_in_by_default::

This driver reads files describing Special Use Airspaces in the Tim
Newport-Peace's .SUA format

Airspace are returned as features of a single layer, with a geometry of
type Polygon and the following fields : TYPE, CLASS, TITLE, TOPS, BASE.

Airspace geometries made of arcs will be tessellated.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `Description of .SUA
   format <http://soaring.gahsys.com/TP/sua.html>`__
