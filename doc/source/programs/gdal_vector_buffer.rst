.. _gdal_vector_buffer:

================================================================================
``gdal vector buffer``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Compute a buffer around geometries of a vector dataset.

.. Index:: gdal vector buffer

Synopsis
--------

.. program-output:: gdal vector buffer --help-doc

Description
-----------

.. below is courtesy of https://postgis.net/docs/ST_Buffer.html

:program:`gdal vector buffer` computes a POLYGON or MULTIPOLYGON that
represents all points whose distance from a geometry/geography is less than or
equal to a given distance. A negative distance shrinks the geometry rather than
expanding it. A negative distance may shrink a polygon completely, in which case
POLYGON EMPTY is returned. For points and lines negative distances always return
empty results.

See https://postgis.net/docs/ST_Buffer.html for graphical illustrations of the
effect of the different parameters.

This command can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

.. note:: This command requires a GDAL build against the GEOS library.

Program-Specific Options
------------------------

.. option:: --distance <DISTANCE>

    Radius of the buffer around the input geometry.
    The unit of the distance is in georeferenced units of the source layer.

.. option:: --endcap-style round|flat|square

    Specifies the end cap style of the generated buffer.
    Default is ``round``.

.. option:: --join-style round|mitre|bevel

    Sets the join style for outside (reflex) corners between line segments.
    Default is ``round``.

.. option:: --input-layer <NAME>

    Specifies one or more layer names to read and process. By default, all
    layers will be read and processed. To read and write all layers but only
    process a subset, use :option:`--active-layer`.

.. option:: --mitre-limit <MITRE-LIMIT>

    Sets the limit on the mitre ratio used for very sharp corners.

    Default is 5.

    .. below is courtesy of GEOS BufferParameters.h

    The mitre ratio is the ratio of the distance from the corner
    to the end of the mitred offset corner.
    When two line segments meet at a sharp angle,
    a miter join will extend far beyond the original geometry.
    (and in the extreme case will be infinitely far.)
    To prevent unreasonable geometry, the mitre limit
    allows controlling the maximum length of the join corner.
    Corners with a ratio which exceed the limit will be beveled.

.. option:: --output-layer <NAME>

   Name of the layer to which output should be written. If not specified,
   the output layer will have the same name as the input layer.

.. option:: --quadrant-segments <QUADRANT-SEGMENTS>

    .. below is courtesy of GEOS BufferParameters.h

    Sets the number of line segments used to approximate an angle fillet in round joins.

    This determines the maximum error in the approximation to the true buffer curve.
    The default value of 8 gives less than 2% max error in the
    buffer distance.
    For a max error of < 1%, use QS = 12.
    For a max error of < 0.1%, use QS = 18.
    The error is always less than the buffer distance
    (in other words, the computed buffer curve is always inside
    the true curve).

.. option:: --side both|left|right

    Sets whether the computed buffer should be single-sided or on both side (default).

    ``left`` (resp. ``right``) means that buffer is on the left-hand
     (resp. right-hand) side of the line when following it in the order of its vertices.

    Single-side buffering is only applicable to LINESTRING geometry and does not
    affect POINT or POLYGON geometries, and the end cap style is forced to square.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/active_layer.rst

    .. include:: gdal_options/active_geometry.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst
       
    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst


Examples
--------

.. example::
   :title: Compute a buffer of one km around input geometries (assuming the CRS is in meters)

   .. code-block:: bash

        $ gdal vector buffer --distance=1000 in.gpkg out.gpkg --overwrite


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    mitre
    mitred
