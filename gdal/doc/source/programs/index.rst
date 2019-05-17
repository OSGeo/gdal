.. _programs:

================================================================================
Programs (TODO)
================================================================================

Raster programs
---------------

.. toctree::
   :maxdepth: 1
   :hidden:

   raster_common_options
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

- :ref:`Common options <raster_common_options>`
- :ref:`gdalinfo`: Lists information about a raster dataset.
- :ref:`gdal_translate`: Converts raster data between different formats.
- :ref:`gdaladdo`: Builds or rebuilds overview images.
- :ref:`gdalwarp`: Image reprojection and warping utility.
- :ref:`gdaltindex`: Builds a shapefile as a raster tileindex.
- :ref:`gdalbuildvrt`: Builds a VRT from a list of datasets.
- :ref:`gdal_contour`: Builds vector contour lines from a raster elevation model.
- :ref:`gdaldem`: Tools to analyze and visualize DEMs.
- :ref:`rgb2pct`
- :ref:`pct2rgb`
- :ref:`gdal_merge`
- :ref:`gdal2tiles`
- :ref:`gdal_rasterize`
- :ref:`gdaltransform`
- :ref:`nearblack`
- :ref:`gdal_retile`
- :ref:`gdal_grid`
- :ref:`gdal_proximity`
- :ref:`gdal_polygonize`
- :ref:`gdal_sieve`: Removes small raster polygons.
- :ref:`gdal_fillnodata`: Fill raster regions by interpolation from edges.
- :ref:`gdallocationinfo`
- :ref:`gdalsrsinfo`
- :ref:`gdalmove`
- :ref:`gdal_edit`: Edit in place various information of an existing GDAL dataset.
- :ref:`gdal_calc`: Command line raster calculator with numpy syntax.
- :ref:`gdal_pansharpen`: Perform a pansharpen operation.
- :ref:`gdal-config`: Determines various information about a GDAL installation.
- :ref:`gdalmanage`: Identify, delete, rename and copy raster data files.
- :ref:`gdalcompare`: Compare two images.


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

- :ref:`Common options <vector_common_options>`
- :ref:`ogrinfo`: Lists information about an OGR-supported data source.
- :ref:`ogr2ogr`: Converts simple features data between file formats.
- :ref:`ogrtindex`: Creates a tileindex.
- :ref:`ogrlineref`
- :ref:`ogrmerge`: Merge several vector datasets into a single one.

Geographic network programs
---------------------------

.. toctree::
   :maxdepth: 1

   gnmmanage
   gnmanalyse
