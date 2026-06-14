.. _gdal_mdim_info:

.. program:: gdal_mdim_info

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
multidimensional dataset, and returns them on the standard output stream when used from the
command line, or in the ``output`` parameter when used from the API.

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

Starting with GDAL 3.14, :program:`gdal mdim info` can be used as the last
step of a :ref:`gdal_mdim_pipeline`.

The following options are available:

Program-Specific Options
------------------------

.. option:: -f, --of, --format, --output-format json|text

    .. versionadded:: 3.14

    Which output format to use. Default is JSON, unless the program is invoked
    from command line, in which case it is text.

.. option:: --summary

    .. versionadded:: 3.13

    Report only group and array hierarchy, without detailed information on attributes or dimensions.
    Mutually exclusive with :option:`--detailed`.

.. option:: --detailed

    Most verbose output. Report attribute data types and array values.
    Mutually exclusive with :option:`--summary`.

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

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Getting information on the file :file:`netcdf-4d.nc` as text output

   .. code-block:: console

       $ gdal mdim info netcdf-4d.nc

   .. code-block::

        Driver: netCDF

        Structural metadata:
          NC_FORMAT  CLASSIC

        Dimensions:
          Name (path)  Size      Type      Direction
          -----------  ----  ------------  ---------
          /levelist       2  VERTICAL
          /longitude     10  HORIZONTAL_X  EAST
          /latitude      10  HORIZONTAL_Y  NORTH
          /time           4  TEMPORAL

        Coordinates (indexing variables):
          Name (path)   Dimension    Type                 Unit
          -----------  -----------  -------  -------------------------------
          /levelist    (levelist)   Int32    millibars
          /longitude   (longitude)  Float32  degrees_east
          /latitude    (latitude)   Float32  degrees_north
          /time        (time)       Float64  hours since 1900-01-01 00:00:00

        Data variables:
          Name (path)  Type   Unit      Shape       Chunk size
          -----------  -----  ----  --------------  ----------

         (/time, /levelist, /latitude, /longitude):
          /t           Int32        [4, 2, 10, 10]  (unknown)

        Attributes:
             Name       Type    Value
          -----------  ------  --------
          Conventions  String  "CF-1.5"

        Arrays:

          - /levelist:
              Dimensions:  (/levelist)
              Shape:       [2]
              Type:        Int32
              Unit:        millibars

              Attributes:
                  Name      Type        Value
                ---------  ------  ----------------
                long_name  String  "pressure_level"

          - /longitude:
              Dimensions:  (/longitude)
              Shape:       [10]
              Type:        Float32
              Unit:        degrees_east

              Attributes:
                    Name        Type      Value
                -------------  ------  -----------
                standard_name  String  "longitude"
                long_name      String  "longitude"
                axis           String  "X"

          - /latitude:
              Dimensions:  (/latitude)
              Shape:       [10]
              Type:        Float32
              Unit:        degrees_north

              Attributes:
                    Name        Type     Value
                -------------  ------  ----------
                standard_name  String  "latitude"
                long_name      String  "latitude"
                axis           String  "Y"

          - /time:
              Dimensions:  (/time)
              Shape:       [4]
              Type:        Float64
              Unit:        hours since 1900-01-01 00:00:00

              Attributes:
                    Name        Type     Value
                -------------  ------  ----------
                standard_name  String  "time"
                calendar       String  "standard"

          - /t:
              Dimensions:    (/time, /levelist, /latitude, /longitude)
              Shape:         [4, 2, 10, 10]
              Type:          Int32
              Nodata value:  -32767

.. example::
   :title: Getting information on the file :file:`netcdf-4d.nc` as JSON output

   .. code-block:: console

       $ gdal mdim info --format json netcdf-4d.nc

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
