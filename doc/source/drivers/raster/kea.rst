.. _raster.kea:

================================================================================
KEA
================================================================================

.. shortname:: KEA

.. build_dependencies:: libkea and libhdf5 libraries

GDAL can read, create and update files in the KEA format, through the libkea library.

KEA is an image file format, named after the New Zealand bird, that
provides a full implementation of the GDAL data model and is implemented
within a HDF5 file. A software library, libkea, is used to access the
file format. The format has comparable performance with existing formats
while producing smaller file sizes and is already within active use for
a number of projects within Landcare Research, New Zealand, and the
wider community.

The KEA format supports the following features of the GDAL data model:

-  Multiple-band support, with possible different datatypes. Bands can
   be added to an existing dataset with AddBand() API
-  Image blocking support
-  Reading, creation and update of data of image blocks
-  Affine geotransform, WKT projection, GCP
-  Metadata at dataset and band level
-  Per-band description
-  Per-band nodata and color interpretation
-  Per-band color table
-  Per-band RAT (Raster Attribute Table) of arbitrary size
-  Internal overviews and mask bands

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. versionadded:: 3.0

    .. supports_virtualio::

Creation options
----------------

|about-creation-options|
The following creation options are available. Some are rather esoteric
and should rarely be specified, unless the user has good knowledge of
the working of the underlying HDF5 format.

-  .. co:: IMAGEBLOCKSIZE
      :choices: <integer>
      :default: 256

      The size of each block for image data.

-  .. co:: ATTBLOCKSIZE
      :choices: <integer>
      :default: 1000

      The size of each block for attribute data.

-  .. co:: MDC_NELMTS
      :choices: <integer>
      :default: 0

      Number of elements in the meta data
      cache. Defaults to 0. See the `Data
      caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
      page of HDF5 documentation.

-  .. co:: RDCC_NELMTS
      :choices: <integer>
      :default: 512

      Number of elements in the raw data
      chunk cache. See the `Data
      caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
      page of HDF5 documentation.

-  .. co:: RDCC_NBYTES
      :choices: <bytes>
      :default: 1048576

      Total size of the raw data chunk cache, in bytes. See the `Data
      caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
      page of HDF5 documentation.

-  .. co:: RDCC_W0
      :choices: <floating_point_value between 0 and 1>
      :default: 0.75

      Preemption policy. See the `Data
      caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
      page of HDF5 documentation.

-  .. co:: SIEVE_BUF
      :choices: <integer>
      :default: 65536

      Sets the maximum size of the data sieve buffer. See
      `H5Pset_sieve_buf_size() <http://www.hdfgroup.org/HDF5/doc/RM/RM_H5P.html#Property-SetSieveBufSize>`__
      documentation

-  .. co:: META_BLOCKSIZE
      :choices: <integer>
      :default: 2048

      Sets the minimum size of metadata block allocations. See
      `H5Pset_meta_block_size() <http://www.hdfgroup.org/HDF5/doc/RM/RM_H5P.html#Property-SetMetaBlockSize>`__
      documentation

-  .. co:: DEFLATE
      :choices: [0-9]
      :default: 1

      Compression level between 0 (no
      compression) to 9 (max compression).

-  .. co:: THEMATIC
      :choices: YES, NO
      :default: NO

      If YES then all bands are set to thematic.

See Also
--------

-  `libkea GitHub
   repository <https://github.com/ubarsc/kealib>`__
-  `The KEAimage file format, by Peter Bunting and Sam Gillingham,
   published in
   Computers&Geosciences <http://www.sciencedirect.com/science/article/pii/S0098300413001015>`__
-  :ref:`HDF5 driver page <raster.hdf5>`
