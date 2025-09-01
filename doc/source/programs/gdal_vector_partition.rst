.. _gdal_vector_partition:

================================================================================
``gdal vector partition``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Partition a vector dataset into multiple files.

.. Index:: gdal vector partition

Synopsis
--------

.. program-output:: gdal vector partition --help-doc

Description
-----------

:program:`gdal vector partition` dispatches features into different
files, depending on the values the feature take on a subset of fields specified
by the user.

`Apache Hive partitioning <https://arrow.apache.org/docs/python/generated/pyarrow.dataset.HivePartitioning.html>`__
is used as the partitioning scheme.

Each partitioning field corresponds to a nested directory. Let's consider a
layer with fields "continent" and "country", chosen as partitioning fields.
All features where "continent" evaluates to "Europe" and "country" evaluates to
"France", will be written in the "continent=Europe/country=France/" subdirectory
of the output directory.

NULL values for partitioning fields are encoded as ``__HIVE_DEFAULT_PARTITION__``
in the directory name. Non-ASCII characters, space, equal sign, or characters
not compatible with directory name constraints are percent-encoded
(e.g. ``%20`` for space).

By default, the format of the input dataset will be used for the output, if
it can be determined and the input driver supports writing. Otherwise,
:option:`--format` must be used.

:program:`gdal vector partition` can be used as the last step of a pipeline.

The following options are available:

Standard options
++++++++++++++++

.. option:: --output <OUTPUT-DIRECTORY>

    Root of the output directory. [required]

.. option:: --field <FIELD-NAME>

    Fields(s) on which to partition. [required]

    Only fields of type String, Integer and Integer64 are allowed.
    The order into which fields are specified matter to determine the directory
    hierarchy.

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/lco.rst

.. include:: gdal_options/overwrite.rst

.. option:: --append

    Whether the output directory must be opened in append mode. Implies that
    it already exists and that the output format supports appending.

    This mode is useful when adding new features to an already an existing
    partitioned dataset.

.. option:: --feature-limit <FEATURE-LIMIT>

    Maximum number of features per file. By default, unlimited. If the limit
    is exceeded, several parts are created.

.. option:: --max-file-size <MAX-FILE-SIZE>

    Maximum file size (MB or GB suffix can be used). By default, unlimited.
    If the limit is exceeded, several parts are created.

    Note that the maximum file size is used as a hint, and might not be
    strictly respected, because the evaluation of the file size corresponding
    to a feature is based on a heuristics, as the file size itself cannot be
    reliably used when it is under writing. In particular, the heuristics does
    not assume any compression, so for compressed formats, the actual size of
    a part can be significantly smaller than the specified limit.

.. option:: --omit-partitioned-field

    Whether to omit partitioned fields from the target layer definition.
    Automatically set for Parquet output format.

.. option:: --skip-errors

    Whether failures to write feature(s) should be ignored. Note that this option
    sets the size of the transaction unit to one feature at a time, which may
    cause severe slowdown when inserting into databases.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Create a partition based on the "continent" and "country" fields

   .. code-block:: bash

        $ gdal vector partition in.gpkg out_directory --field continent,country --format Parquet

.. example::
   :title: Create a partition based on the "country" field, filtering on cities with population bigger than 1 million

   .. code-block:: bash

        $ gdal pipeline ! read in.gpkg ! filter --where "pop > 1e6" ! partition out_directory --field country --format GPKG
