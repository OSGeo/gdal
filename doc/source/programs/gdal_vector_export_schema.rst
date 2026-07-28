.. _gdal_vector_export_schema:

.. program:: gdal_vector_export_schema

================================================================================
``gdal vector export-schema``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Export the OGR_SCHEMA from a vector dataset.

    OGR_SCHEMA is a JSON object describing the structure of a vector dataset
    according to the schema definition at :source_file:`ogr/data/ogr_fields_override.schema.json`

.. Index:: gdal vector export-schema

Synopsis
--------

.. program-output:: gdal vector export-schema --help-doc

Description
-----------

:program:`gdal vector export-schema` exports the ``OGR_SCHEMA`` from a GDAL-supported
vector dataset, and returns it on the standard output stream when used from the
command line, or in the ``output`` parameter when used from the API.

``OGR_SCHEMA`` is a JSON object describing the structure of a vector dataset
according to the schema definition at :source_file:`ogr/data/ogr_fields_override.schema.json`

:program:`gdal vector export-schema` can be used as the last
step of a pipeline.

The following options are available:

Program-Specific Options
------------------------

.. option:: -l, --layer, --input-layer <INPUT-LAYER>

    Name of one or more layers to inspect. If no layer names are passed,
    then all layers will be selected.


Standard Options
----------------

.. collapse:: Details

    .. option:: -o, --output <OUTPUT>

        .. versionadded:: 3.14

        Filename to write the ``OGR_SCHEMA``. If not specified, the output will be written to the standard output stream.

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/overwrite.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Extracting the OGR_SCHEMA from the file :file:`poly.gpkg`

   .. code-block:: bash

       gdal vector export-schema poly.gpkg

.. example::
   :title: Save the OGR_SCHEMA to a file

   When using Windows PowerShell 5.1, redirecting output with ``>`` or ``Out-File -Encoding utf8``
   produces UTF-8 with a BOM (Byte Order Mark). When the schema is read by :ref:`gdal_vector_create`,
   this may result in ``ERROR 1: JSON parsing error: unexpected character (at offset 0)``.

   To avoid encoding issues, write the file using the :option:`--output` option.

   .. tabs::

     .. code-tab:: bash

         gdal vector export-schema natural_earth_vector.gpkg --layer "ne_50m_admin_0_countries" > countries.json

     .. code-tab:: ps1

        # PowerShell (all versions)
        gdal vector export-schema natural_earth_vector.gpkg --layer "ne_50m_admin_0_countries" --output countries.json --overwrite

        # PowerShell 7+ only
        gdal vector export-schema natural_earth_vector.gpkg --layer "ne_50m_admin_0_countries" | Out-File countries.json -Encoding utf8

.. example::
   :title: Validate an OGR_SCHEMA file using Python

   Validate against the latest version of the schema definition at :source_file:`ogr/data/ogr_fields_override.schema.json`
   using the Python package `check-jsonschema <https://pypi.org/project/check-jsonschema/>`__ available on PyPI.

   .. code-block:: bash

       $ pip install check-jsonschema
       $ check-jsonschema --schemafile https://raw.githubusercontent.com/OSGeo/gdal/master/ogr/data/ogr_fields_override.schema.json countries.json --verbose

.. example::
   :title: Create an OGR_SCHEMA at the end of a pipeline

   This example renames the field ``pop_est`` to ``estimated_population`` and changes its
   type from ``Real`` to ``Integer``, before exporting the resulting OGR_SCHEMA.

   .. tabs::

     .. code-tab:: bash

        gdal vector pipeline \
            ! read natural_earth_vector.gpkg --layer ne_50m_admin_0_countries \
            ! sql --sql "SELECT geom,name,abbrev,pop_est AS estimated_population FROM ne_50m_admin_0_countries" --output-layer "countries" \
            ! set-field-type --field-name "estimated_population" --field-type Integer \
            ! export-schema --output countries-population.json

     .. code-tab:: ps1

        gdal vector pipeline `
            ! read natural_earth_vector.gpkg --layer ne_50m_admin_0_countries `
            ! sql --sql "SELECT geom,name,abbrev,pop_est AS estimated_population FROM ne_50m_admin_0_countries" --output-layer "countries" `
            ! set-field-type --field-name "estimated_population" --field-type Integer `
            ! export-schema --output countries-population.json
