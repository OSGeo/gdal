.. _vector.gdalg:

================================================================================
GDALG: GDAL Streamed Algorithm
================================================================================

.. versionadded:: 3.11

.. shortname:: GDALG

.. built_in_by_default::

.. note:: GDALG is the contraction of GDAL and ALGorithm.

This is a read-only driver that reads a JSON file containing an invocation
of the :ref:`gdal command line interface <gdal_program>`, that results in a
on-the-fly / streamed vector dataset when used as an input
to :cpp:func:`GDALOpenEx`, or anywhere else in GDAL
where a vector input dataset is expected. GDALG files are conceptually close
to :ref:`VRT (Virtual) files <vector.vrt>`, although the implementation is
substantially different.

The subset of commands of :program:`gdal` that can generate such streamed datasets
indicate support for writing ``GDALG`` in the ``--help`` description for
``--output-format``. This includes :ref:`gdal_vector_pipeline`,
:ref:`gdal_vector_reproject`, :ref:`gdal_vector_filter`, :ref:`gdal_vector_select`,
:ref:`gdal_vector_sql`, etc.

It is recommended that GDALG files use the ``.gdalg.json`` extension.

The JSON document must include a ``type`` member with the value ``gdal_streamed_alg``,
and a ``command_line`` member, which is very close to the one that would be used
to generate a materialized dataset, but using the ``stream`` output format.

.. code-block:: json

    {
        "type": "gdal_streamed_alg",
        "command_line": "gdal vector reproject --dst-crs=EPSG:32632 in.gpkg --output-format stream streamed_dataset"
    }

If such file is saved as ``reprojected.gdalg.json``, it can be for example read with
``gdal vector info reprojected.gdalg.json``.

In the case of a :ref:`gdal_vector_pipeline`, the final ``write`` step can be
omitted.

.. code-block:: json

    {
        "type": "gdal_streamed_alg",
        "command_line": "gdal vector pipeline ! read in.tif ! reproject --dst-crs=EPSG:32632 ! select --fields fid,geom"
    }

An optional ``relative_paths_relative_to_this_file`` boolean member defaults to ``true``,
to indicate that relative input filenames in the command line should be interpreted
as relative to the location of the ``.gdalg.json`` file. If setting this member to ``false``,
they will be interpreted as being relative to the current working directory.

``.gdalg.json`` files can be validated against the following
JSON schema :source_file:`frmts/gdalg/data/gdalg.schema.json`

This driver has also raster capabilities as detailed in :ref:`raster.gdalg`.

Driver capabilities
-------------------

.. supports_georeferencing::
