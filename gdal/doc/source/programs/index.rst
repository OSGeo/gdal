.. _programs:

================================================================================
Programs
================================================================================

Raster programs
---------------

.. toctree::
   :maxdepth: 1
   :hidden:

   Common options <raster_common_options>
   gdalinfo
   gdal_translate
   gdaladdo
   gdalwarp
   gdaltindex
   gdalbuildvrt
   gdal_contour
   gdaldem
   rgb2pct
   pct2rgb
   gdalattachpct
   gdal_merge
   gdal2tiles
   gdal_rasterize
   gdaltransform
   nearblack
   gdal_retile
   gdal_grid
   gdal_proximity
   gdal_polygonize
   gdal_sieve
   gdal_fillnodata
   gdallocationinfo
   gdalsrsinfo
   gdalmove
   gdal_edit
   gdal_calc
   gdal_pansharpen
   gdal-config
   gdalmanage
   gdalcompare
   gdal_viewshed
   gdal_create

.. only:: html

    - :ref:`Common options <raster_common_options>`
    - :ref:`gdalinfo`: Lists information about a raster dataset.
    - :ref:`gdal_translate`: Converts raster data between different formats.
    - :ref:`gdaladdo`: Builds or rebuilds overview images.
    - :ref:`gdalwarp`: Image reprojection and warping utility.
    - :ref:`gdaltindex`: Builds a shapefile as a raster tileindex.
    - :ref:`gdalbuildvrt`: Builds a VRT from a list of datasets.
    - :ref:`gdal_contour`: Builds vector contour lines from a raster elevation model.
    - :ref:`gdaldem`: Tools to analyze and visualize DEMs.
    - :ref:`rgb2pct`: Convert a 24bit RGB image to 8bit paletted.
    - :ref:`pct2rgb`: Convert an 8bit paletted image to 24bit RGB.
    - :ref:`gdalattachpct`: Attach a color table to a raster file from an input file.
    - :ref:`gdal_merge`: Mosaics a set of images.
    - :ref:`gdal2tiles`: Generates directory with TMS tiles, KMLs and simple web viewers.
    - :ref:`gdal_rasterize`: Burns vector geometries into a raster.
    - :ref:`gdaltransform`: Transforms coordinates.
    - :ref:`nearblack`: Convert nearly black/white borders to black.
    - :ref:`gdal_retile`: Retiles a set of tiles and/or build tiled pyramid levels.
    - :ref:`gdal_grid`: Creates regular grid from the scattered data.
    - :ref:`gdal_proximity`: Produces a raster proximity map.
    - :ref:`gdal_polygonize`: Produces a polygon feature layer from a raster.
    - :ref:`gdal_sieve`: Removes small raster polygons.
    - :ref:`gdal_fillnodata`: Fill raster regions by interpolation from edges.
    - :ref:`gdallocationinfo`: Raster query tool
    - :ref:`gdalsrsinfo`: Lists info about a given SRS in number of formats (WKT, PROJ.4, etc.)
    - :ref:`gdalmove`: Transform georeferencing of raster file in place.
    - :ref:`gdal_edit`: Edit in place various information of an existing GDAL dataset.
    - :ref:`gdal_calc`: Command line raster calculator with numpy syntax.
    - :ref:`gdal_pansharpen`: Perform a pansharpen operation.
    - :ref:`gdal-config`: Determines various information about a GDAL installation.
    - :ref:`gdalmanage`: Identify, delete, rename and copy raster data files.
    - :ref:`gdalcompare`: Compare two images.
    - :ref:`gdal_viewshed`: Compute a visibility mask for a raster.
    - :ref:`gdal_create`: Create a raster file (without source dataset).

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

.. only:: html

    - :ref:`Common options <vector_common_options>`
    - :ref:`ogrinfo`: Lists information about an OGR-supported data source.
    - :ref:`ogr2ogr`: Converts simple features data between file formats.
    - :ref:`ogrtindex`: Creates a tileindex.
    - :ref:`ogrlineref`: Create linear reference and provide some calculations using it.
    - :ref:`ogrmerge`: Merge several vector datasets into a single one.

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
