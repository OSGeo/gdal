.. _programs:

================================================================================
Programs
================================================================================

"gdal" application
------------------

Starting with GDAL 3.11, parts of the GDAL utilities are available from a new
single :program:`gdal` program that accepts commands and subcommands.

.. warning::

    The :program:`gdal` command is provisionally provided as an alternative
    interface to GDAL and OGR command line utilities. The project reserves the
    right to modify, rename, reorganize, and change the behavior of the utility
    until it is officially frozen via PSC vote in a future major GDAL release.
    The utility needs time to mature, benefit from incremental feedback, and
    explore enhancements without carrying the burden of full backward compatibility.
    Your usage of it should have no expectation of compatibility until that time.


.. toctree::
   :maxdepth: 1
   :hidden:

   migration_guide_to_gdal_cli
   gdal_cli_from_python
   gdal_cli_gdalg
   gdal
   gdal_info
   gdal_convert
   gdal_driver_gti_create
   gdal_mdim
   gdal_mdim_info
   gdal_mdim_convert
   gdal_raster
   gdal_raster_info
   gdal_raster_aspect
   gdal_raster_astype
   gdal_raster_calc
   gdal_raster_clip
   gdal_raster_color_map
   gdal_raster_contour
   gdal_raster_convert
   gdal_raster_create
   gdal_raster_edit
   gdal_raster_hillshade
   gdal_raster_index
   gdal_raster_mosaic
   gdal_raster_overview
   gdal_raster_overview_add
   gdal_raster_overview_delete
   gdal_raster_pipeline
   gdal_raster_reproject
   gdal_raster_resize
   gdal_raster_roughness
   gdal_raster_scale
   gdal_raster_select
   gdal_raster_slope
   gdal_raster_stack
   gdal_raster_tpi
   gdal_raster_tri
   gdal_raster_unscale
   gdal_vector
   gdal_vector_info
   gdal_vector_clip
   gdal_vector_convert
   gdal_vector_edit
   gdal_vector_filter
   gdal_vector_geom
   gdal_vector_geom_set_type
   gdal_vector_geom_explode_collections
   gdal_vector_geom_make_valid
   gdal_vector_geom_segmentize
   gdal_vector_geom_simplify
   gdal_vector_geom_buffer
   gdal_vector_geom_swap_xy
   gdal_vector_grid
   gdal_vector_pipeline
   gdal_vector_rasterize
   gdal_vector_reproject
   gdal_vector_select
   gdal_vector_sql
   gdal_vfs
   gdal_vfs_copy
   gdal_vfs_delete
   gdal_vfs_list

