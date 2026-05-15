.. _gdal_mdim_get_refs:

================================================================================
``gdal mdim get-refs``
================================================================================

.. versionadded:: 3.14

.. program:: gdal mdim get-refs

.. only:: html

    Extract per-chunk byte references from a multidimensional raster array
    into a vector layer.

.. Index:: gdal mdim get-refs

Synopsis
--------

.. program-output:: gdal mdim get-refs --help-doc

Description
-----------

:program:`gdal mdim get-refs` enumerates the chunks of a multidimensional
array and writes one feature per chunk into a vector layer. Each feature
records the chunk's coordinates within the array's chunk grid, the location
of the chunk's backing storage (file path, byte offset, and byte size), and
any per-chunk metadata reported by the driver.

The output is an attribute-only vector layer (no geometry).

The algorithm is format-general: it relies on the multidimensional
``GetRawBlockInfo()``.

Three-state classification
~~~~~~~~~~~~~~~~~~~~~~~~~~

Each chunk is classified as one of three states, reflected in the ``present``
field of the output layer:

* **Present** (``present=1``, ``path``/``offset``/``size`` populated): the
  chunk is file-backed and can be read from the given byte range.
* **Absent** (``present=0``, ``path``/``offset``/``size`` NULL): the chunk
  is not stored (sparse Zarr, missing chunks). Consumers should substitute
  the array's fill value.
* **Inline** (``present=1``, ``path``/``offset`` NULL, ``size`` populated):
  the chunk data is small enough to be embedded in the storage metadata
  rather than referenced as bytes. (Not yet implemented.)

Schema
------

The output layer's schema is determined by the input array's rank:

.. list-table::
   :header-rows: 1

   * - Field name
     - Type
     - Description
   * - ``dim_0`` .. ``dim_{n-1}``
     - Integer64
     - Chunk coordinate per dimension of the input array, in row-major
       order. The dimension *names* are not used as column names directly
       (to avoid sanitization concerns); they are recorded in the layer
       metadata as ``DIM_N_NAME``.
   * - ``present``
     - Integer (Boolean subtype)
     - 1 if the chunk has storage (file-backed or inline); 0 if absent.
   * - ``path``
     - String, nullable
     - For present chunks, the path to the backing file (including any
       ``/vsicurl/`` or other VSI prefix). NULL for absent and inline
       chunks.
   * - ``offset``
     - Integer64, nullable
     - For file-backed present chunks, the byte offset within the file
       where the chunk's raw bytes start. For native Zarr (one file per
       chunk), this is typically 0. NULL for absent and inline chunks. Note
       that ``offset`` is a SQL reserved word and must be quoted in queries.
   * - ``size``
     - Integer64, nullable
     - For present chunks, the number of bytes of raw chunk storage.
       NULL for absent chunks.
   * - ``info``
     - String, nullable
     - Per-chunk metadata reported by the driver, joined as ``KEY=VALUE``
       pairs separated by ``; ``. Typically describes compression and
       byte-order. The same codec information is also hoisted to the layer
       as metadata items ``CODEC_*``.

Layer metadata
~~~~~~~~~~~~~~

The output layer carries the following metadata items, accessible via
:cpp:func:`GDALMajorObject::GetMetadata` (``-mdd`` in :program:`ogrinfo`):

* ``ARRAY_NAME`` — the fullname of the input array
* ``DTYPE`` — the array's data type (e.g. ``Int16``, ``Float32``)
* ``DIM_N_NAME``, ``DIM_N_SIZE``, ``DIM_N_BLOCK``, ``DIM_N_CHUNKS`` — for
  each dimension, the dimension name, size, block size, and number of
  chunks along that axis
* ``CODEC_*`` — array-level codec metadata hoisted from the first chunk
  (e.g. ``CODEC_COMPRESSION=DEFLATE``, ``CODEC_FILTER=SHUFFLE``)

Limitations
-----------

* Only a single array can be emitted (``--array`` is required).
* Arrays without natural block size decline with a ``not chunk-enumerable`` error.
* The inline data payload is not extracted as a binary field.
* No geometry column is emitted.
* ``GetRawBlockInfo()`` iterates chunks and this can be slow especially for remote sources.

Options
-------

.. include:: options/gdal_options/of_vector.rst

.. option:: --array <ARRAY>

    Required. Fullname of the multidimensional array within the input
    dataset (e.g. ``/temp`` or ``/HDFEOS/SWATHS/MySwath/Data Fields/MyDataField``).
    Use :program:`gdal mdim info` to list available arrays in an input
    dataset.

.. include:: options/gdal_options/oo.rst

.. include:: options/gdal_options/if_multidim_raster.rst

.. include:: options/gdal_options/co_vector.rst

.. include:: options/gdal_options/overwrite.rst


Examples
--------

.. example::
   :title: Extract chunk references from a chunked HDF5 array to GeoPackage

   .. code-block:: bash

       gdal mdim get-refs \
           --array "/HDFEOS/SWATHS/MySwath/Data Fields/MyDataField" \
           input.h5 chunks.gpkg

.. example::
   :title: Extract from a remote netCDF, output as Parquet

   .. code-block:: bash

       gdal mdim get-refs \
           --array /temp --of Parquet \
           /vsicurl/https://example.org/path/to/ocean.nc \
           ocean_chunks.parquet

.. example::
   :title: Query the resulting Parquet for specific chunk coordinates

   The output participates in SQL pushdown. The ``offset`` column must be
   quoted as it is a reserved word.

   .. code-block:: bash

       ogrinfo ocean_chunks.parquet -sql \
           'SELECT "offset", size FROM ocean_chunks
            WHERE dim_2 BETWEEN 600 AND 900
              AND dim_3 BETWEEN 1200 AND 1500'

.. example::
   :title: Native Zarr (one file per chunk; offset is always 0)

   .. code-block:: bash

       gdal mdim get-refs \
           --array /adt --of GPKG \
           'ZARR:"/vsicurl/https://example.org/store.zarr"' \
           adt_chunks.gpkg

See Also
--------

* :ref:`gdal_mdim_info` — discover the arrays and chunk geometry of a
  multidimensional dataset
* :ref:`gdal_mdim_convert` — copy or transform a multidimensional dataset
* :ref:`gdal_mdim_mosaic` — compose multiple multidimensional datasets into
  one virtual view
