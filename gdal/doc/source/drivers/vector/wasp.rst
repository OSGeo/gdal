.. _vector.wasp:

WAsP - WAsP .map format
=======================

.. shortname:: WAsP

.. built_in_by_default::

This driver writes .map files to be used with WAsP. The only allowed
geometries are linestrings.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Configuration options
---------------------

-  WASP_FIELDS : a comma separated list of fields. For elevation, the
   name of the height field. For roughness, the name of the left and
   right roughness fields resp.
-  WASP_MERGE : this may be set to "NO". Used only when generating
   roughness from polygons. All polygon boundaries will be output
   (including those with the same left and right roughness). This is
   useful (along with option -skipfailures) for debugging incorrect
   input geometries.
-  WASP_GEOM_FIELD : in case input has several geometry columns and the
   first one (default) is not the right one.
-  WASP_TOLERANCE : specify a tolerance for line simplification of
   output (calls geos).
-  WASP_ADJ_TOLER : points that are less than tolerance apart from
   previous point on x and on y are omitted.
-  WASP_POINT_TO_CIRCLE_RADIUS : lines that became points due to
   simplification are replaces by 8 point circles (octagons).

Note that if not option is specified, the layer is assumed to be an
elevation layer where the elevation is the z-components of the
linestrings' points.
