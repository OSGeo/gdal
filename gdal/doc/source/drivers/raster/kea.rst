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

The following creation options are available. Some are rather esoteric
and should rarely be specified, unless the user has good knowledge of
the working of the underlying HDF5 format.

-  **IMAGEBLOCKSIZE**\ =integer_value: The size of each block for image
   data. Defaults to 256

-  **ATTBLOCKSIZE**\ =integer_value: The size of each block for
   attribute data. Defaults to 1000

-  **MDC_NELMTS**\ =integer_value: Number of elements in the meta data
   cache. Defaults to 0. See the `Data
   caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
   page of HDF5 documentation.

-  **RDCC_NELMTS**\ =integer_value: Number of elements in the raw data
   chunk cache. Defaults to 512. See the `Data
   caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
   page of HDF5 documentation.

-  **RDCC_NBYTES**\ =integer_value: Total size of the raw data chunk
   cache, in bytes. Defaults to 1048576. See the `Data
   caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
   page of HDF5 documentation.

-  **RDCC_W0**\ =floating_point_value between 0 and 1: Preemption
   policy. Defaults to 0.75. See the `Data
   caching <http://www.hdfgroup.org/HDF5/doc/H5.user/Caching.html>`__
   page of HDF5 documentation.

-  **SIEVE_BUF**\ =integer_value: Sets the maximum size of the data
   sieve buffer. Defaults to 65536. See
   `H5Pset_sieve_buf_size() <http://www.hdfgroup.org/HDF5/doc/RM/RM_H5P.html#Property-SetSieveBufSize>`__
   documentation

-  **META_BLOCKSIZE**\ =integer_value: Sets the minimum size of metadata
   block allocations. Defaults to 2048. See
   `H5Pset_meta_block_size() <http://www.hdfgroup.org/HDF5/doc/RM/RM_H5P.html#Property-SetMetaBlockSize>`__
   documentation

-  **DEFLATE**\ =integer_value: Compression level between 0 (no
   compression) to 9 (max compression). Defaults to 1

-  **THEMATIC**\ =YES/NO: If YES then all bands are set to thematic.
   Defaults to NO

See Also
--------

-  `libkea GitHub
   repository <https://github.com/ubarsc/kealib>`__
-  `The KEAimage file format, by Peter Bunting and Sam Gillingham,
   published in
   Computers&Geosciences <http://www.sciencedirect.com/science/article/pii/S0098300413001015>`__
-  :ref:`HDF5 driver page <raster.hdf5>`
