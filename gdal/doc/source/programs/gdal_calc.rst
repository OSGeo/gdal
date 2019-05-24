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
``\`` along with logical operators such as ``>``. Note that all files
must have the same dimensions, but no projection checking is
performed.

.. option:: --help

    Show this help message and exit

.. option:: -h

    The same as :option:`--help`.

.. option:: --calc=expression

    Calculation in gdalnumeric syntax using ``+``, ``-``, ``/``, ``*``, or any numpy array functions (i.e. ``log10()``).

.. option:: -A <filename>

    Input gdal raster file, you can use any letter (A-Z).

.. option:: --A_band=<n>

    Number of raster band for file A (default 1).

.. option::  --outfile=<filename>

    Output file to generate or fill.

.. option:: --NoDataValue=<value>

    Output nodata value (default datatype specific value).

.. option:: --type=<datatype>

    Output datatype, must be one of [``Int32``, ``Int16``, ``Float64``, ``UInt16``, ``Byte``, ``UInt32``, ``Float32``].
    
.. option:: --format=<gdal_format>

    GDAL format for output file.


.. _creation-option:

.. option:: --creation-option=<option>

    Passes a creation option to the output format driver.  Multiple
    options may be listed. See format specific documentation for legal
    creation options for each format.

.. option:: --co=<option>

        The same as creation-option_.

.. option:: --allBands=[A-Z]

    Process all bands of given raster (A-Z).

.. option:: --overwrite

    Overwrite output file if it already exists.

.. option:: --debug

    Print debugging information.

.. option:: --quiet

    Suppress progress messages.

Example
-------

Add two files together:

.. code-block::

    gdal_calc.py -A input1.tif -B input2.tif --outfile=result.tif --calc="A+B"

Average of two layers:

.. code-block::

    gdal_calc.py -A input.tif -B input2.tif --outfile=result.tif --calc="(A+B)/2"

Set values of zero and below to null:

.. code-block::

    gdal_calc.py -A input.tif --outfile=result.tif --calc="A*(A>0)" --NoDataValue=0
