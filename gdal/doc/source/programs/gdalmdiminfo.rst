.. _gdalmdiminfo:

================================================================================
gdalmdiminfo
================================================================================

.. only:: html

    .. versionadded:: 3.1

    Reports structure and content of a multidimensional dataset.

.. Index:: gdalmdiminfo

Synopsis
--------

.. code-block::

    gdalmdiminfo [--help-general] [-oo NAME=VALUE]* [-arrayoption NAME=VALUE]*
                 [-detailed] [-nopretty] [-array {array_name}] [-limit {number}]
                 [-stats] <datasetname>

Description
-----------

:program:`gdalmdiminfo` program lists various information about a GDAL supported
multidimensional raster dataset as JSON output. It follows the
following `JSON schema <https://github.com/OSGeo/gdal/blob/master/gdal/data/gdalmdiminfo_output.schema.json>`_

The following command line parameters can appear in any order

.. program:: gdalmdiminfo

.. option:: -detailed

    Most verbose output. Report attribute data types and array values.

.. option:: -nopretty

    Outputs on a single line without any indentation.

.. option:: -array {array_name}

    Name of the array used to restrict the output to the specified array.

.. option:: -limit {number}

    Number of values in each dimension that is used to limit the display of
    array values. By default, unlimited. Only taken into account if used with
    -detailed.

.. option:: -oo <NAME=VALUE>

    Dataset open option (format specific).
    This option may be used several times.

.. option:: -arrayoption <NAME=VALUE>

    Option passed to :cpp:func:`GDALGroup::GetMDArrayNames` to filter reported
    arrays. Such option is format specific. Consult driver documentation.
    This option may be used several times.

.. option:: -stats

    Read and display image statistics. Force computation if no
    statistics are stored in an image.

    .. versionadded:: 3.2

C API
-----

This utility is also callable from C with :cpp:func:`GDALMultiDimInfo`.

Examples
--------

- Display general structure1

.. code-block::

    $ gdalmdiminfo netcdf-4d.nc 


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

- Display detailed information about a given array

.. code-block::

    $ gdalmdiminfo netcdf-4d.nc -array t -detailed -limit 3
