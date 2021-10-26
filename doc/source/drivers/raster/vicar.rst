.. _raster.vicar:

================================================================================
VICAR -- VICAR
================================================================================

.. shortname:: VICAR

.. built_in_by_default::

.. note::
    PDS3 datasets can incorporate a VICAR header. By default, GDAL will use the
    :ref:`PDS <raster.pds>` driver in that situation. Starting with GDAL 3.1, if
    the :decl_configoption:`GDAL_TRY_PDS3_WITH_VICAR` configuration option is
    set to YES, the dataset will be opened by the VICAR driver.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

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

Binary prefixes
---------------

Starting with GDAL 3.1, if the VICAR label declares a non-zero binary prefix
length (`NBB` label item), then GDAL will look in the `vicar.json` configuration file if
there is an entry corresponding to the `BLTYPE` label item (currently only
M94_HRSC is defined), and if there is a match, a OGR vector layer will be
available on the dataset, with a feature for each image record.

For example:

::

    $ ogrinfo h0038_0000.bl2.16 -al -q

    Layer name: binary_prefixes
    OGRFeature(binary_prefixes):0
        EphTime (Real) = 127988268.646895
        Exposure (Real) = 40.1072692871094
        COT (Integer) = 28275
        FEETemp (Integer) = 28508
        FPMTemp (Integer) = 29192
        OBTemp (Integer) = 28295
        FERT (Integer) = 27001
        LERT (Integer) = 28435
        CmpDataLen (Integer) = 146
        FrameCount (Integer) = 486
        Pischel (Integer) = 5
        ActPixel (Integer) = 5120
        RSHits (Integer) = 0
        DceInput (Integer) = 0
        DceOutput (Integer) = 4
        FrameErr1 (Integer) = 0
        FrameErr2 (Integer) = 0
        Gob1 (Integer) = 0
        Gob2 (Integer) = 0
        Gob3 (Integer) = 0
        DSS (Integer) = 97
        DecmpErr1 (Integer) = 0
        DecmpErr2 (Integer) = 0
        DecmpErr3 (Integer) = 0
        FillerFlag (Integer) = 5


Creation support
----------------

Starting with GDAL 3.1, the VICAR driver supports updating imagery of
existing datasets, creating new datasets through the CreateCopy() and
Create() interfaces.

When using CreateCopy(), gdal_translate or gdalwarp, an effort is made
to preserve as much as possible of the original label when doing VICAR
to VICAR conversions. This can be disabled with the USE_SRC_LABEL=NO
creation option.

The available creation options are:

-  **GEOREF_FORMAT**\ =MIPL/GEOTIFF. (GDAL >= 3.4) How to encode georeferencing
   information. Defaults to MIPL using the ``MAP`` property group. When setting to
   GEOTIFF, a ``GEOTIFF`` property group will be used using GeoTIFF keys and tags.
   The COORDINATE_SYSTEM_NAME, POSITIVE_LONGITUDE_DIRECTION and TARGET_NAME
   options will be ignored when selecting the GEOTIFF encoding.
-  **COORDINATE_SYSTEM_NAME**\ =PLANETOCENTRIC/PLANETOGRAPHIC. Value of
   MAP.COORDINATE_SYSTEM_NAME. Defaults to PLANETOCENTRIC. If specified, and
   USE_SRC_MAP is in effect, this will be taken into account to
   override the source COORDINATE_SYSTEM_NAME.
-  **POSITIVE_LONGITUDE_DIRECTION**\ =EAST/WEST. Value of
   MAP.override. Defaults to EAST. If specified,
   and USE_SRC_MAP is in effect, this will be taken into account to
   override the source POSITIVE_LONGITUDE_DIRECTION.
-  **TARGET_NAME**\ =string. Value of MAP.TARGET_NAME. This is
   normally deduced from the SRS datum name. If specified, and
   USE_SRC_MAP is in effect, this will be taken into account to
   override the source TARGET_NAME.
-  **USE_SRC_LABEL**\ =YES/NO. Whether to use source label in VICAR to
   VICAR conversions. Defaults to YES.
-  **LABEL**\ =string. Label to use, either as a JSON string or a filename
   containing one. If defined, takes precedence over USE_SRC_LABEL.
-  **COMPRESS**\= NONE/BASIC/BASIC2. Compression method. Default to NONE.
   For maximum interoperability, do not use BASIC or BASIC2 which are not
   well specified and not always available in VICAR capable applications.

See Also
--------

- Implemented as ``gdal/frmts/pds/vicardataset.cpp``.
- `VICAR documentation <https://www-mipl.jpl.nasa.gov/vicar.html>`_
- `VICAR file format <https://www-mipl.jpl.nasa.gov/external/VICAR_file_fmt.pdf>`_
