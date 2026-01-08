.. _gdal_mdim_convert:

================================================================================
``gdal mdim convert``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a multidimensional dataset.

.. Index:: gdal mdim convert

Synopsis
--------

.. program-output:: gdal mdim convert --help-doc

Description
-----------

:program:`gdal mdim convert` can be used to convert multidimensional data between
different formats.

The following options are available:

Standard options
++++++++++++++++

.. option:: -f, --of, --format, --output-format <OUTPUT-FORMAT>

    Select the output format. This can be a format that supports multidimensional
    output (such as :ref:`raster.netcdf`, :ref:`vrt_multidimensional`), or a "classic" 2D formats, if only one single 2D array
    results of the other specified conversion operations. When this option is
    not specified, the format is guessed when possible from the extension of the
    destination filename.

.. option:: --co, --creation-option <KEY>=<VALUE>

    Many formats have one or more optional creation options that can be
    used to control particulars about the file created.

    The creation options available vary by format driver, and some
    simple formats have no creation options at all. A list of options
    supported for a format can be listed with the
    :ref:`--format <raster_common_options_format>`
    command line option but the documentation for the format is the
    definitive source of information on driver creation options.
    See :ref:`raster_drivers` format
    specific documentation for legal creation options for each format.

    Array-level creation options may be passed by prefixing them with ``ARRAY:``.
    See :cpp:func:`GDALGroup::CopyFrom` for further details regarding such options.

.. include:: gdal_options/overwrite.rst

.. option:: --array <array_spec>

    Instead of converting the whole dataset, select one array, and possibly
    perform operations on it. This option can be specified several times to
    operate on different arrays.

    <array_spec> may be just an array name, potentially using a fully qualified
    syntax (/group/subgroup/array_name). Or it can be a combination of options
    with the syntax:
    name={src_array_name}[,dstname={dst_array_name}][,resample=yes][,transpose=[{axis1},{axis2},...][,view={view_expr}]

    The following options are processed in that order:

    - ``resample=yes`` asks for the array to run through :cpp:func:`GDALMDArray::GetResampled`.

    - [{axis1},{axis2},...] is the argument of  :cpp:func:`GDALMDArray::Transpose`.
       For example, transpose=[1,0] switches the axis order of a 2D array.
       See :example:`mdim-convert-transpose`.

    - {view_expr} is the value of the *viewExpr* argument of :cpp:func:`GDALMDArray::GetView`.
      See :example:`mdim-convert-reorder`.

    When specifying a view_expr that performs a slicing or subsetting on a dimension, the
    equivalent operation will be applied to the corresponding indexing variable.

.. option:: --array-option <NAME>=<VALUE>

    Option passed to :cpp:func:`GDALGroup::GetMDArrayNames` to filter reported
    arrays. Such option is format specific. Consult driver documentation.
    This option may be used several times.

.. option:: --group <group_spec>

    Instead of converting the whole dataset, select one group, and possibly
    perform operations on it. This option can be specified several times to
    operate on different groups. If only one group is specified, its content will be
    copied directly to the target root group. If several ones are specified,
    they are copied under the target root group

    <group_spec> may be just a group name, potentially using a fully qualified
    syntax (/group/subgroup/subsubgroup_name). Or it can be a combination of options
    with the syntax:
    ``name={src_group_name}[,dstname={dst_group_name}][,recursive=no]``

.. option:: --subset <subset_spec>

    Performs a subsetting (trimming or slicing) operation along a dimension,
    provided that it is indexed by a 1D variable of numeric or string data type,
    and whose values are monotonically sorted.
    <subset_spec> follows exactly the `OGC WCS 2.0 KVP encoding <https://portal.opengeospatial.org/files/09-147r3>`__
    for subsetting.

    That is ``dim_name(min_val,max_val)`` or ``dim_name(sliced_val)``
    The first syntax will subset the dimension ``dim_name`` to values in the
    [min_val,max_val] range. The second syntax will slice the dimension dim_name
    to value sliced_val (and this dimension will be removed from the arrays
    that reference to it)

    Beware that dimension name is case sensitive.

    Several subsetting operations along different dimensions can be specified by repeating the option.

    Using --subset is incompatible with specifying a *view* option in --array.

    See :example:`mdim-convert-subset-1` or :example:`mdim-convert-subset-2`.

.. option:: --scale-axes <scaleaxes_spec>

    Applies a integral scale factor to one or several dimensions, that is
    extract 1 value every N values (without resampling).

    <scaleaxes_spec> follows exactly the syntax of the KVP encoding of the
    SCALEAXES parameter of
    `OGC WCS 2.0 Scaling Extension <https://portal.opengeospatial.org/files/12-039>`__,
    but limited to integer scale factors.

    That is ``<dim1_name>(<scale_factor>)``.

    Several scaling operations along different dimensions can be specified by repeating the option.

    Using --scale-axes is incompatible with specifying a *view* option in --array.

    See :example:`mdim-convert-subsample-1`.

.. option:: --strict

    By default, some failures during the translation are tolerated, such as not
    being able to write group attributes. When setting this option, such
    failures will cause the process to fail.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Convert a netCDF file to a multidimensional VRT file

   .. code-block:: bash

      gdal mdim convert in.nc out.vrt

.. example::
   :title: Extract a 2D slice of a time,Y,X array
   :id: mdim-convert-subset-1

   .. code-block:: bash

       gdal mdim convert in.nc out.tif --subset "time(\"2010-01-01\")" --array temperature

.. example::
   :title: Extract a a 3D chunk from a time-indexed GRIB file into a multiband GeoTIFF.
   :id: mdim-convert-subset-2

   .. code-block:: bash

       gdal mdim convert /vsicurl/https://tgftp.nws.noaa.gov/SL.us008001/ST.opnl/DF.gr2/DC.ndfd/AR.conus/VP.001-003/ds.qpf.bin tst.tif --subset "TIME(1763164800,1763251200)" --array QPF_0-SFC --co COMPRESS=DEFLATE

.. example::
   :title: Subsample along x and y axis
   :id: mdim-convert-subsample-1

   .. code-block:: bash

       gdal mdim convert in.nc out.nc --scale-axes "x(2),y(2)"

.. example::
   :title: Reorder the values of an array
   :id: mdim-convert-reorder

   Reorder the values of the time,Y,X array along the Y axis from top-to-bottom
   to bottom-to-top (or the reverse)

   .. code-block:: bash

      gdal mdim convert in.nc out.nc --array "name=temperature,view=[:,::-1,:]"

.. example::
   :title: Transpose an array that has X,Y,time dimension order to time,Y,X
   :id: mdim-convert-transpose


   .. code-block:: bash

       gdal mdim convert in.nc out.nc --array "name=temperature,transpose=[2,1,0]"
