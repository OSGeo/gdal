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
/vsicurl/, it is also possible to prefix the directory name with ``ZARR:``.

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

The driver support the ``_ARRAY_DIMENSIONS`` specifial attribute used by
`XArray <http://xarray.pydata.org/en/stable/generated/xarray.open_zarr.html>`__
to store the dimension names of an array.

SRS encoding
------------

The Zarr specification has no provision for spatial reference system encoding.
GDAL uses a ``crs`` attribute that is a dictionnary that may contain one or
several of the following keys: ``url`` (using a OGC CRS URL), ``wkt`` (WKT:2019
used by default on writing, WKT1 also supported on reading.), ``projjson``.
On reading, it will use ``url`` by default, if not found will fallback to ``wkt``
and then ``projjson``.

.. code-block:: json

    {
      "crs":{
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

Creation options
----------------

The following options are creation options of the classic raster API, or
array-level creation options for the multidimensional API (must be prefixed
with ``ARRAY:`` using :program:`gdalmdimtranslate`):

- **COMPRESS=[NONE/BLOSC/ZLIB/GZIP/LZMA/ZSTD/LZ4]**: Compression method.
  Defaults to NONE.

- **FILTER=[NONE/DELTA]**: Filter method. Only support for FORMAT=ZARR_V2.
  Defaults to NONE.

- **BLOCKSIZE=string**: Comma separated list of chunk size along each dimension.
  If not specified, the fastest varying 2 dimensions (the last ones) used a
  block size of 256 samples, and the other ones of 1.

- **CHUNK_MEMORY_LAYOUT=C/F**: Whether to use C (row-major) order or F (column-major)
  order in encoded chunks. Only useful when using compression. Defaults to C.
  Changing to F may improve depending on array content.

- **DIM_SEPARATOR=string**: Dimension separator in chunk filenames.
  Default to decimal point for ZarrV2 and slash for ZarrV3.

- **BLOSC_CNAME=bloclz/lz4/lz4hc/snappy/zlib/std**: Blosc compressor name.
  Only used when COMPRESS=BLOSC. Defaults to lz4.

- **BLOSC_CLEVEL=integer** [1-9]: Blosc compression level. Only used when COMPRESS=BLOSC.
  Defaults to 5.

- **BLOSC_SHUFFLE=NONE/BYTE/BIT**: Type of shuffle algorithm. Only used when COMPRESS=BLOSC.
  Defaults to BYTE.

- **BLOSC_BLOCKSIZE=integer**: Blosc block size. Only used when COMPRESS=BLOSC.
  Defaults to 0.

- **BLOSC_NUM_THREADS=string**: Number of worker threads for compression.
  Can be set to ``ALL_CPUS``. Only used when COMPRESS=BLOSC. Defaults to 1.

- **ZLIB_LEVEL=integer** [1-9]: ZLib compression level. Only used when COMPRESS=ZLIB.
  Defaults to 6.

- **GZIP_LEVEL=integer** [1-9]: GZip compression level. Only used when COMPRESS=GZIP.
  Defaults to 6.

- **LZMA_PRESET=integer** [0-9]: LZMA compression level. Only used when COMPRESS=LZMA.
  Defaults to 6.

- **LZMA_DELTA=integer** : Delta distance in byte. Only used when COMPRESS=LZMA.
  Defaults to 1.

- **ZSTD_LEVEL=integer** [1-9]: ZSTD compression level. Only used when COMPRESS=ZSTD.
  Defaults to 13.

- **LZ4_ACCELERATION=integer** [1-]: LZ4 acceleration factor.
  The higher, the less compressed. Only used when COMPRESS=LZ4.
  Defaults to 1 (the fastest).

- **DELTA_DTYPE=string** [1-]: Data type following NumPy array protocol type
  string (typestr) format (https://numpy.org/doc/stable/reference/arrays.interface.html#arrays-interface).
  Only ``u1``, ``i1``, ``u2``, ``i2``, ``u4``, ``i4``, ``u8``, ``i8``, ``f4``, ``f8``,
  potentially prefixed with the endianness flag (``<`` for little endian, ``>`` for big endian)
  are supported.
  Only used when FILTER=DELTA. Defaults to the native data type.


The following options are creation options of the classic raster API, or
dataset-level creation options for the multidimensional API :

- **FORMAT=[ZARR_V2/ZARR_V3]**: Defaults to ZARR_V2

- **CREATE_ZMETADATA=[YES/NO]**: Whether to create consolidated metadata into
  .zmetadata (Zarr V2 only). Defaults to YES.


The following options are creation options of the classic raster API only:

- **ARRAY_NAME=string**: Array name. If not specified, deduced from the filename.

- **APPEND_SUBDATASET=YES/NO**: Whether to append the new dataset to an existing
  Zarr hierarchy. Defaults to NO.


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

    gdal_translate ZARR:"my.zarr":/group/myarray:0 out.tif


See Also:
---------

- `Zarr format and its Python implementation <https://zarr.readthedocs.io/en/stable/>`__
- `(In progress) Zarr V3 specification <https://zarr-specs.readthedocs.io/en/core-protocol-v3.0-dev/>`__
