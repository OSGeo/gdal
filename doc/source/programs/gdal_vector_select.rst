.. _gdal_vector_select_subcommand:

================================================================================
"gdal vector select" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Select a subset of fields from a vector dataset.

.. Index:: gdal vector select

Synopsis
--------

.. code-block::

    Usage: gdal vector select [OPTIONS] <INPUT> <OUTPUT> <FIELDS>

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]
      -o, --output <OUTPUT>                                Output vector dataset [required]
      --fields <FIELDS>                                    Fields to select (or exclude if --exclude) [may be repeated] [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --version                                            Display GDAL version and exit
      --json-usage                                         Display usage as JSON document and exit
      --drivers                                            Display driver list as JSON document and exit
      --config <KEY>=<VALUE>                               Configuration option [may be repeated]
      --progress                                           Display progress bar

    Options:
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format ("stream" allowed)
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether to open existing dataset in update mode
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      --output-layer <OUTPUT-LAYER>                        Output layer name
      --exclude                                            Exclude specified fields
                                                           Mutually exclusive with --ignore-missing-fields
      --ignore-missing-fields                              Ignore missing fields
                                                           Mutually exclusive with --exclude

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


Description
-----------

:program:`gdal vector select` can be used to select a subset of fields.

``select`` can also be used as a step of :ref:`gdal_vector_pipeline_subcommand`.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/overwrite.rst

.. option:: --fields <FIELDS>

    Comma-separated list of fields from input layer to copy to the new layer
    (or to exclude if :option:`--exclude` is specified)

    Field names with spaces, commas or double-quote
    should be surrounded with a starting and ending double-quote character, and
    double-quote characters in a field name should be escaped with backslash.

    Depending on the shell used, this might require further quoting. For example,
    to select ``regular_field``, ``a_field_with space, and comma`` and
    ``a field with " double quote`` with a Unix shell:

    .. code-block:: bash

        --fields "regular_field,\"a_field_with space, and comma\",\"a field with \\\" double quote\""

    A field is only selected once, even if mentioned several times in the list.

    Geometry fields can also be specified in the list. If the source layer has
    no explicit name for the geometry field, ``_ogr_geometry_`` must be used to
    select the unique geometry field.

    Specifying a non-existing source field name results in an error.

.. option:: --ignore-missing-fields

    By default, if a field specified by :option:`--fields` does not exist in the input
    layer(s), an error is emitted and the processing is stopped.
    When specifying :option:`--ignore-missing-fields`, only a warning is
    emitted and the non existing fields are just ignored.

.. option:: --exclude

    Modifies the behavior of the algorithm such that all fields are selected,
    except the ones mentioned by :option:`--fields`.


Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Select the EAS_ID field and the geometry field from a Shapefile

   .. code-block:: bash

        $ gdal vector select in.shp out.gpkg "EAS_ID,_ogr_geometry_" --overwrite


.. example::
   :title: Remove sensitive fields from a layer

   .. code-block:: bash

        $ gdal vector select in.shp out.gpkg --exclude "name,surname,address" --overwrite
