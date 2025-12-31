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

Starting with GDAL 3.12, it is also possible to use a :ref:`VRT C++ pixel function <builtin_pixel_functions>`
(such a ``sum``, ``mean``, ``min``, ``max``) by specifying :option:`--dialect` to
``builtin``.

The inputs should have the same spatial reference system and should cover the same spatial extent but are not required to have the same
spatial resolution. The spatial extent check can be disabled with :option:`--no-check-extent`,
in which case the inputs must have the same dimensions. The spatial reference system check can be
disabled with :option:`--no-check-crs`.

Since GDAL 3.12, this algorithm can be part of a :ref:`gdal_pipeline` or :ref:`gdal_raster_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Program-Specific Options
------------------------

.. option:: --calc

    An expression to be evaluated using the `muparser <https://beltoforion.de/en/muparser>`__ math parser library.
    The expression may refer to individual bands of each input (e.g., ``X[1] + 3``) or it may be applied to all bands
    of an input (``X + 3``).

    There are two methods by which an expression may be applied to multiple bands. In the default method, the expression is
    applied to each band individually, resulting in one output band for each input band. For example, with a three-band
    input ``X``, the expression ``--calc "X+3"`` would be expanded into ``--calc "X[1]+3" --calc "X[2]+3" --calc "X[3]+3"``.
    If the expression contains a reference to all bands of multiple inputs, those inputs must either have the same
    number of bands, or a single band. For example, if inputs ``A`` and ``B`` each have three bands, and input ``C`` has
    a single band, then the argument ``--calc "A + B + C"`` is equivalent to
    ``--calc "A[1] + B[1] + C[1]" --calc "A[2] + B[2] + C[1]" --calc "A[3] + B[3] + C[1]"``.
    Similarly, ``--calc "sum(A,B,C)"`` would be expanded to
    ``--calc "sum(A[1], B[1], C[1]" --calc "sum(A[2], B[2], C[1])" --calc "sum(A[3], B[3], C[1])"``.

    In the second method, which is enabled with :option:`--flatten`, aggregate functions (``sum``, ``avg``, ``min``, and ``max``)
    invoked on multiple bands will be applied to all bands in aggregate. In this case, ``sum(A)`` is expanded into ``sum(A[1], A[2], A[3])``,
    and ``sum(A,C)`` is expanded into ``sum(A[1], A[2], A[3], C[1])``. If the expression consists _only_ of aggregate functions,
    this will produce a single output band no matter how many input bands are present. However, if the expression contains
    a non-aggregate reference to all bands of an input, then one output band will still be produced for each input band. A simple
    example is the expression ``A / sum(A)``, which would produce an N-band raster where each output band contains the input band's
    fraction of the total.

    Multiple calculations may be specified; output band(s) will be produced for each expression in the order they
    are provided.

    Input rasters will be converted to 64-bit floating point numbers before performing calculations.

    Starting with GDAL 3.12, it is also possible to use a :ref:`VRT C++ pixel function <builtin_pixel_functions>`
    (such a ``sum``, ``mean``, ``min``, ``max``) by specifying :option:`--dialect` to
    ``builtin``.
    Arguments to functions are passed within parentheses, like ``sum(k=1)``,
    ``min(propagateNoData=true)`` or ``interpolate_linear(t0=1,dt=1,t=10)``.

    Built-in variables
    ^^^^^^^^^^^^^^^^^^

    The following built-in variables are available in expressions:

    - ``_CENTER_X_``: X coordinate of the pixel center, expressed in the dataset CRS
    - ``_CENTER_Y_``: Y coordinate of the pixel center, expressed in the dataset CRS
    - ``NODATA``: The output `NoData` value, if any.

    These variables are useful for calculations that depend on spatial position,
    such as latitude-based corrections, solar angle approximations, or zonal masks.

    See :example:`raster-calc-center-y` for a usage example.
 
    Note: To work with longitude/latitude values, the input dataset must be in a
    geographic coordinate reference system (for example, EPSG:4326).

