.. _geometry_validity:

Geometry Validity
=================

Several functions and utilities in GDAL deal with the concepts of "valid" and "invalid" geometries. 
Validity in this context refers to the `OGR Simple Features standard <https://www.ogc.org/standards/sfa/>`__.
Incorrect results may be obtained when invalid geometries are used in spatial algorithms such as intersection testing/computation.

Geometry validity can be checked using the :ref:`gdal_vector_check_geometry` command-line utility or the API functions :cpp:func:`OGRGeometry::IsValid` (C++) and :py:meth:`ogr.Geometry.IsValid` (Python).
In many cases, invalid geometries can be repaired using the :ref:`gdal_vector_make_valid` command-line utility or the API functions :cpp:func:`OGRGeometry::MakeValid` (C++) and :py:meth:`ogr.Geometry.MakeValid` (Python).

Geometry validity does not consider interactions between features, such as gaps and overlaps, although :ref:`gdal_vector_check_coverage` provides some means for evaluating this.
Qualitative errors such as spikes and slivers are outside the scope of validity testing.

Validity checking
^^^^^^^^^^^^^^^^^

GDAL relies on the `GEOS <https://libgeos.org>`__ library to check the validity of geometries. 
This library is widely used by open source geospatial software, so geometries considered valid by GDAL should be considered valid by QGIS, PostGIS, shapely, etc.
Still, some geometries considered valid by GEOS may not be considered valid in other software.
The following limitations apply to validity testing:

- GEOS does not enforce any particular ring orientation for polygon shells and holes, and permits repeated points within a ring.

- GEOS considers only two dimensions when checking geometry validity. For example, if two elements of a MultiPolygon occupy the same space but with different Z values, they will still be considered invalid by GEOS and therefore GDAL.

- Curved geometries are approximated as linear geometries before being evaluated by GEOS. Linearized geometries may be valid where the original geometries are not, and vice-versa.

Finally, not all software uses the OGC model for invalid geometries.
As a simple example, the self-touching ring shown :ref:`below <polygon_self_touching>` is considered valid in the Esri geometry model.

Geometry repair
^^^^^^^^^^^^^^^

In many cases, the correct representation of an invalid geometry is ambiguous.
GEOS provides two different algorithms for constructing a valid geometry from an invalid one.
The "linework" method attempts to preserve as many of the input lines as possible, in some cases converting polygon holes into shells and vice-versa.
On the other hand, the "structure" method can remove input lines such that areas covered by a polygon shell in the input remain covered in the output.

In some cases, such as a polygon with a non-closed ring, an invalid geometry cannot be represented in the GEOS library and therefore cannot be repaired automatically.

Invalid geometry examples
-------------------------

The cases below illustrate a number of topological invalidities, how they are reported by :ref:`gdal_vector_check_geometry`, and how they may be resolved by the different methods of :ref:`gdal_vector_make_valid`.

.. include:: geometry_validity_examples.rst
