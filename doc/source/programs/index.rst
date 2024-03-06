.. _programs:

================================================================================
Programs
================================================================================

General
-------

.. toctree::
   :maxdepth: 1
   :hidden:

   argument_syntax

.. only:: html

    - :ref:`argument_syntax`

Raster programs
---------------

.. toctree::
   :maxdepth: 1
   :hidden:

   Common options <raster_common_options>

   gdal-config
   gdal2tiles
   gdal2xyz
   gdal_calc
   gdal_contour
   gdal_create
   gdal_edit
   gdal_fillnodata
   gdal_footprint
   gdal_grid
   gdal_merge
   gdal_pansharpen
   gdal_polygonize
   gdal_proximity
   gdal_rasterize
   gdal_retile
   gdal_sieve
   gdal_translate
   gdal_viewshed
   gdaladdo
   gdalattachpct
   gdalbuildvrt
   gdalcompare
   gdaldem
   gdalinfo
   gdallocationinfo
   gdalmanage
   gdalmove
   gdalsrsinfo
   gdaltindex
   gdaltransform
   gdalwarp
   nearblack
   pct2rgb
   rgb2pct

.. only:: html

    - :ref:`Common options <raster_common_options>`
    - :ref:`gdal-config`: Determines various information about a GDAL installation.
    - :ref:`gdal2tiles`: Generates directory with TMS tiles, KMLs and simple web viewers.
    - :ref:`gdal2xyz`: Translates a raster file into xyz format.
    - :ref:`gdal_calc`: Command line raster calculator with numpy syntax.
    - :ref:`gdal_contour`: Builds vector contour lines from a raster elevation model.
    - :ref:`gdal_create`: Create a raster file (without source dataset).
    - :ref:`gdal_edit`: Edit in place various information of an existing GDAL dataset.
    - :ref:`gdal_fillnodata`: Fill raster regions by interpolation from edges.
    - :ref:`gdal_footprint`: Compute footprint of a raster.
    - :ref:`gdal_grid`: Creates regular grid from the scattered data.
    - :ref:`gdal_merge`: Mosaics a set of images.
    - :ref:`gdal_pansharpen`: Perform a pansharpen operation.
    - :ref:`gdal_polygonize`: Produces a polygon feature layer from a raster.
    - :ref:`gdal_proximity`: Produces a raster proximity map.
    - :ref:`gdal_rasterize`: Burns vector geometries into a raster.
    - :ref:`gdal_retile`: Retiles a set of tiles and/or build tiled pyramid levels.
    - :ref:`gdal_sieve`: Removes small raster polygons.
    - :ref:`gdal_translate`: Converts raster data between different formats.
    - :ref:`gdal_viewshed`: Compute a visibility mask for a raster.
    - :ref:`gdaladdo`: Builds or rebuilds overview images.
    - :ref:`gdalattachpct`: Attach a color table to a raster file from an input file.
    - :ref:`gdalbuildvrt`: Builds a VRT from a list of datasets.
    - :ref:`gdalcompare`: Compare two images.
    - :ref:`gdaldem`: Tools to analyze and visualize DEMs.
    - :ref:`gdalinfo`: Lists information about a raster dataset.
    - :ref:`gdallocationinfo`: Raster query tool
    - :ref:`gdalmanage`: Identify, delete, rename and copy raster data files.
    - :ref:`gdalmove`: Transform georeferencing of raster file in place.
    - :ref:`gdalsrsinfo`: Lists info about a given SRS in number of formats (WKT, PROJ.4, etc.)
    - :ref:`gdaltindex`: Builds an OGR-supported dataset as a raster tileindex.
    - :ref:`gdaltransform`: Transforms coordinates.
    - :ref:`gdalwarp`: Image reprojection and warping utility.
    - :ref:`nearblack`: Convert nearly black/white borders to black.
    - :ref:`pct2rgb`: Convert an 8bit paletted image to 24bit RGB.
    - :ref:`rgb2pct`: Convert a 24bit RGB image to 8bit paletted.

Multidimensional Raster programs
--------------------------------

.. toctree::
   :maxdepth: 1
   :hidden:

   gdalmdiminfo
   gdalmdimtranslate

.. only:: html

    - :ref:`gdalmdiminfo`: Reports structure and content of a multidimensional dataset.
    - :ref:`gdalmdimtranslate`: Converts multidimensional data between different formats, and perform subsetting.

Vector programs
---------------

.. toctree::
   :maxdepth: 1
   :hidden:

   Common options <vector_common_options>
   ogrinfo
   ogr2ogr
   ogrtindex
   ogrlineref
   ogrmerge
   ogr_layer_algebra

.. only:: html

    - :ref:`Common options <vector_common_options>`
    - :ref:`ogrinfo`: Lists information about an OGR-supported data source.
    - :ref:`ogr2ogr`: Converts simple features data between file formats.
    - :ref:`ogrtindex`: Creates a tileindex.
    - :ref:`ogrlineref`: Create linear reference and provide some calculations using it.
    - :ref:`ogrmerge`: Merge several vector datasets into a single one.
    - :ref:`ogr_layer_algebra`: Performs various Vector layer algebraic operations.

Geographic network programs
---------------------------

.. toctree::
   :maxdepth: 1
   :hidden:

   gnmmanage
   gnmanalyse

.. only:: html

    - :ref:`gnmmanage`: Manages networks
    - :ref:`gnmanalyse`: Analyses networks

Other utilities
---------------

.. toctree::
   :maxdepth: 1
   :hidden:

   sozip

.. only:: html

    - :ref:`sozip`: Generate a seek-optimized ZIP (SOZip) file
