.. _gdal_raster_calc:

================================================================================
``gdal raster calc``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Perform raster algebra

.. Index:: gdal raster calc

Synopsis
--------

.. program-output:: gdal raster calc --help-doc

Description
-----------

:program:`gdal raster calc` performs pixel-wise calculations on one or more input GDAL datasets. Calculations
can be performed eagerly, writing results to a conventional raster format,
or lazily, written as a set of derived bands in a :ref:`VRT (Virtual Dataset) <raster.vrt>`.

The list of input GDAL datasets can be specified at the end
of the command line or put in a text file (one input per line) for very long lists.
If more than one input dataset is used, it should be prefixed with a name by which it
will be referenced in the calculation, e.g. ``A=my_data.tif``. (If a single dataset is
used, it will be referred to with the variable ``X`` in formulas.)

The inputs should have the same spatial reference system and should cover the same spatial extent but are not required to have the same
spatial resolution. The spatial extent check can be disabled with :option:`--no-check-extent`,
in which case the inputs must have the same dimensions. The spatial reference system check can be
disabled with :option:`--no-check-srs`.

The following options are available:

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: -i [<name>=]<input>

    Select an input dataset to be processed. If more than one input dataset is provided,
    each dataset must be prefixed with a name to which it will will be referenced in :option:`--calc`.

.. option:: --calc

    An expression to be evaluated using the `muparser <https://beltoforion.de/en/muparser>`__ math parser library.
    The expression may refer to individual bands of each input (e.g., ``X[1] + 3``) or it may be applied to all bands
    of an input (``X + 3``). If the expression contains a reference to all bands of multiple inputs, those inputs
    must either have the same the number of bands, or a single band.

    For example, if inputs ``A`` and ``B`` each have three bands, and input ``C`` has a single band, then the argument
    ``--calc "A + B + C"`` is equivalent to ``--calc "A[1] + B[1] + C[1]" --calc "A[2] + B[2] + C[1]" --calc "A[3] + B[3] + C[1]"``.

    Multiple calculations may be specified; output band(s) will be produced for each expression in the order they
    are provided.

    Input rasters will be converted to 64-bit floating point numbers before performing calculations.

.. option:: --no-check-extent

    Do not verify that the input rasters have the same spatial extent. The input rasters will instead be required to
    have the same dimensions. The geotransform of the first input will be assigned to the output.

.. option:: --no-check-srs

    Do not check the spatial reference systems of the inputs for consistency. All inputs will be assumed to have the
    spatial reference system of the first input, and this spatial reference system will be used for the output.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Examples
--------

.. example::
   :title: Per-band sum of three files
   :id: simple-sum

   .. code-block:: bash

       gdal raster calc -i "A=file1.tif" -i "B=file2.tif" -i "C=file3.tif" --calc "A+B+C" -o out.tif


.. example::
   :title: Per-band maximum of three files
   :id: simple-max

   .. code-block:: bash

       gdal raster calc -i "A=file1.tif" -i "B=file2.tif" -i "C=file3.tif" --calc "max(A,B,C)" -o out.tif


.. example::
   :title: Setting values of zero and below to NaN

   .. code-block:: bash

       gdal raster calc -i "A=input.tif" -o result.tif --calc="A > 0 ? A : NaN"
