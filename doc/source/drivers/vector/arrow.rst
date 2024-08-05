.. _vector.arrow:

(Geo)Arrow IPC File Format / Stream
===================================

.. versionadded:: 3.5

.. shortname:: Arrow

.. build_dependencies:: Apache Arrow C++ library

The Arrow IPC File Format (Feather) is a portable file format for storing Arrow
tables or data frames (from languages like Python or R) that utilizes the Arrow
IPC format internally.

The driver supports the 2 variants of the format:

- File or Random Access format, also known as Feather:
  for serializing a fixed number of record batches.
  Random access is required to read such files, but they can be generated using
  a streaming-only capable file. The recommended extension for such file is ``.arrow``

- Streaming IPC format: for sending an arbitrary length sequence of record batches.
  The format must generally be processed from start to end, and does not require
  random access. That format is not generally materialized as a file. If it is,
  the recommended extension is ``.arrows`` (with a trailing s). But the
  driver can support regular files as well as the /vsistdin/ and /vsistdout/ streaming files.
  On opening, it might difficult for the driver to detect that the content is
  specifically a Arrow IPC stream, especially if the extension is not ``.arrows``,
  and the metadata section is large.
  Prefixing the filename with ``ARROW_IPC_STREAM:`` (e.g "ARROW_IPC_STREAM:/vsistdin/")
  will cause the driver to unconditionally open the file as a streaming IPC format.
  Alternatively, starting with GDAL 3.10, specifying the ``-if ARROW`` option to
  command line utilities accepting it, or ``ARROW`` as the only value of the
  ``papszAllowedDrivers`` of :cpp:func:`GDALOpenEx`, also forces the driver to
  recognize the passed filename.


This driver also supports geometry columns using the GeoArrow specification.

.. note:: The driver should be considered experimental as the GeoArrow specification is not finalized yet.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation issues
---------------

The driver supports creating only a single layer in a dataset.

Layer creation options
----------------------

|about-layer-creation-options|
The following layer creation options are supported:

- .. lco:: COMPRESSION
     :choices: NONE, ZSTD, LZ4

     Compression method.
     Available values depend on how the Arrow library was compiled.
     Defaults to LZ4 when available, otherwise NONE.

- .. lco:: FORMAT
     :choices: FILE, STREAM

     Variant of the file format. See introduction paragraph
     for the difference between both. Defaults to FILE, unless the filename is
     "/vsistdout/" or its extension is ".arrows", in which case STREAM is used.

- .. lco:: GEOMETRY_ENCODING
     :choices: GEOARROW, WKB, WKT, GEOARROW_INTERLEAVED
     :default: GEOARROW

     Geometry encoding.
     As of GDAL 3.9, GEOARROW uses the GeoArrow "struct" based
     encodings (where points are modeled as a struct field with a x and y subfield,
     lines are modeled as a list of such points, etc.).
     The GEOARROW_INTERLEAVED option has been renamed in GDAL 3.9 from what was
     named GEOARROW in previous versions, and uses an encoding where points uses
     a FixedSizedList of (x,y), lines a variable-size list of such
     FixedSizedList of points, etc.

- .. lco:: BATCH_SIZE
     :choices: <integer>
     :default: 65536

     Maximum number of rows per record batch.

- .. lco:: GEOMETRY_NAME
     :default: geometry

     Name of geometry column.

- .. lco:: FID

     Name of the FID (Feature Identifier) column to create. If
     none is specified, no FID column is created. Note that if using ogr2ogr with
     the Arrow driver as the target driver and a source layer that has a named
     FID column, this FID column name will be automatically used to set the FID
     layer creation option of the Arrow driver (unless ``-lco FID=`` is used to
     set an empty name)

Conda-forge package
-------------------

The driver can be installed as a plugin for the ``libgdal`` conda-forge package with:

::

    conda install -c conda-forge libgdal-arrow-parquet

Standalone plugin compilation
-----------------------------

.. versionadded:: 3.10

While this driver may be built as part of a whole GDAL build, either in libgdal
itself, or as a plugin, it is also possible to only build this driver as a plugin,
against an already built libgdal.

The version of the GDAL sources used to build the driver must match the version
of the libgdal it is built against.

For example, from a "build_arrow" directory under the root of the GDAL source tree:

::

    cmake -S ../ogr/ogrsf_frmts/parquet -DCMAKE_PREFIX_PATH=/path/to/GDAL_installation_prefix -DArrow_DIR=/path/to/lib/cmake/Arrow
    cmake --build .


Note that such a plugin, when used against a libgdal not aware of it, will be
systematically loaded at GDAL driver initialization time, and will not benefit from
`deferred plugin loading capabilities <rfc-96>`. For that, libgdal itself must be built with the
CMake variable OGR_REGISTER_DRIVER_ARROW_FOR_LATER_PLUGIN=ON set.

Arrow VSI file system
---------------------

.. versionadded:: 3.10

Starting with GDAL 3.10 and Arrow 16.0, any GDAL Virtual File System can be
used (in a read-only context) wherever the Arrow C++ library expects a URI, in
particular outside of the context of the OGR Arrow driver, by:

- loading the libgdal.so/dll library (or the ogr_Arrow.so/dll plugin library if
  the Arrow driver is built as a library) with the arrow::fs::LoadFileSystemFactories()
  function (cf `Defining new filesystems <https://arrow.apache.org/docs/cpp/io.html#defining-new-filesystems>`__)
  Note: if the Arrow driver is fully loaded, e.g. by querying
  GetGDALDriverManager()->GetDriverByName("ARROW")->GetMetadata(), the Arrow VSI
  file system will be also registered.

- Prefixing any GDAL file name with the ``vsi://`` URI scheme prefix. In addition
  to any potential vsi prefix in the GDAL file name. So the ``/vsicurl/http://example.com``
  GDAL file name becomes the ``vsi:///vsicurl/http://example.com`` Arrow URI.

Links
-----

- `Feather File Format <https://arrow.apache.org/docs/python/feather.html>`__

- `GeoArrow specification <https://github.com/geopandas/geo-arrow-spec>`__

-  Related driver: :ref:`Parquet driver <vector.parquet>`
