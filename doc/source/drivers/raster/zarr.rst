.. _raster.zarr:

================================================================================
Zarr
================================================================================

.. versionadded:: 3.4

.. shortname:: Zarr

.. build_dependencies:: Built-in by default, but liblz4, libxz (lzma), libzstd and libblosc
                        strongly recommended to get all compressors

Zarr is a format for the storage of chunked, compressed, N-dimensional arrays.
This format is supported for read and write access, and using the traditional
2D raster API or the newer multidimensional API

The driver supports the Zarr V2 specification, and has experimental support
for the in-progress Zarr V3 specification.

.. warning::

    The current implementation of Zarr V3 before GDAL 3.8 is incompatible with
    the latest evolutions of the Zarr V3 specification.
    GDAL 3.8 is compatible with the Zarr V3 specification at date 2023-May-7,
    and is not interoperable with Zarr V3 datasets produced by earlier GDAL
    versions.

Local and cloud storage (see :ref:`virtual_file_systems`) are supported in read and write.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_multidimensional::

.. supports_virtualio::

Concepts
--------

A Zarr dataset is made of a hierarchy of nodes, with intermediate nodes being
groups (:cpp:class:`GDALGroup`), and leafs being arrays (:cpp:class:`GDALMDArray`).

Dataset name
------------

For Zarr V2, the dataset name recognized by the Open() method of the driver is
a directory that contains a :file:`.zgroup` file, a :file:`.zarray` file or a
:file:`.zmetadata` file (consolidated metadata). For faster exploration,
the driver will use consolidated metadata by default when found.

For Zarr V3, the dataset name recognized by the Open() method of the driver is
a directory that contains a :file:`zarr.json` file (root of the dataset).

For datasets on file systems where file listing is not reliable, as often with
/vsicurl/, it is also possible to prefix the directory name with ``ZARR:``,
and it is necessary to surround the /vsicurl/-prefixed URL with double quotes.
e.g `ZARR:"/vsicurl/https://example.org/foo.zarr"`. Note that when passing such
string in a command line shell, extra quoting might be necessary to preserve the
double-quoting.

For example with a Bash shell, the whole connection string needs to be surrounded
with single-quote characters:

::

    gdalmdiminfo 'ZARR:"/vsicurl/https://example.org/foo.zarr"'


Compression methods
-------------------

Compression methods available depend on how GDAL is built, and
`libblosc <https://github.com/Blosc/c-blosc>`__ too.

A full-feature build will show:

::

    $ gdalinfo --format Zarr

    [...]

      Other metadata items:
        COMPRESSORS=blosc,zlib,gzip,lzma,zstd,lz4
        BLOSC_COMPRESSORS=blosclz,lz4,lz4hc,snappy,zlib,zstd

For specific uses, it is also possible to register at run-time extra compressors
and decompressors with :cpp:func:`CPLRegisterCompressor` and :cpp:func:`CPLRegisterDecompressor`.

XArray _ARRAY_DIMENSIONS
------------------------

The driver support the ``_ARRAY_DIMENSIONS`` special attribute used by
`XArray <http://xarray.pydata.org/en/stable/generated/xarray.open_zarr.html>`__
to store the dimension names of an array.

NCZarr extensions
-----------------

The driver support the
`NCZarr v2 <https://www.unidata.ucar.edu/software/netcdf/documentation/NUG/nczarr_head.html>`__
extensions of storing the dimension names of an array (read-only)

SRS encoding
------------

The Zarr specification has no provision for spatial reference system encoding.
GDAL uses a ``_CRS`` attribute that is a dictionary that may contain one or
several of the following keys: ``url`` (using a OGC CRS URL), ``wkt`` (WKT:2019
used by default on writing, WKT1 also supported on reading.), ``projjson``.
On reading, it will use ``url`` by default, if not found will fallback to ``wkt``
and then ``projjson``.

