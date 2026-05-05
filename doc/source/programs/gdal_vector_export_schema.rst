.. _gdal_vector_export_schema:

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

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/if.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Extracting the OGR_SCHEMA from the file :file:`poly.gpkg`

   .. code-block:: bash

       gdal vector export-schema poly.gpkg
