.. _gdal_driver_parquet_create_metadata_file:

================================================================================
``gdal driver parquet create-metadata-file``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Create the _metadata file for a partitioned Parquet dataset

.. Index:: gdal driver parquet create-metadata-file

Synopsis
--------

::

    Usage: gdal driver parquet create-metadata-file [OPTIONS] <INPUT>... <OUTPUT>

    Create the _metadata file for a partitioned dataset

    Positional arguments:
      --input <INPUT>         Input Parquet datasets (created by algorithm) [1.. values] [required]
      --output <OUTPUT>       Output Parquet dataset [required]

    Common Options:
      -h, --help              Display help message and exit
      --json-usage            Display usage as JSON document and exit
      --config <KEY>=<VALUE>  Configuration option [may be repeated]

    Options:
      --overwrite             Whether overwriting existing output is allowed

Description
-----------

Creates a :file:`_metadata` Parquet metadata-only file that contains the schema
of the partitioned Parquet files and the path to them.

This program is automatically called by :ref:`gdal_vector_partition` for a Parquet
output.

Examples
--------

.. example::

   .. code-block:: bash

       gdal driver parquet create-metadata-file --input partitioned/0.parquet \
                   --input partitioned/1.parquet --output partitioned/_metadata
