.. _gdal_calc:

================================================================================
gdal_calc
================================================================================

.. only:: html

    Command line raster calculator with numpy syntax.

.. Index:: gdal_calc

Synopsis
--------

.. code-block::

    gdal_calc [--help] [--help-general]
                 --calc=expression --outfile=<out_filename> [-A <filename>]
                 [--A_band=<n>] [-B...-Z <filename>] [<other_options>]

.. rubric::  DESCRIPTION
   :name: description

Command line raster calculator with numpy syntax. Use any basic
arithmetic supported by numpy arrays such as ``+``, ``-``, ``*``, and
``/`` along with logical operators such as ``>``.
Note that all files must have the same dimensions (unless the :option:`--extent` option is used),
but no projection checking is performed (unless the :option:`--projectionCheck` option is used).

.. note::

    gdal_calc is a Python utility, and is only available if GDAL Python bindings are available.

.. tip:: Equivalent in new "gdal" command line interface:

    See :ref:`gdal_raster_calc`.

.. include:: options/help_and_help_general.rst

.. option:: --calc=<expression>

    Calculation in numpy syntax using ``+``, ``-``, ``/``, ``*``, or any numpy array functions (i.e. ``log10()``).
    Multiple :option:`--calc` options can be listed to produce a multiband file (GDAL >= 3.2).
    See :example:`gdal-calc-multiple-expr`.

.. option:: -A <filename>

    Input GDAL raster file, you can use any letter (a-z, A-Z).  (lower case supported since GDAL 3.3)

    A letter may be repeated, or several values (separated by space) can be provided (GDAL >= 3.3).
    The effect will be to create a 3D numpy array.
    Since GDAL 3.5, wildcard exceptions (using ?, \*) are supported for all shells/platforms.
    In such a case, the calculation formula must use this input as a 3D array and must return a 2D array (see :example:`gdal-calc-3d-sum`).
    If the calculation does not return a 2D array an error will be generated.

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

    Output datatype, must be one of [``Byte``, ``Int8``, ``UInt16``, ``Int16``, ``UInt32``, ``Int32``, ``UInt64``, ``Int64``, ``Float64``, ``Float32``, ``CInt16``, ``CInt32``, ``CFloat64``, ``CFloat32``].

    .. note::

       Despite the datatype set using ``--type``, when doing intermediate arithmetic operations using operands of the
       same type, the operation result will honor the original datatype. This may lead into unexpected results in the final result.

    .. note::

        UInt64, Int64, CInt16, CInt32, CFloat32, CFloat64 have been added in GDAL 3.5.3
        Int8 has been added in GDAL 3.7

.. option:: --format=<gdal_format>

    GDAL format for output file.

.. option:: --color-table=<filename>

    Allows specifying a filename of a color table (or a ColorTable object) (with Palette Index interpretation) to be used for the output raster.
    Supported formats: txt (i.e. like gdaldem, but color names are not supported), qlr, qml (i.e. exported from QGIS)

.. option:: --extent=<option>

    .. versionadded:: 3.3

    This option determines how to handle rasters with different extents.
    This option is mutually exclusive with the `projwin` option, which is used for providing a custom extent.

    For all the options below the pixel size (resolution) and SRS (Spatial Reference System) of all the input rasters must be the same.

    ``ignore`` (default) - only the dimensions of the rasters are compared. if the dimensions do not agree the operation will fail.

    ``fail`` - the dimensions and the extent (bounds) of the rasters must agree, otherwise the operation will fail.

    ``union`` - the extent (bounds) of the output will be the minimal rectangle that contains all the input extents.

    ``intersect`` - the extent (bounds) of the output will be the maximal rectangle that is contained in all the input extents.

.. option:: --projwin <ulx> <uly> <lrx> <lry>

    .. versionadded:: 3.3

    This option provides a custom extent for the output, it is mutually exclusive with the `extent` option.

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

    Apply the expression to all bands of a given raster. When
    :option:`--allBands` is used, :option:`--calc` may be specified only once.
    See :example:`gdal-calc-allbands-1` and :example:`gdal-calc-allbands-2`.