.. code-block:: json

    {
      "_CRS":{
        "wkt":"PROJCRS[\"NAD27 \/ UTM zone 11N\",BASEGEOGCRS[\"NAD27\",DATUM[\"North American Datum 1927\",ELLIPSOID[\"Clarke 1866\",6378206.4,294.978698213898,LENGTHUNIT[\"metre\",1]]],PRIMEM[\"Greenwich\",0,ANGLEUNIT[\"degree\",0.0174532925199433]],ID[\"EPSG\",4267]],CONVERSION[\"UTM zone 11N\",METHOD[\"Transverse Mercator\",ID[\"EPSG\",9807]],PARAMETER[\"Latitude of natural origin\",0,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8801]],PARAMETER[\"Longitude of natural origin\",-117,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8802]],PARAMETER[\"Scale factor at natural origin\",0.9996,SCALEUNIT[\"unity\",1],ID[\"EPSG\",8805]],PARAMETER[\"False easting\",500000,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8806]],PARAMETER[\"False northing\",0,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8807]]],CS[Cartesian,2],AXIS[\"easting\",east,ORDER[1],LENGTHUNIT[\"metre\",1]],AXIS[\"northing\",north,ORDER[2],LENGTHUNIT[\"metre\",1]],ID[\"EPSG\",26711]]",

        "projjson":{
          "$schema":"https:\/\/proj.org\/schemas\/v0.2\/projjson.schema.json",
          "type":"ProjectedCRS",
          "name":"NAD27 \/ UTM zone 11N",
          "base_crs":{
            "name":"NAD27",
            "datum":{
              "type":"GeodeticReferenceFrame",
              "name":"North American Datum 1927",
              "ellipsoid":{
                "name":"Clarke 1866",
                "semi_major_axis":6378206.4,
                "inverse_flattening":294.978698213898
              }
            },
            "coordinate_system":{
              "subtype":"ellipsoidal",
              "axis":[
                {
                  "name":"Geodetic latitude",
                  "abbreviation":"Lat",
                  "direction":"north",
                  "unit":"degree"
                },
                {
                  "name":"Geodetic longitude",
                  "abbreviation":"Lon",
                  "direction":"east",
                  "unit":"degree"
                }
              ]
            },
            "id":{
              "authority":"EPSG",
              "code":4267
            }
          },
          "conversion":{
            "name":"UTM zone 11N",
            "method":{
              "name":"Transverse Mercator",
              "id":{
                "authority":"EPSG",
                "code":9807
              }
            },
            "parameters":[
              {
                "name":"Latitude of natural origin",
                "value":0,
                "unit":"degree",
                "id":{
                  "authority":"EPSG",
                  "code":8801
                }
              },
              {
                "name":"Longitude of natural origin",
                "value":-117,
                "unit":"degree",
                "id":{
                  "authority":"EPSG",
                  "code":8802
                }
              },
              {
                "name":"Scale factor at natural origin",
                "value":0.9996,
                "unit":"unity",
                "id":{
                  "authority":"EPSG",
                  "code":8805
                }
              },
              {
                "name":"False easting",
                "value":500000,
                "unit":"metre",
                "id":{
                  "authority":"EPSG",
                  "code":8806
                }
              },
              {
                "name":"False northing",
                "value":0,
                "unit":"metre",
                "id":{
                  "authority":"EPSG",
                  "code":8807
                }
              }
            ]
          },
          "coordinate_system":{
            "subtype":"Cartesian",
            "axis":[
              {
                "name":"Easting",
                "abbreviation":"",
                "direction":"east",
                "unit":"metre"
              },
              {
                "name":"Northing",
                "abbreviation":"",
                "direction":"north",
                "unit":"metre"
              }
            ]
          },
          "id":{
            "authority":"EPSG",
            "code":26711
          }
        },

        "url":"http:\/\/www.opengis.net\/def\/crs\/EPSG\/0\/26711"
      }
    }

Particularities of the classic raster API
-----------------------------------------

If the Zarr dataset contains one single array with 2 dimensions, it will be
exposed as a regular GDALDataset when using the classic raster API.
If the dataset contains more than one such single array, or arrays with 3 or
more dimensions, the driver will list subdatasets to access each array and/or
2D slices within arrays with 3 or more dimensions.

Open options
------------

The following dataset open options are available:

-  .. oo:: USE_ZMETADATA
      :choices: YES, NO
      :default: YES

      Whether to use consolidated metadata from .zmetadata (Zarr V2 only).

