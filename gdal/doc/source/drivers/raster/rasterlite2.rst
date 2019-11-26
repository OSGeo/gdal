.. _raster.rasterlite2:

================================================================================
RasterLite2 - Rasters in SQLite DB
================================================================================

.. versionadded:: 2.2

.. shortname:: SQLite

.. note:: The above short name is not a typo.
          The RasterLite2 functionality is part of the :ref:`vector.sqlite` driver.

.. build_dependencies:: libsqlite3, librasterlite2, libspatialite

The SQLite driver allows reading and writing
SQLite databases containing RasterLite2 coverages.

| Those databases can be produced by the utilities of the
  `RasterLite2 <https://www.gaia-gis.it/fossil/librasterlite2>`__
  distribution, such as rl2tools.
| The driver supports reading grayscale, paletted, RGB, multispectral
  images stored as tiles in the many compressed formats supported by
  libRasterLite2. The driver also supports reading overviews/pyramids,
  spatial reference system and spatial extent.

GDAL/OGR must be compiled with sqlite support and against librasterlite2
and libspatialite.

The driver is implemented a unified SQLite / SpatiaLite / RasterLite2
vector and raster capable driver.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Opening syntax
--------------

A RasterLite2 filename can be specified as the connection string. If the
file contains a single RasterLite2 coverage, this one will be exposed as
the GDAL dataset. If the file contains multiple coverages, each one will
be exposed as a subdataset with the syntax
RASTERLITE2:filename:coverage_name. See `the basic concepts of
RasterLite2 <https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=basic_concepts>`__.

If a coverage is made of several sections, they will be listed as
subdatasets of the coverage dataset, so as to be accessed individually.
By default, they will be exposed as a unified dataset. The syntax of
section-based dataset is
RASTERLITE2:filename:coverage_name:section_id:section_name.

Creation
--------

The driver supports creating new databases from scratch, adding new
coverages to an existing database and adding sections to an existing
coverage.

Creation options
----------------

-  **APPEND_SUBDATASET**\ =YES/NO: Whether to add the raster to the
   existing file. If set to YES, COVERAGE must be specified. Default is
   NO (ie overwrite existing file)
-  **COVERAGE**\ =string: Coverage name. If not specified, the basename
   of the output file is used.
-  **SECTION**\ =string: Section name. If not specified, the basename of
   the output file is used.
-  **COMPRESS**\ =NONE/DEFLATE/LZMA/PNG/CCITTFAX4/JPEG/WEBP/CHARS/JPEG2000:
   Compression method. Default is NONE. See the `information about
   supported
   codecs <https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=codecs>`__.
   Note that some codecs may not be available depending on how
   librasterlite2 has been built.
-  **QUALITY**\ =0 to 100: Image quality for JPEG, WEBP and JPEG2000
   compressions. Exact meaning depends on the compression method. For
   WEBP and JPEG2000, the value 100 triggers the use of their lossless
   variants.
-  **PIXEL_TYPE**\ =MONOCHROME/PALETTE/GRAYSCALE/RGB/MULTIBAND/DATAGRID:
   Raster pixel type. Determines the photometric interpretation. See the
   `information about supported pixel
   types <https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=reference_table>`__.
   The driver will automatically determine an appropriate pixel type
   given the band characteristics.
-  **BLOCKXSIZE**\ =int_value. Block width. Defaults to 512.
-  **BLOCKYSIZE**\ =int_value. Block height. Defaults to 512.
-  **NBITS**\ =1/2/4. Force bit width. This will be by default gotten
   from the NBITS metadata item in the IMAGE_STRUCTURE metadata domain
   of the source raster band.
-  **PYRAMIDIZE**\ =YES/NO. Whether to build automatically build
   relevant pyramids/overviews. Defaults to NO. Pyramids can be built
   with the BuildOverviews() / gdaladdo.

Examples
--------

-  Reading a RasterLite2 database with a single coverage:

   ::

      gdalinfo my.rl2

-  Listing the subdatasets corresponding to the coverages of a
   RasterLite2 database with several coverages:

   ::

      gdalinfo multiple_coverages.rl2

-  Reading a subdataset corresponding to a coverage:

   ::

      gdalinfo RASTERLITE2:multiple_coverages.rl2:my_coverage

-  Creating a RasterLite2 dataset from a grayscale image:

   ::

      gdal_translate -f SQLite byte.tif byte.rl2

-  Creating a RasterLite2 dataset from a RGB image, and using JPEG
   compression:

   ::

      gdal_translate -f SQLite rgb.tif rgb.rl2 -co COMPRESS=JPEG

-  Adding a RasterLite2 coverage to an existing SpatiaLite/RasterLite2
   database:

   ::

      gdal_translate -f SQLite rgb.tif rgb.rl2 -co APPEND_SUBDATASET=YES -co COVERAGE=rgb

-  Adding pyramids to a coverage:

   ::

      gdaladdo rgb.rl2 2 4 8 16

See Also
--------

-  `Rasterlite2 home
   page <https://www.gaia-gis.it/fossil/libRasterLite2/home>`__
-  :ref:`OGR SQLite driver <vector.sqlite>`
