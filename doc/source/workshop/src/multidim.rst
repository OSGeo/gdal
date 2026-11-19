.. _multidim:

================================================================================
GDAL multidimensional tools
================================================================================

Downloading and extracting information using GDAL multidimensional API
----------------------------------------------------------------------

Using :ref:`gdal mdim convert <gdal_mdim_convert>`

Extracting NOAA Global Forecast System (GFS) data available from https://registry.opendata.aws/noaa-gfs-bdp-pds/

::

    $ gdal mdim convert "/vsis3/noaa-gfs-bdp-pds/gdas.20260519/00/atmos/gdas.t00z.sfcanl.nc" --config AWS_NO_SIGN_REQUEST=YES --array /tmp2m --output 20260519_00_tmp2m.nc

    $ gdal mdim convert "/vsis3/noaa-gfs-bdp-pds/gdas.20260519/06/atmos/gdas.t06z.sfcanl.nc" --config AWS_NO_SIGN_REQUEST=YES --array /tmp2m --output 20260519_06_tmp2m.nc

.. warning::

  The above may only work properly on Linux due to limitations in the netCDF driver
  regarding working with files in ``/vsi`` virtual file systems, hence the
  converted files :file:`20260519_00_tmp2m.nc` and  :file:`20260519_06_tmp2m.nc`
  are provided in the input datasets.


Inspecting
----------

Using :ref:`gdal mdim info <gdal_mdim_info>`

Let's have a look:

::

    $ gdal mdim info 20260519_00_tmp2m.nc

.. code-block:: json

    "time": {
      "full_name": "/time",
      "datatype": "Float64",
      "dimensions": [
        "/time"
      ],
      "dimension_size": [
        1
      ],
      "attributes": {
        "calendar": "JULIAN",
        "calendar_type": "JULIAN",
        "cartesian_axis": "T",
        "long_name": "time"
      },
      "unit": "hours since 2026-05-19 00:00:00"
    }

::

    $ gdal mdim info 20260519_06_tmp2m.nc

.. code-block:: json

    "time": {
      "full_name": "/time",
      "datatype": "Float64",
      "dimensions": [
        "/time"
      ],
      "dimension_size": [
        1
      ],
      "attributes": {
        "calendar": "JULIAN",
        "calendar_type": "JULIAN",
        "cartesian_axis": "T",
        "long_name": "time"
      },
      "unit": "hours since 2026-05-19 06:00:00"
    }


We are going to modify the "time" (single) value of :file:`20260519_06_tmp2m.nc`
so it is relative to "hours since 2026-05-19 00:00:00" by first creating a
:ref:`multidimensional VRT <vrt_multidimensional>`

::

    $ gdal mdim convert 20260519_06_tmp2m.nc 20260519_06_tmp2m.vrt
    $ cp 20260519_06_tmp2m.vrt 20260519_06_tmp2m_mod.vrt

And modifying the time array as following:

.. code-block:: xml

    <Array name="time">
      <DataType>Float64</DataType>
      <DimensionRef ref="time" />
      <Unit>hours since 2026-05-19 00:00:00</Unit>
      <InlineValuesWithValueElement><Value>6</Value></InlineValuesWithValueElement>
      <Attribute name="calendar">
        <DataType>String</DataType>
        <Value>JULIAN</Value>
      </Attribute>
      <Attribute name="calendar_type">
        <DataType>String</DataType>
        <Value>JULIAN</Value>
      </Attribute>
      <Attribute name="cartesian_axis">
        <DataType>String</DataType>
        <Value>T</Value>
      </Attribute>
      <Attribute name="long_name">
        <DataType>String</DataType>
        <Value>time</Value>
      </Attribute>
    </Array>

Now we can convert it back to netCDF:

::

    $ gdal mdim convert 20260519_06_tmp2m_mod.vrt 20260519_06_tmp2m_mod.nc


Mosaicing / creating a 3D cube
------------------------------

Using :ref:`gdal mdim mosaic <gdal_mdim_mosaic>`

::

    $ gdal mdim mosaic 20260519_00_tmp2m.nc 20260519_06_tmp2m_mod.nc 20260519_00_06_tmp2m.zarr --format Zarr

    $ gdal mdim info 20260519_00_06_tmp2m.zarr

.. collapse:: Output

    .. code-block:: json

        {
          "type": "group",
          "driver": "Zarr",
          "name": "/",
          "dimensions": [
            {
              "name": "grid_xt",
              "full_name": "/grid_xt",
              "size": 3072,
              "indexing_variable": {
                "grid_xt": {
                  "full_name": "/grid_xt",
                  "datatype": "Float64",
                  "dimensions": [
                    "/grid_xt"
                  ],
                  "dimension_size": [
                    3072
                  ],
                  "block_size": [
                    3072
                  ],
                  "attributes": {
                    "cartesian_axis": "X",
                    "long_name": "T-cell longitude"
                  }
                }
              }
            },
            {
              "name": "grid_yt",
              "full_name": "/grid_yt",
              "size": 1536,
              "indexing_variable": {
                "grid_yt": {
                  "full_name": "/grid_yt",
                  "datatype": "Float64",
                  "dimensions": [
                    "/grid_yt"
                  ],
                  "dimension_size": [
                    1536
                  ],
                  "block_size": [
                    1536
                  ],
                  "attributes": {
                    "cartesian_axis": "Y",
                    "long_name": "T-cell latitude"
                  }
                }
              }
            },
            {
              "name": "time",
              "full_name": "/time",
              "size": 2,
              "indexing_variable": {
                "time": {
                  "full_name": "/time",
                  "datatype": "Float64",
                  "dimensions": [
                    "/time"
                  ],
                  "dimension_size": [
                    2
                  ],
                  "block_size": [
                    2
                  ],
                  "attributes": {
                    "calendar": "JULIAN",
                    "calendar_type": "JULIAN",
                    "cartesian_axis": "T",
                    "long_name": "time"
                  }
                }
              }
            }
          ],
          "arrays": {
            "grid_xt": {
              "full_name": "/grid_xt",
              "datatype": "Float64",
              "dimensions": [
                "/grid_xt"
              ],
              "dimension_size": [
                3072
              ],
              "block_size": [
                3072
              ],
              "attributes": {
                "cartesian_axis": "X",
                "long_name": "T-cell longitude"
              }
            },
            "grid_yt": {
              "full_name": "/grid_yt",
              "datatype": "Float64",
              "dimensions": [
                "/grid_yt"
              ],
              "dimension_size": [
                1536
              ],
              "block_size": [
                1536
              ],
              "attributes": {
                "cartesian_axis": "Y",
                "long_name": "T-cell latitude"
              }
            },
            "time": {
              "full_name": "/time",
              "datatype": "Float64",
              "dimensions": [
                "/time"
              ],
              "dimension_size": [
                2
              ],
              "block_size": [
                2
              ],
              "attributes": {
                "calendar": "JULIAN",
                "calendar_type": "JULIAN",
                "cartesian_axis": "T",
                "long_name": "time"
              }
            },
            "tmp2m": {
              "full_name": "/tmp2m",
              "datatype": "Float32",
              "dimensions": [
                "/time",
                "/grid_yt",
                "/grid_xt"
              ],
              "dimension_size": [
                2,
                1536,
                3072
              ],
              "block_size": [
                1,
                768,
                1536
              ],
              "attributes": {
                "cell_methods": "time: point",
                "long_name": "2m temperature",
                "missing": 9.99e+20,
                "output_file": "sfc"
              },
              "unit": "K"
            }
          }
        }

|
