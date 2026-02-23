.. _gdal_vector_dissolve:

================================================================================
``gdal vector dissolve``
================================================================================

.. versionadded:: 3.13

.. only:: html

     Unions the elements of each feature's geometry.

.. Index:: gdal vector dissolve

Synopsis
--------

.. program-output:: gdal vector dissolve --help-doc

Description
-----------

:program:`gdal vector dissolve` performs a union operation on the elements of each feature's geometry. This has the following effects:

- Duplicate vertices are eliminated.
- Nodes are added where input linework intersects.
- Polygons that overlap are "dissolved" into a single feature.

For linear geometries, the union step is followed by a line-merging step, where lines are merged at points that form an endpoint of exactly two lines.

To dissolve the geometries of multiple features together, first combine them into single features with :ref:`gdal_vector_combine`.

``dissolve`` can be used as a step of :ref:`gdal_vector_pipeline`.

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
   :title: Dissolve country boundaries into continent boundaries

   .. code-block:: bash

      gdal vector pipeline read countries.shp !
          combine --group-by CONTINENT ! \
          dissolve ! \
          write continents.shp

