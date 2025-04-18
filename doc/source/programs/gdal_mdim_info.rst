.. _gdal_mdim_info:

================================================================================
``gdal mdim info``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Get information on a multidimensional dataset.

.. Index:: gdal mdim info

Synopsis
--------

.. program-output:: gdal mdim info --help-doc

Description
-----------

:program:`gdal mdim info` lists various information about a GDAL supported
multidimensional dataset.

The following items will be reported (when known) as JSON:

-  The format driver used to access the file.
-  Hierarchy of groups
-  Group attributes
-  Arrays:
    * Name
    * Dimension name, sizes, indexing variable
    * Data type
    * Attributes
    * SRS
    * Nodata value
    * Units
    * Statistics (if requested)

The following options are available:

Standard options
++++++++++++++++

.. option:: --detailed

    Most verbose output. Report attribute data types and array values.

.. option:: --array <array_name>

    Name of the array used to restrict the output to the specified array.

    .. note::

        Bash completion for :option:`--array` is possible if the dataset name
        is specified before.

.. option:: --limit <number>

    Number of values in each dimension that is used to limit the display of
    array values. By default, unlimited. Only taken into account if used with
    -detailed.

.. option:: --array-option <NAME>=<VALUE>

    Option passed to :cpp:func:`GDALGroup::GetMDArrayNames` to filter reported
    arrays. Such option is format specific. Consult driver documentation.
    This option may be used several times.

    .. note::

        Bash completion for :option:`--array-option` is possible if the dataset name
        is specified before.

.. option:: --stats

    Read and display array statistics. Force computation if no
    statistics are stored in an array.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Getting information on the file :file:`netcdf-4d.nc` as JSON output

   .. code-block:: console

       $ gdal mdim info netcdf-4d.nc

   .. code-block:: json

     {
       "type": "group",
       "name": "/",
       "attributes": {
         "Conventions": "CF-1.5"
       },
       "dimensions": [
         {
           "name": "levelist",
           "full_name": "/levelist",
           "size": 2,
           "type": "VERTICAL",
           "indexing_variable": "/levelist"
         },
         {
           "name": "longitude",
           "full_name": "/longitude",
           "size": 10,
           "type": "HORIZONTAL_X",
           "direction": "EAST",
           "indexing_variable": "/longitude"
         },
         {
           "name": "latitude",
           "full_name": "/latitude",
           "size": 10,
           "type": "HORIZONTAL_Y",
           "direction": "NORTH",
           "indexing_variable": "/latitude"
         },
         {
           "name": "time",
           "full_name": "/time",
             "size": 4,
           "type": "TEMPORAL",
           "indexing_variable": "/time"
           }
       ],
       "arrays": {
         "levelist": {
           "datatype": "Int32",
           "dimensions": [
               "/levelist"
             ],
           "attributes": {
             "long_name": "pressure_level"
           },
           "unit": "millibars"
         },
         "longitude": {
           "datatype": "Float32",
           "dimensions": [
             "/longitude"
           ],
           "attributes": {
             "standard_name": "longitude",
             "long_name": "longitude",
             "axis": "X"
           },
           "unit": "degrees_east"
         },
         "latitude": {
           "datatype": "Float32",
           "dimensions": [
             "/latitude"
           ],
           "attributes": {
             "standard_name": "latitude",
             "long_name": "latitude",
             "axis": "Y"
           },
           "unit": "degrees_north"
         },
         "time": {
           "datatype": "Float64",
           "dimensions": [
             "/time"
           ],
           "attributes": {
             "standard_name": "time",
             "calendar": "standard"
           },
           "unit": "hours since 1900-01-01 00:00:00"
         },
         "t": {
           "datatype": "Int32",
           "dimensions": [
             "/time",
             "/levelist",
             "/latitude",
             "/longitude"
           ],
           "nodata_value": -32767
         }
       },
       "structural_info": {
         "NC_FORMAT": "CLASSIC"
       }
     }