-  .. oo:: CACHE_TILE_PRESENCE
      :choices: YES, NO
      :default: NO

      Whether to establish an initial listing of
      present tiles. This cached listing will be stored in a .gmac file next to the
      .zarray / .array.json.gmac file if they can be written. Otherwise the
      :config:`GDAL_PAM_PROXY_DIR` config option should be set to an
      existing directory where those cached files will be stored. Once the cached
      listing has been established, the open option no longer needs to be specified.
      Note: the runtime of this option can be in minutes or more for large datasets
      stored on remote file systems. And for network file systems, this will rarely
      work for /vsicurl/ itself, but more cloud-based file systems (such as /vsis3/,
      /vsigs/, /vsiaz/, etc) which have a dedicated directory listing operation.

-  .. oo:: MULTIBAND
      :choices: YES, NO
      :default: YES
      :since: 3.8

      Whether to expose > 3D arrays as GDAL multiband datasets (when using the
      classic 2D API)

-  .. oo:: DIM_X
      :choices: <string> or <integer>
      :since: 3.8

      Name or index of the X dimension (only used when MULTIBAND=YES and with
      th classic 2D API). If not specified, deduced from dimension type
      (when equal to "HORIZONTAL_X"), or the last dimension (i.e. fastest
      varying one), if no dimension type found.

-  .. oo:: DIM_Y
      :choices: <string> or <integer>
      :since: 3.8

      Name or index of the Y dimension (only used when MULTIBAND=YES and with
      th classic 2D API). If not specified, deduced from dimension type
      (when equal to "HORIZONTAL_Y"), or the before last dimension, if no
      dimension type found.

