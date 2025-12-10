.. _gdal_raster_reclassify:

================================================================================
``gdal raster reclassify``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reclassify a raster dataset.

.. Index:: gdal raster reclassify

Synopsis
--------

.. program-output:: gdal raster reclassify --help-doc

Description
-----------

:program:`gdal raster reclassify` reclassifies values in an input dataset.
A file (or string) specifies the mapping of input pixel values or ranges to output files.

An example file is shown below.

::

   # remap land cover types
   0       = 10      # land
   [2,4]   = 20      # freshwater
   1       = 40      # ocean
   NO_DATA = NO_DATA # leave NoData pixels unmodified

(The ``#`` character indicates a comment that is ignored by the parser but
can make the file easier to read.)
In this case:

- input values of 0 will be output as 10
- input values between 2 and 4 (inclusive) will be output as 20
- input values of 1 will be output as 40
- NoData values will be preserved as NoData

The presence of any other values in the input will cause an error.
If this is not desired, the input range ``DEFAULT`` can be used to specify an output
value for pixels not covered by any other input range.
These pixels may be converted unto NoData (``DEFAULT = NO_DATA``), some other constant value (e.g., ``DEFAULT = 50``), or left unmodified (``DEFAULT = PASS_THROUGH``).

.. only:: html

   .. figure:: ../../images/programs/gdal_raster_reclassify.svg

   Raster dataset before (left) and after (right) reclassification with :program:`gdal raster reclassify --mapping "[1,3]= 101; [4, 5)= 102; 7=102; NO_DATA=103; DEFAULT=NO_DATA"`.

.. note::

   :program:`gdal raster reclassify` supports writing to VRT format; however, VRT
   files generated in this way can only be opened using GDAL 3.11 or greater.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Program-Specific Options
------------------------

.. option:: -m, --mapping <MAPPING>

   A definition of mappings between input and output pixel values, as described above.
   The mappings may either be provided as text (with each entry separate by a semicolon),
   or they may be read from a file using ``@filename.txt``.

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

   .. code-block:: console

       $ gdal raster reclassify -m "0=10; [2,4]=20; 1=40" -i wbm.tif -o typ.tif