.. only:: html

    - :ref:`migration_guide_to_gdal_cli`: Migration guide to "gdal" command line interface
    - :ref:`gdal_cli_from_python`: How to use "gdal" CLI algorithms from Python
    - :ref:`gdal_cli_gdalg`: .gdalg files to replay serialized "gdal" commands
    - :ref:`gdal_program`: Main "gdal" entry point
    - :ref:`gdal_info_command`: Get information on a dataset
    - :ref:`gdal_convert_command`: Convert a dataset
    - :ref:`gdal_driver_gti_create_subcommand`: Create an index of raster datasets compatible of the GDAL Tile Index (GTI) driver
    - :ref:`gdal_mdim_command`: Entry point for multidimensional commands
    - :ref:`gdal_mdim_info_subcommand`: Get information on a multidimensional dataset
    - :ref:`gdal_mdim_convert_subcommand`: Convert a multidimensional dataset
    - :ref:`gdal_raster_command`: Entry point for raster commands
    - :ref:`gdal_raster_info_subcommand`: Get information on a raster dataset
    - :ref:`gdal_raster_aspect_subcommand`: Generate an aspect map.
    - :ref:`gdal_raster_astype_subcommand`: Modify the data type of bands of a raster dataset
    - :ref:`gdal_raster_calc_subcommand`: Perform raster algebra
    - :ref:`gdal_raster_clip_subcommand`: Clip a raster dataset
    - :ref:`gdal_raster_color_map_subcommand`: Generate a RGB or RGBA dataset from a single band, using a color map
    - :ref:`gdal_raster_convert_subcommand`: Convert a raster dataset
    - :ref:`gdal_raster_contour_subcommand`: Builds vector contour lines from a raster elevation model
    - :ref:`gdal_raster_create_subcommand`: Create a new raster dataset
    - :ref:`gdal_raster_edit_subcommand`: Edit in place a raster dataset
    - :ref:`gdal_raster_hillshade_subcommand`: Generate a shaded relief map
    - :ref:`gdal_raster_index_subcommand`: Create a vector index of raster datasets
    - :ref:`gdal_raster_mosaic_subcommand`: Build a mosaic, either virtual (VRT) or materialized.
    - :ref:`gdal_raster_overview_subcommand`: Manage overviews of a raster dataset
    - :ref:`gdal_raster_overview_add_subcommand`: Add overviews to a raster dataset
    - :ref:`gdal_raster_overview_delete_subcommand`: Remove overviews of a raster dataset
    - :ref:`gdal_raster_pipeline_subcommand`: Process a raster dataset
    - :ref:`gdal_raster_reproject_subcommand`: Reproject a raster dataset
    - :ref:`gdal_raster_resize_subcommand`: Resize a raster dataset without changing the georeferenced extents
    - :ref:`gdal_raster_roughness_subcommand`: Generate a roughness map.
    - :ref:`gdal_raster_scale_subcommand`: Scale the values of the bands of a raster dataset.
    - :ref:`gdal_raster_select_subcommand`: Select a subset of bands from a raster dataset.
    - :ref:`gdal_raster_slope_subcommand`: Generate a slope map.
    - :ref:`gdal_raster_stack_subcommand`: Combine together input bands into a multi-band output, either virtual (VRT) or materialized.
    - :ref:`gdal_raster_tpi_subcommand`: Generate a Topographic Position Index (TPI) map.
    - :ref:`gdal_raster_tri_subcommand`: Generate a Terrain Ruggedness Index (TRI) map.
    - :ref:`gdal_raster_unscale_subcommand`: Convert scaled values of a raster dataset into unscaled values.
    - :ref:`gdal_vector_command`: Entry point for vector commands
    - :ref:`gdal_vector_info_subcommand`: Get information on a vector dataset
    - :ref:`gdal_vector_clip_subcommand`: Clip a vector dataset
    - :ref:`gdal_vector_edit_subcommand`: Edit metadata of a vector dataset
    - :ref:`gdal_vector_filter_subcommand`: Filter a vector dataset
    - :ref:`gdal_vector_geom_subcommand`: Geometry operations on a vector dataset
    - :ref:`gdal_vector_geom_set_type_subsubcommand`: Modify the geometry type of a vector dataset
    - :ref:`gdal_vector_geom_explode_collections_subsubcommand`: Explode geometries of type collection of a vector dataset
    - :ref:`gdal_vector_geom_make_valid_subsubcommand`: Fix validity of geometries of a vector dataset
    - :ref:`gdal_vector_geom_segmentize_subsubcommand`: Segmentize geometries of a vector dataset
    - :ref:`gdal_vector_geom_simplify_subsubcommand`: Simplify geometries of a vector dataset
    - :ref:`gdal_vector_geom_buffer_subsubcommand`: Compute a buffer around geometries of a vector dataset
    - :ref:`gdal_vector_geom_swap_xy_subsubcommand`: Swap X and Y coordinates of geometries of a vector dataset
    - :ref:`gdal_vector_grid_subcommand`: Create a regular grid from scattered points
    - :ref:`gdal_vector_convert_subcommand`: Convert a vector dataset
    - :ref:`gdal_vector_pipeline_subcommand`: Process a vector dataset
    - :ref:`gdal_vector_reproject_subcommand`: Reproject a vector dataset
    - :ref:`gdal_vector_select_subcommand`: Select a subset of fields from a vector dataset.
    - :ref:`gdal_vector_rasterize_subcommand`: Burns vector geometries into a raster
    - :ref:`gdal_vector_sql_subcommand`: Apply SQL statement(s) to a dataset
    - :ref:`gdal_vfs_command`: Entry point for GDAL Virtual file system (VSI) commands
    - :ref:`gdal_vfs_copy_subcommand`: Copy files located on GDAL Virtual file systems (VSI)
    - :ref:`gdal_vfs_delete_subcommand`: Delete files located on GDAL Virtual file systems (VSI)
    - :ref:`gdal_vfs_list_subcommand`: List files of one of the GDAL Virtual file systems (VSI)


"Traditional" applications
--------------------------

General
+++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   argument_syntax

.. only:: html

    - :ref:`argument_syntax`


Raster programs
+++++++++++++++

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
   gdalenhance
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
    - :ref:`gdalenhance`: Enhance an image with LUT-based contrast enhancement
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
++++++++++++++++++++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdalmdiminfo
   gdalmdimtranslate

.. only:: html

    - :ref:`gdalmdiminfo`: Reports structure and content of a multidimensional dataset.
    - :ref:`gdalmdimtranslate`: Converts multidimensional data between different formats, and perform subsetting.

Vector programs
+++++++++++++++

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
+++++++++++++++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gnmmanage
   gnmanalyse

.. only:: html

    - :ref:`gnmmanage`: Manages networks
    - :ref:`gnmanalyse`: Analyses networks

Other utilities
+++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   sozip

.. only:: html

    - :ref:`sozip`: Generate a seek-optimized ZIP (SOZip) file