.. option:: --dialect muparser|builtin

    .. versionadded:: 3.12

    Specify the expression engine used to interpret the value of :option:`--calc`.
    The default is ``muparser``.

.. option:: --flatten

    .. versionadded:: 3.12

    Generate a single band output raster per expression, even if input datasets are multiband.

    For the default muparser dialect, using the name of an input raster (let's say A)
    is equivalent to ``A[1],A[2],...,A[n]``. Such syntax works with the
    ``min``, ``max``, ``sum`` and ``avg`` muparser functions.

    For example, let's consider 2 rasters A and B with 3bands each, then
    ``--flatten --calc "avg(A)-avg(B)"`` is equivalent to ``--calc "avg(A[1],A[2],A[3])-avg(B[1],B[2],B[3])"``

    And ``--flatten --dialect=builtin --calc mean`` will compute a single band
    with the average of all 4 input bands.

.. option:: -i [<name>=]<input>

    Select an input dataset to be processed. If more than one input dataset is provided,
    each dataset must be prefixed with a name to which it will will be referenced in :option:`--calc`.

.. option:: --no-check-extent

    Do not verify that the input rasters have the same spatial extent. The input rasters will instead be required to
    have the same dimensions. The geotransform of the first input will be assigned to the output.

.. option:: --no-check-crs

    Do not check the spatial reference systems of the inputs for consistency. All inputs will be assumed to have the
    spatial reference system of the first input, and this spatial reference system will be used for the output.

.. option:: --nodata

    .. versionadded:: 3.12

    Set the NoData value for the output dataset. May be set to "none" to leave the NoData value undefined. If
    :option:`--nodata` is not specified, :program:`gdal raster calc` will use a NoData value from the first
    source dataset to have one.

.. option:: --propagate-nodata

    .. versionadded:: 3.12

    If set, a NoData value in any input dataset used an in expression will cause the output value to be NoData.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/ot.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Per-band sum of three files
   :id: simple-sum

   .. code-block:: bash

       gdal raster calc -i "A=file1.tif" -i "B=file2.tif" -i "C=file3.tif" --calc "A+B+C" -o out.tif

.. example::
   :title: Per-dataset average of all bands of each dataset
   :id: simple-avg

   .. code-block:: bash

       gdal raster calc -i "A=file1.tif" -i "B=file2.tif" -i "C=file3.tif" --flatten --calc "avg(A)" --calc "avg(B)" --calc "avg(C)" -o out.tif


.. example::
   :title: Per-band maximum of three files
   :id: simple-max

   .. code-block:: bash

       gdal raster calc -i "A=file1.tif" -i "B=file2.tif" -i "C=file3.tif" --calc "max(A,B,C)" -o out.tif


.. example::
   :title: Setting values of zero and below to NaN

   .. code-block:: bash

       gdal raster calc -i "A=input.tif" -o result.tif --calc="A > 0 ? A : NaN"


.. example::
   :title: Compute the average (as a single band) of all bands of two input datasets

   .. code-block:: bash

       gdal raster calc -i A=input1.tif -i B=input2.tif -o result.tif --flatten --calc=mean --dialect=builtin


.. example::
   :title: Generate a masked aspect layer where the slope angle is greater than 2 degrees, using nested pipelines (since GDAL 3.12.1)

   .. code-block:: bash

       gdal raster calc -i "SLOPE=[ read dem.tif ! slope ]" -i "ASPECT=[ read dem.tif ! aspect ]" -o result.tif --calc "(SLOPE >= 2) ? ASPECT : -9999" --nodata -9999


.. example::
   :title: Latitude-based calculation using ``_CENTER_Y_``
   :id: raster-calc-center-y

   .. code-block:: bash

       gdal raster calc -i A=input.tif \
           --calc="sin(_CENTER_Y_ * 0.0174533)" \
           -o output.tif