.. option:: --overwrite

    Overwrite output file if it already exists. Overwriting must be understood
    here as deleting and recreating the file from scratch. Note that if this option
    is *not* specified and the output file already exists, it will be updated in
    place.

.. option:: --debug

    Print debugging information.

.. option:: --quiet

    Suppress progress messages.


Python options
--------------

.. versionadded:: 3.3

The following options are available by using the Python interface of :program:`gdal_calc`.
They are not available using the command prompt.

.. option:: user_namespace

    A dictionary of custom functions or other names to be available for use in the Calc expression.

.. option:: return_ds

    If enabled, the output dataset will be returned from the function and not closed.

.. option:: color_table

    Allows specifying a ColorTable object (with Palette Index interpretation) to be used for the output raster.

Examples
--------

.. example::
   :title: Average of two files

   .. code-block:: bash

    gdal_calc -A input1.tif -B input2.tif --outfile=result.tif --calc="(A+B)/2"

   .. caution::

      If A and B inputs both have integer data types, integer division will be
      performed. To avoid this, you can convert one of the operands to a
      floating point type before the division operation.

      .. code-block:: bash

         gdal_calc -A input.tif -B input2.tif --outfile=result.tif --calc="(A.astype(numpy.float64) + B) / 2"

.. example::
   :title: Summing three files

   .. code-block:: bash

       gdal_calc -A input1.tif -B input2.tif -C input3.tif --outfile=result.tif --calc="A+B+C"

.. example::
   :title: Combining three files into a 3D array and summing
   :id: gdal-calc-3d-sum

   .. code-block:: bash

       gdal_calc -A input1.tif -A input2.tif -A input3.tif --outfile=result.tif --calc="numpy.sum(A,axis=0)".

.. example::
   :title: Average of three files

   .. code-block:: bash

       gdal_calc -A input1.tif -B input2.tif -C input3.tif --outfile=result.tif --calc="(A+B+C)/3"

.. example::
   :title: Average of three files, using 3D array

   .. code-block:: bash

       gdal_calc -A input1.tif input2.tif input3.tif --outfile=result.tif --calc="numpy.average(a,axis=0)".

.. example::
   :title: Maximum of three files

   .. code-block:: bash

       gdal_calc -A input1.tif -B input2.tif -C input3.tif --outfile=result.tif --calc="numpy.max((A,B,C),axis=0)"

.. example::
   :title: Maximum of three files, using a 3D array

   .. code-block:: bash

       gdal_calc -A input1.tif input2.tif input3.tif --outfile=result.tif --calc="numpy.max(A,axis=0)"

.. example::
   :title: Setting values of zero and below to NODATA

   .. code-block:: bash

       gdal_calc -A input.tif --outfile=result.tif --calc="A*(A>0)" --NoDataValue=0

.. example::
   :title: Using logical operator to keep a range of values from input

   .. code-block:: bash

       gdal_calc -A input.tif --outfile=result.tif --calc="A*logical_and(A>100,A<150)"

.. example::
   :title: Performing two calculations and storing results in separate bands
   :id: gdal-calc-multiple-expr

   .. code-block:: bash

       gdal_calc -A input.tif --A_band=1 -B input.tif --B_band=2 \
         --outfile=result.tif --calc="(A+B)/2" --calc="B*logical_and(A>100,A<150)"

.. example::
   :title: Add a raster to each band in a 3-band raster
   :id: gdal-calc-allbands-1

   .. code-block:: bash

       gdal_calc -A 3band.tif -B 1band.tif --outfile result.tif --calc "A+B" --allBands A

   The result will have three bands, where each band contains the values of ``1band.tif``
   added to the corresponding band in ``3band.tif``.

.. example::
   :title: Add two three-band rasters
   :id: gdal-calc-allbands-2

   .. code-block:: bash

       gdal_calc -A 3band_a.tif -B 3band_b.tif --outfile result.tif --calc "A+B" --allBands A --allBands B

   The result will have three bands, where each band contains the values of the corresponding
   band of ``3band_a.tif`` added to the corresponding band of ``3band_b.tif``.


