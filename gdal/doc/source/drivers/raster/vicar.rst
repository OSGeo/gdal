.. _raster.vicar:

================================================================================
VICAR -- VICAR
================================================================================

.. shortname:: VICAR

See `VICAR documentation <https://www-mipl.jpl.nasa.gov/external/vicar.htm>`_

NOTE: Implemented as ``gdal/frmts/pds/vicardataset.cpp``.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::


Metadata
--------

Starting with GDAL 3.1, the VICAR label can be retrieved as
JSon-serialized content in the json:VICAR metadata domain.

For example:

::

   $ python
   from osgeo import gdal
   ds = gdal.Open('../autotest/gdrivers/data/test_vicar_truncated.bin')
   print(ds.GetMetadata_List('json:VICAR')[0])
   {
    "LBLSIZE":9680,
    "FORMAT":"BYTE",
    "TYPE":"IMAGE",
    "BUFSIZ":2097152,
    "DIM":3,
    "EOL":0,
    "RECSIZE":4840,
    "ORG":"BSQ",
    "NL":1000,
    "NS":400,
    "NB":1,
    "N1":4000,
    "N2":1000,
    "N3":1,
    "N4":0,
    "NBB":0,
    "NLB":0,
    "HOST":"X86-64-LINX",
    "INTFMT":"LOW",
    "REALFMT":"RIEEE",
    "BHOST":"X86-LINUX",
    "BINTFMT":"LOW",
    "BREALFMT":"RIEEE",
    "BLTYPE":"M94_HRSC",
    "COMPRESS":"NONE",
    "EOCI1":0,
    "EOCI2":0,
    "PROPERTY":{
        "M94_ORBIT":{
            "ORBIT_NUMBER":5273,
            "ASCENDING_NODE_LONGITUDE":118.46,
            "ORBITAL_ECCENTRICITY":1.23,
            "ORBITAL_INCLINATION":4.56,
            "PERIAPSIS_ARGUMENT_ANGLE":7.89,
            "PERIAPSIS_TIME":"PERIAPSIS_TIME",
            "PERIAPSIS_ALTITUDE":333.16,
            "ORBITAL_SEMIMAJOR_AXIS":1.23,
            "SPACECRAFT_SOLAR_DISTANCE":4.56,
            "SPACECRAFT_CLOCK_START_COUNT":"1\/1",
            "SPACECRAFT_CLOCK_STOP_COUNT":"1\/2",
            "START_TIME":"start_time",
            "STOP_TIME":"stop_time",
            "SPACECRAFT_POINTING_MODE":"NADIR",
            "RIGHT_ASCENSION":-1.0000000000000001e+32,
            "DECLINATION":-1.0000000000000001e+32,
            "OFFSET_ANGLE":-1.0000000000000001e+32,
            "SPACECRAFT_ORIENTATION":[
                0.000000,
                -1.000000,
                0.000000
            ]
        },
        [...]
        "PHOT":{
            "PHO_FUNC":"NONE"
        }
    },
    "TASK":{
        "HRCONVER":{
            "USER":"mexsyst",
            "DAT_TIM":"DAT_TIM",
            "SPICE_FILE_NAME":[
                "foo"
            ],
            "SPICE_FILE_ID":"(LSK,SCLK,ON)",
            "DETECTOR_TEMPERATURE":1.23,
            "DETECTOR_TEMPERATURE__UNIT":"degC",
            "FOCAL_PLANE_TEMPERATURE":8.5833,
            "FOCAL_PLANE_TEMPERATURE__UNIT":"degC",
            "INSTRUMENT_TEMPERATURE":2.34,
            "INSTRUMENT_TEMPERATURE__UNIT":"degC",
            "LENS_TEMPERATURE":4.56,
            "LENS_TEMPERATURE__UNIT":"degC",
            "SOURCE_FILE_NAME":"SOURCE_FILE_NAME",
            "MISSING_FRAMES":0,
            "OVERFLOW_FRAMES":0,
            "ERROR_FRAMES":1
        }
      }
    }

or

::

   $ gdalinfo -json ../autotest/gdrivers/data/test_vicar_truncated.bin -mdd all