-  .. oo:: LOAD_EXTRA_DIM_METADATA_DELAY
      :choices: <integer> or "unlimited"
      :default: 5
      :since: 3.8

      Maximum delay in seconds allowed to set the DIM_{dimname}_VALUE band
      metadata items from the indexing variable of the dimensions.
      Default value is 5. ``unlimited`` can be used to mean unlimited delay.
      Can also be defined globally with the GDAL_LOAD_EXTRA_DIM_METADATA_DELAY
      configuration` option.
      Only used through the classic 2D API.

Multi-threaded caching
----------------------

The driver implements the :cpp:func:`GDALMDArray::AdviseRead` method. This
proceed to multi-threaded decoding of the tiles that intersect the area of
interest specified. A sufficient cache size must be specified. The call is
blocking.

The options that can be passed to the methods are:

- **CACHE_SIZE=value_in_byte**: Maximum RAM to use, expressed in number of bytes.
  If not specified, half of the remaining GDAL block cache size will be used.
  Note: the caching mechanism of Zarr array will not update this remaining block
  cache size.

- **NUM_THREADS=integer or ALL_CPUS**: Number of threads to use in parallel.
  If not specified, the :config:`GDAL_NUM_THREADS` configuration option
  will be taken into account.

Creation options
----------------

The following options are creation options of the classic raster API, or
array-level creation options for the multidimensional API (must be prefixed
with ``ARRAY:`` using :program:`gdalmdimtranslate`):

-  .. co:: COMPRESS
      :choices: NONE, BLOSC, ZLIB, GZIP, LZMA, ZSTD, LZ4
      :default: NONE

      Compression method.

-  .. co:: FILTER
      :choices: NONE, DELTA
      :default: NONE

      Filter method. Only support for FORMAT=ZARR_V2.

-  .. co:: BLOCKSIZE
      :choices: <string>

      Comma separated list of chunk size along each dimension.
      If not specified, the fastest varying 2 dimensions (the last ones) used a
      block size of 256 samples, and the other ones of 1.

-  .. co:: CHUNK_MEMORY_LAYOUT
      :choices: C, F
      :default: C

      Whether to use C (row-major) order or F (column-major)
      order in encoded chunks. Only useful when using compression.
      Changing to F may improve depending on array content.

-  .. co:: STRING_FORMAT
      :choices: ASCII, UNICODE
      :default: ASCII

      Whether to use the numpy type for ASCII-only
      strings or Unicode strings. Unicode strings take 4 byte per character.

-  .. co:: DIM_SEPARATOR
      :choices: <string>

      Dimension separator in chunk filenames.
      Default to decimal point for ZarrV2 and slash for ZarrV3.

-  .. co:: BLOSC_CNAME
      :choices: bloclz, lz4, lz4hc, snappy, zlib, zstd
      :default: lz4

      Blosc compressor name. Only used when :co:`COMPRESS=BLOSC`.

-  .. co:: BLOSC_CLEVEL
      :choices: 1-9
      :default: 5

      Blosc compression level. Only used when :co:`COMPRESS=BLOSC`.

-  .. co:: BLOSC_SHUFFLE
      :choices: NONE, BYTE, BIT
      :default: BYTE

      Type of shuffle algorithm. Only used when :co:`COMPRESS=BLOSC`.

-  .. co:: BLOSC_BLOCKSIZE
      :choices: <integer>
      :default: 0

      Blosc block size. Only used when :co:`COMPRESS=BLOSC`.

-  .. co:: BLOSC_NUM_THREADS
      :choices: <integer>, ALL_CPUS
      :default: 1

      Number of worker threads for compression.
      Only used when :co:`COMPRESS=BLOSC`.

-  .. co:: ZLIB_LEVEL
      :choices: 1-9
      :default: 6

      ZLib compression level. Only used when :co:`COMPRESS=ZLIB`.

-  .. co:: GZIP_LEVEL
      :choices: 1-9
      :default: 6

      GZip compression level. Only used when :co:`COMPRESS=GZIP`.

-  .. co:: LZMA_PRESET
      :choices: 0-9
      :default: 6

      LZMA compression level. Only used when :co:`COMPRESS=LZMA`.

-  .. co:: LZMA_DELTA
      :choices: <integer>
      :default: 1

      Delta distance in byte. Only used when :co:`COMPRESS=LZMA`.

-  .. co:: ZSTD_LEVEL
      :choices: 1-22
      :default: 13

      ZSTD compression level. Only used when :co:`COMPRESS=ZSTD`.

-  .. co:: LZ4_ACCELERATION
      :choices: <integer> [1-]
      :default: 1

      LZ4 acceleration factor.
      The higher, the less compressed. Only used when :co:`COMPRESS=LZ4`.
      Defaults to 1 (the fastest).

-  .. co:: DELTA_DTYPE
      :choices: <string>

      Data type following NumPy array protocol type
      string (typestr) format (https://numpy.org/doc/stable/reference/arrays.interface.html#arrays-interface).
      Only ``u1``, ``i1``, ``u2``, ``i2``, ``u4``, ``i4``, ``u8``, ``i8``, ``f4``, ``f8``,
      potentially prefixed with the endianness flag (``<`` for little endian, ``>`` for big endian)
      are supported.
      Only used when :co:`FILTER=DELTA`. Defaults to the native data type.


The following options are creation options of the classic raster API, or
dataset-level creation options for the multidimensional API :

-  .. co:: FORMAT
      :choices: ZARR_V2, ZARR_V3
      :default: ZARR_V2

-  .. co:: CREATE_ZMETADATA
      :choices: YES, NO
      :default: YES

      Whether to create consolidated metadata into
      .zmetadata (Zarr V2 only).

The following options are creation options of the classic raster API only:

-  .. co:: ARRAY_NAME
      :choices: <string>

      Array name. If not specified, deduced from the filename.

-  .. co:: APPEND_SUBDATASET
      :choices: YES, NO
      :default: NO

      Whether to append the new dataset to an existing Zarr hierarchy.

-  .. co:: SINGLE_ARRAY
      :choices: YES, NO
      :default: YES
      :since: 3.8

      Whether to write a multi-band dataset as a 3D Zarr array. If false,
      one 2D Zarr array per band will be written.

-  .. co:: INTERLEAVE
      :choices: BAND, PIXEL
      :default: BAND
      :since: 3.8

      When writing a multi-band dataset as a 3D Zarr array, whether the band
      dimension should be the first one/slowest varying one (BAND), or the
      last one/fastest varying one (INTERLEAVE)


Examples
--------

Get information on the dataset using the multidimensional tools:

::

    gdalmdiminfo my.zarr


Convert a netCDF file to ZARR using the multidimensional tools:

::

    gdalmdimtranslate in.nc out.zarr -co ARRAY:COMPRESS=GZIP


Convert a 2D slice (the one at index 0 of the non-2D dimension) of a 3D array to GeoTIFF:

::

    gdal_translate 'ZARR:"my.zarr":/group/myarray:0' out.tif


.. note::
    The single quoting around the connection string is specific to the Bash shell
    to make sure that the double quoting is preserved.


See Also:
---------

- `Zarr format and its Python implementation <https://zarr.readthedocs.io/en/stable/>`__
- `(In progress) Zarr V3 specification <https://zarr-specs.readthedocs.io/en/core-protocol-v3.0-dev/>`__
