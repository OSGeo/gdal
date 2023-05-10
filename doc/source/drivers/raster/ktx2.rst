.. _raster.ktx2:

================================================================================
KTX2
================================================================================

.. versionadded:: 3.6

.. shortname:: KTX2

.. build_dependencies:: Basis Universal


From https://github.com/BinomialLLC/basis_universal,

    Basis Universal is a "supercompressed" GPU texture data interchange system
    that supports two highly compressed intermediate file formats (.basis or
    the .KTX2 open standard from the Khronos Group) that can be quickly
    transcoded to a very wide variety of GPU compressed and uncompressed pixel
    formats.

This driver handles textures with the .ktx2 extension. For .basic, refer to the
:ref:`raster.basisu` driver. Note that this driver does *not* handle KTX(1)
files.

Note that while the file format supports direct transcoding to other GPU pixel
formats, this GDAL driver supports only conversion between uncompressed RGB(A)
data and Basis Universal textures.

When a file is made of several images, they are exposed as subdataset, with
the `KTX2:filename:layer_idx:face_idx` syntax.

Mipmaps levels are exposed as GDAL overviews.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_virtualio::

Creation options
----------------

The format supports 2 modes: a high quality mode which is internally based off
the `UASTC compressed texture format <https://richg42.blogspot.com/2020/01/uastc-block-format-encoding.html>`_,
and the original lower quality mode which is based off a subset of ETC1 called "ETC1S".
The default is ETC1S.
Only input of type Byte and with 1 (grey), 2 (grey+alpha), 3 (RGB) or 4 (RGB + alpha)
bands is supported.
Refer to https://github.com/BinomialLLC/basis_universal for more details on those
modes and their options.

The following creation options are available:

- .. co:: COMPRESSION
     :choices: ETC1S, UASTC
     :default: ETC1S

- .. co:: UASTC_SUPER_COMPRESSION
     :choices: ZSTD, NONE
     :default: ZSTD

     "Super" compression to apply. Only valid when :co:`COMPRESSION=UASTC`.

- .. co:: UASTC_LEVEL
     :choices: 0, 1, 2, 3, 4
     :default: 2

     The higher value,
     the higher the quality but the slower computing time. 4 is impractically slow.
     Only valid when :co:`COMPRESSION=UASTC`.

- .. co:: UASTC_RDO_LEVEL
     :choices: <float>
     :default: 1

     Rate distortion optimization level. The lower value,
     the higher the quality, but the larger the file size.
     Usual range is [0.2,3]. Only valid when :co:`COMPRESSION=UASTC`.

- .. co:: ETC1S_LEVEL
     :choices: 0, 1, 2, 3, 4, 5, 6
     :default: 1

     The higher value,
     the higher the quality but the slower computing time.
     Only valid when :co:`COMPRESSION=ETC1S`.

- .. co:: ETC1S_QUALITY_LEVEL
     :choices: 1-255
     :default: 128

     The higher
     value, the higher the quality, but the larger the file size.
     Only valid when :co:`COMPRESSION=ETC1S`.

- .. co:: ETC1S_MAX_ENDPOINTS_CLUSTERS
     :choices: 1-16128

     Maximum number of endpoint clusters.
     When set, :co:`ETC1S_MAX_SELECTOR_CLUSTERS` must also be set.
     Mutually exclusive with :co:`ETC1S_QUALITY_LEVEL`.
     Only valid when :co:`COMPRESSION=ETC1S`.

- .. co:: ETC1S_MAX_SELECTOR_CLUSTERS
     :choices: 1-16128.

     Maximum number of selector clusters.
     When set, :co:`ETC1S_MAX_ENDPOINTS_CLUSTERS` must also be set.
     Mutually exclusive with :co:`ETC1S_QUALITY_LEVEL`.
     Only valid when :co:`COMPRESSION=ETC1S`.

- .. co:: NUM_THREADS
     :choices: <integer>

     Defaults to the maximum number of virtual CPUs
     available. Can also be controlled with the :config:`GDAL_NUM_THREADS`
     configuration option

- .. co:: MIPMAP
     :choices: YES, NO
     :default: NO

      Whether to enable MIPMAP generation.

- .. co:: COLORSPACE
     :choices: PERCEPTUAL_SRGB, LINEAR
     :default: PERCEPTUAL_SRGB

     For non-photometric input, use LINEAR to avoid unnecessary artifacts.


Build instructions
------------------

Building basisu as a library requires currently building the `cmake` branch of the
https://github.com/rouault/basis_universal/tree/cmake fork.

.. code-block::

    git clone -b cmake https://github.com/rouault/basis_universal
    cd basis_universal
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/install-basisu -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
    cmake --build . --config Release --target install

Once basisu is built, GDAL CMake options must be configured by pointing the
basisu install prefix in the ``CMAKE_PREFIX_PATH`` variable or ``basisu_ROOT`` variable.
