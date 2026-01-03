Erdas Imagine (.ige) – Large Raster Spill File
=============================================

Overview
--------

The Erdas Imagine ``.ige`` format (Large Raster Spill File) is used by Erdas
Imagine for raster datasets that exceed the 4 GB file size limit.

In such cases, the dataset is split into two files:

- ``.img``: Contains the traditional Imagine metadata and dataset structure
- ``.ige``: Stores the actual raster pixel data in a separate file

This design works around 32-bit file offset limitations of the classic
Imagine (HFA) format.

GDAL Support
------------

GDAL provides **read support** for raster datasets using the ``.ige`` spill
file mechanism.

The ``.ige`` format itself is not publicly documented by Erdas. GDAL support
is based on reverse-engineering and analysis of sample datasets.

Driver Characteristics
----------------------

- **Format type**: Raster
- **Data storage**: External tiled raster data
- **Compression**: None (tiles are stored uncompressed)
- **Tile indexing**: Implicit (no explicit tile index structure)

The raster data is organized into tiles, typically 64×64 pixels in size,
stored sequentially without an index.

Internal Structure (Historical Notes)
-------------------------------------

The ``.img`` file references the external ``.ige`` file through an
``ExternalRasterDMS`` structure instead of the usual ``RasterDMS``.

The ``.ige`` file begins with a fixed ASCII magic header:

::

  ERDAS_IMG_EXTERNAL_RASTER\0

Following the header, the file contains:

- A layer stack prefix describing raster dimensions and tiling
- A valid-flags section indicating which tiles are present
- Raw, uncompressed raster tile data

Tiles are stored in row-major order. The valid-flags section is a packed bit
array where each bit corresponds to a tile’s availability.

Limitations
-----------

- The ``.ige`` format is **not officially documented**
- Write support is not guaranteed
- No compression or overviews are stored in the ``.ige`` file
- Tile indexing is implicit, which may impact random access performance

Background
----------

This documentation is based on historical GDAL documentation originally
authored by Frank Warmerdam and preserved via the Internet Archive.

The content has been restored to improve visibility of legacy format support
and to assist users encountering existing Erdas Imagine datasets.

Acknowledgements
----------------

Thanks to Mark Audin (Keyhole / EarthViewer) for providing sample datasets and
to Erdas for enabling analysis using Imagine software.
