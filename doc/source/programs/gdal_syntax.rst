.. _gdal_syntax:

================================================================================
Syntax for commands of ``gdal`` program
================================================================================

.. versionadded:: 3.11

* Commands accept one or several positional arguments, typically for dataset
  names. The order is input(s) first, output last.

  Positional arguments can also be specified as named arguments, if preferred
  to avoid any ambiguity.

* Named arguments have:

  - at least one "long" name, preceded by two dash characters when specified on
    the command line,

  - optionally auxiliary long names,

  - and optionally a one-letter short name, preceded by a single dash character
    on the command line.

  e.g. ``-f, --of, --format, --output-format <OUTPUT-FORMAT>``

  Boolean arguments (also called flags) are enabled by specifying the argument name without a value.

  e.g. ``--overwrite``

  Arguments that require a value are specified like:

  - ``-f VALUE`` for one-letter short names.

  - ``--format VALUE`` or ``--format=VALUE`` for long names.

  Some arguments can be multi-valued. Some of them require all values to be
  packed together and separated by commas. This is, for example, the case of
  ``--bbox <BBOX>   Clipping bounding box as xmin,ymin,xmax,ymax``.
  e.g. ``--bbox=2.1,49.1,2.9,49.9``

  Others accept each value to be preceded by a new mention of the argument name.
  e.g ``--co COMPRESS=LZW --co TILED=YES``. In this example, if the value of the
  argument does not contain commas, the packed form is also accepted:
  ``--co COMPRESS=LZW,TILED=YES``.

* Named arguments can be placed before or after positional arguments.

Example
+++++++

Given the following (simplified) synopsis:

.. code-block::

    gdal raster convert [OPTIONS] <INPUT> <OUTPUT>

    Convert a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]
      -o, --output <OUTPUT>                                Output raster dataset (created by algorithm) [required]

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed


The following command lines are valid:

.. code-block::

    gdal raster convert input.tif output.tif
    gdal raster convert -i input.tif -o output.tif
    gdal raster convert --input=input.tif --output=output.tif
    gdal raster convert --input input.tif --output output.tif
    gdal raster convert --creation-option COMPRESS=LZW --creation-option TILED=YES --overwrite input.tif output.tif
    gdal raster convert input.tif output.tif --co COMPRESS=LZW,TILED=YES --overwrite


Suggestions for argument values
+++++++++++++++++++++++++++++++

As an alternative to :ref:`gdal_bash_completion`, it is possible to ask for
potential enumerated values for certain arguments by appending ``=?`` to the argument name.
This feature is available in all shells and on all operating systems.

.. example::
   :title: Asking for the layer names of a vector dataset

   .. code-block:: bash

        gdal vector info my.gpkg --layer=?

   can return:

   .. code-block::

        ERROR 1: info: Potential values for argument 'layer' are:
        - towns
        - countries


.. example::
   :title: Asking for the allowed values of the resampling argument

   .. code-block:: bash

        gdal raster reproject --resampling=?

   returns:

   .. code-block::

        ERROR 1: reproject: Potential values for argument 'resampling' are:
        - nearest
        - bilinear
        - cubic
        - cubicspline
        - lanczos
        - average
        - rms
        - mode
        - min
        - max
        - med
        - q1
        - q3
        - sum
