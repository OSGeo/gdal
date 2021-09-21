.. _gdal_calc:

================================================================================
gdal_calc.py
================================================================================

.. only:: html

    Command line raster calculator with numpy syntax.

.. Index:: gdal_calc

Synopsis
--------

.. code-block::

    gdal_calc.py --calc=expression --outfile=out_filename [-A filename]
                 [--A_band=n] [-B...-Z filename] [other_options]

.. rubric::  DESCRIPTION
   :name: description

Command line raster calculator with numpy syntax. Use any basic
arithmetic supported by numpy arrays such as ``+``, ``-``, ``*``, and
``\`` along with logical operators such as ``>``.
Note that all files must have the same dimensions (unless extent option is used),
but no projection checking is performed (unless projectionCheck option is used).

.. option:: --help

    Show this help message and exit

.. option:: -h

    The same as :option:`--help`.

.. option:: --calc=expression

    Calculation in numpy syntax using ``+``, ``-``, ``/``, ``*``, or any numpy array functions (i.e. ``log10()``).
    Multiple ``--calc`` options can be listed to produce a multiband file (GDAL >= 3.2).

.. option:: -A <filename>

    Input gdal raster file, you can use any letter (a-z, A-Z).  (lower case supported since GDAL 3.3)

    A letter may be repeated (GDAL >= 3.3). The effect will be to create a 3-dim numpy array.
    In such a case, the calculation formula must use this input as a 3-dim array and must return a 2D array (see examples below).
    In case the calculation does not return a 2D array an error would be generated.

.. option:: --A_band=<n>

    Number of raster band for file A (default 1).

.. option::  --outfile=<filename>

    Output file to generate or fill.

.. option:: --NoDataValue=<value>

    Output NoDataValue (default datatype specific value).
    To indicate not setting a NoDataValue use --NoDataValue=none (GDAL >= 3.3)

    .. note::
        Using the Python API:
        ``None`` value will indicate default datatype specific value.
        ``'none'`` value will indicate not setting a NoDataValue.

.. option:: --hideNoData

    .. versionadded:: 3.3

    Ignores the input bands NoDataValue.
    By default, the input bands NoDataValue are not participating in the calculation.
    By setting this setting - no special treatment will be performed on the input NoDataValue. and they will be participating in the calculation as any other value.
    The output will not have a set NoDataValue, unless you explicitly specified a specific value by setting --NoDataValue=<value>.

.. option:: --type=<datatype>

    Output datatype, must be one of [``Int32``, ``Int16``, ``Float64``, ``UInt16``, ``Byte``, ``UInt32``, ``Float32``].

    .. note::

       Despite the datatype set using ``--type``, when doing intermediate aritmethic operations using operands of the
       same type, the operation result will honor the original datatype. This may lead into unexpected results in the final result.

.. option:: --format=<gdal_format>

    GDAL format for output file.

.. option:: color-table=<filename>

    Allows specifying a filename of a color table (or a ColorTable object) (with Palette Index interpretation) to be used for the output raster.
    Supported formats: txt (i.e. like gdaldem, but color names are not supported), qlr, qml (i.e. exported from QGIS)

.. option:: --extent=<option>

    .. versionadded:: 3.3

    this option determines how to handle rasters with different extents.
    this option is mutually exclusive with the `projwin` option, which is used for providing a custom extent.
    for all the options below the pixel size (resolution) and SRS (Spatial Reference System) of all the input rasters must be the same.
    ``ignore`` (default) - only the dimensions of the rasters are compared. if the dimensions do not agree the operation will fail.
    ``fail`` - the dimensions and the extent (bounds) of the rasters must agree, otherwise the operation will fail.
    ``union`` - the extent (bounds) of the output will be the minimal rectangle that contains all the input extents.
    ``intersect`` - the extent (bounds) of the output will be the maximal rectangle that is contained in all the input extents.

.. option:: --projwin <ulx> <uly> <lrx> <lry>

    .. versionadded:: 3.3

    this option provides a custom extent for the output, it is mutually exclusive with the `extent` option.

.. option:: --projectionCheck

    .. versionadded:: 3.3

    By default, no projection checking will be performed.
    By setting this option, if the projection is not the same for all bands then the operation will fail.

.. _creation-option:

.. option:: --creation-option=<option>

    Passes a creation option to the output format driver.  Multiple
    options may be listed. See format specific documentation for legal
    creation options for each format.

.. option:: --co=<option>

        The same as creation-option_.

.. option:: --allBands=[a-z, A-Z]

    Process all bands of given raster (a-z, A-Z). Requires a single calc for all bands.

.. option:: --overwrite

    Overwrite output file if it already exists.

.. option:: --debug

    Print debugging information.

.. option:: --quiet

    Suppress progress messages.


Python options
--------------

.. versionadded:: 3.3

The following options are available by using function the python interface of gdal_calc.
They are not available using the command prompt.

.. option:: user_namespace

    A dictionary of custom functions or other names to be available for use in the Calc expression.

.. option:: return_ds

    If enabled, the output dataset would be returned from the function and not closed.

.. option:: color_table

    Allows specifying a ColorTable object (with Palette Index interpretation) to be used for the output raster.

Example
-------

Add two files together:

.. code-block::

    gdal_calc.py -A input1.tif -B input2.tif --outfile=result.tif --calc="A+B"

Average of two layers:

.. code-block::

    gdal_calc.py -A input1.tif -B input2.tif --outfile=result.tif --calc="(A+B)/2"

.. note::

   In the previous example, beware that if A and B inputs are of the same datatype, for example integers, you
   may need to force the conversion of one of the operands before the division operation.

   .. code-block::

      gdal_calc.py -A input.tif -B input2.tif --outfile=result.tif --calc="(A.astype(numpy.float64) + B) / 2"

Add three files together (two options with the same result):

.. code-block::

    gdal_calc.py -A input1.tif -B input2.tif -C input3.tif --outfile=result.tif --calc="A+B+C"

.. versionadded:: 3.3

.. code-block::

    gdal_calc.py -A input1.tif -A input2.tif -A input3.tif --outfile=result.tif --calc="numpy.sum(A,axis=0)".

Average of three layers (two options with the same result):

.. code-block::

    gdal_calc.py -A input1.tif -B input2.tif -C input3.tif --outfile=result.tif --calc="(A+B+C)/3"

.. versionadded:: 3.3

.. code-block::

    gdal_calc.py -A input1.tif -A input2.tif -A input3.tif --outfile=result.tif --calc="numpy.average(a,axis=0)".

Maximum of three layers  (two options with the same result):

.. code-block::

    gdal_calc.py -A input1.tif -B input2.tif -C input3.tif --outfile=result.tif --calc="numpy.max((A,B,C),axis=0)"

.. versionadded:: 3.3

.. code-block::

    gdal_calc.py -A input1.tif -A input2.tif -A input3.tif --outfile=result.tif --calc="numpy.max(A,axis=0)"

Set values of zero and below to null:

.. code-block::

    gdal_calc.py -A input.tif --outfile=result.tif --calc="A*(A>0)" --NoDataValue=0

Using logical operator to keep a range of values from input:

.. code-block::

    gdal_calc.py -A input.tif --outfile=result.tif --calc="A*logical_and(A>100,A<150)"

Work with multiple bands:

.. code-block::

    gdal_calc.py -A input.tif --A_band=1 -B input.tif --B_band=2 --outfile=result.tif --calc="(A+B)/2" --calc="B*logical_and(A>100,A<150)"
