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
files, depending on the values the feature take on a subset of attribute or
geometry fields specified by the user and/or by limiting each output layer
to a maximum number of features and/or a maximum file size.

Two partitioning schemes are available:

- ``hive``, corresponding to
  `Apache Hive partitioning <https://arrow.apache.org/docs/python/generated/pyarrow.dataset.HivePartitioning.html>`__,
  is the default one.

  Each partitioning field corresponds to a nested directory. Let's consider a
  layer with fields "continent" and "country", chosen as partitioning fields.
  All features where "continent" evaluates to "Europe" and "country" evaluates to
  "France", will be written in the "continent=Europe/country=France/" subdirectory
  of the output directory.

  NULL values for partitioning fields are encoded as ``__HIVE_DEFAULT_PARTITION__``
  in the directory name. Non-ASCII characters, space, equal sign, or characters
  not compatible with directory name constraints are percent-encoded
  (e.g. ``%20`` for space).

- ``flat`` where files are written directly under the output directory using
  a default filename pattern of ``{LAYER_NAME}_{FIELD_VALUE}_%10d``.

By default, the format of the input dataset will be used for the output, if
it can be determined and the input driver supports writing. Otherwise,
:option:`--format` must be used.

:program:`gdal vector partition` can be used as the last step of a pipeline.

The following options are available:

Program-Specific Options
------------------------

.. option:: --feature-limit <FEATURE-LIMIT>

    Maximum number of features per file. By default, unlimited. If the limit
    is exceeded, several parts are created.

.. option:: --field <FIELD-NAME>

    Fields(s) on which to partition

    Only attribute fields of type String, Integer and Integer64 are allowed.
    The order into which fields are specified matter to determine the directory
    hierarchy.

    Starting with GDAL 3.13, geometry field names can be specified (``OGR_GEOMETRY``
    being the generic name for the first geometry field). Partitioning on
    geometry fields is done on the geometry type. This can be useful for file
    formats where a single geometry type per layer is allowed.

    Starting with GDAL 3.13, :option:`--field` is no longer required, but when
    it is not specified, :option:`--feature-limit` and/or :option:`--max-file-size`
    must be specified.

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
    Automatically set for Parquet output format and Hive partitioning.

.. option:: --output <OUTPUT-DIRECTORY>

    Root of the output directory. [required]

.. option:: --pattern <PATTERN>

    Filename pattern. User chosen string, with substitutions for:

    * ``{LAYER_NAME}``, when found, is substituted with the
      layer name (percent encoded where needed).

    * ``{FIELD_VALUE}``, when found, is substituted with the partitioning field value
      (percent encoded where needed). If several partitioning fields are used,
      each value is separated by underscore (`_`). Empty strings are substituted
      with ``__EMPTY__`` and null fields with ``__NULL__``.

    * ``%[0?][0-9]?[0]?d``: C-style integer formatter for the part number.
      Valid values are for example ``%d`` or ``%05d``.
      One and only one part number specifier must be present in the pattern.

    Default values for the pattern are ``part_%010d`` for the hive scheme,
    and ``{LAYER_NAME}_{FIELD_VALUE}_%010d`` for the flat scheme.`

.. option:: --scheme hive|flat

    Partitioning scheme. Defaults to ``hive``.


Standard Options
----------------

.. collapse:: Details

    .. option:: --append

        Whether the output directory must be opened in append mode. Implies that
        it already exists and that the output format supports appending.

        This mode is useful when adding new features to an already an existing
        partitioned dataset.

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/skip_errors.rst

Examples
--------

.. example::
   :title: Create a partition based on the "continent" and "country" fields

   .. code-block:: bash

        $ gdal vector partition world_cities.gpkg out_directory --field continent,country --format Parquet

.. example::
   :title: Create a partition based on the "country" field, filtering on cities with population bigger than 1 million, with a flat partitioning scheme

   .. code-block:: bash

        $ gdal pipeline ! read world_cities.gpkg ! filter --where "pop > 1e6" ! partition out_directory --field country --format GPKG --scheme flat

.. example::
   :title: Create multiple Shapefiles, one for each geometry type, from a GeoPackage file, by grouping together POLYGON/MULTIPOLYGON or LINESTRING/MULTILINESTRING.

   .. code-block:: bash

        $ gdal vector pipeline ! read input.gpkg ! set-geom-type --multi ! partition out_directory --scheme flat --field OGR_GEOMETRY --format "ESRI Shapefile"

.. example::
   :title: Split a file into files with at most 100,000 features.

   .. code-block:: bash

        $ gdal vector partition world_cities.gpkg out_directory --feature-limit 100000 --scheme flat --format Parquet

.. example::
   :title: Sort a file spatially and split it into files with at most 100,000 features.

   .. code-block:: bash

        $ gdal vector pipeline read world_cities.gpkg ! sort ! partition out_directory --feature-limit 100000 --scheme flat --format Parquet
