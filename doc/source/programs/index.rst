.. _programs:

================================================================================
Programs
================================================================================

.. _programs_gdal:

"gdal" application
------------------

.. versionadded:: 3.11

Starting with GDAL 3.11, parts of the GDAL utilities are available from a new
single :program:`gdal` program that accepts commands and subcommands.

As an introduction, you can follow the webinar given on June 3, 2025 about the
GDAL Command Line Interface Modernization as a `PDF slide deck <https://download.osgeo.org/gdal/presentations/GDAL%20CLI%20Modernization.pdf>`__
or the `recording of the video <https://www.youtube.com/watch?v=ZKdrYm3TiBU>`__.

.. warning::

    The :program:`gdal` command is provisionally provided as an alternative
    interface to GDAL and OGR command line utilities. The project reserves the
    right to modify, rename, reorganize, and change the behavior of the utility
    until it is officially frozen via PSC vote in a future major GDAL release.
    The utility needs time to mature, benefit from incremental feedback, and
    explore enhancements without carrying the burden of full backward compatibility.
    Your usage of it should have no expectation of compatibility until that time.

General
+++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal
   gdal_syntax
   migration_guide_to_gdal_cli
   gdal_bash_completion
   gdal_cli_from_c
   gdal_cli_from_cpp
   gdal_cli_from_python
   gdal_cli_gdalg

.. only:: html

    - :ref:`gdal_program`: Main ``gdal`` entry point
    - :ref:`gdal_syntax`: Syntax for commands of ``gdal`` program
    - :ref:`migration_guide_to_gdal_cli`: Migration guide to ``gdal`` command line interface
    - :ref:`gdal_bash_completion`: Bash completion for ``gdal``
    - :ref:`gdal_cli_from_c`: How to use ``gdal`` CLI algorithms from C
    - :ref:`gdal_cli_from_cpp`: How to use ``gdal`` CLI algorithms from C++
    - :ref:`gdal_cli_from_python`: How to use ``gdal`` CLI algorithms from Python
    - :ref:`gdal_cli_gdalg`: .gdalg files to replay serialized ``gdal`` commands

Commands working with raster or vector inputs
+++++++++++++++++++++++++++++++++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal_info
   gdal_convert
   gdal_pipeline

.. only:: html

    - :ref:`gdal_info`: Get information on a dataset
    - :ref:`gdal_convert`: Convert a dataset
    - :ref:`gdal_pipeline`: Process a dataset applying several steps

Raster commands
+++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal_raster
   gdal_raster_info
   gdal_raster_as_features
   gdal_raster_aspect
   gdal_raster_blend
   gdal_raster_calc
   gdal_raster_clean_collar
   gdal_raster_clip
   gdal_raster_color_map
   gdal_raster_contour
   gdal_raster_compare
   gdal_raster_convert
   gdal_raster_create
   gdal_raster_edit
   gdal_raster_footprint
   gdal_raster_fill_nodata
   gdal_raster_hillshade
   gdal_raster_index
   gdal_raster_materialize
   gdal_raster_mosaic
   gdal_raster_neighbors
   gdal_raster_nodata_to_alpha
   gdal_raster_overview
   gdal_raster_overview_add
   gdal_raster_overview_delete
   gdal_raster_overview_refresh
   gdal_raster_pansharpen
   gdal_raster_pipeline
   gdal_raster_pixel_info
   gdal_raster_polygonize
   gdal_raster_proximity
   gdal_raster_reclassify
   gdal_raster_reproject
   gdal_raster_resize
   gdal_raster_rgb_to_palette
   gdal_raster_roughness
   gdal_raster_scale
   gdal_raster_select
   gdal_raster_set_type
   gdal_raster_slope
   gdal_raster_sieve
   gdal_raster_stack
   gdal_raster_tile
   gdal_raster_tpi
   gdal_raster_tri
   gdal_raster_unscale
   gdal_raster_update
   gdal_raster_viewshed
   gdal_raster_zonal_stats

.. only:: html

    Single operations:

    - :ref:`gdal_raster`: Entry point for raster commands
    - :ref:`gdal_raster_info`: Get information on a raster dataset
    - :reF:`gdal_raster_as_features`: Create features representing raster pixels
    - :ref:`gdal_raster_aspect`: Generate an aspect map.
    - :ref:`gdal_raster_blend`: Blend/compose two raster datasets
    - :ref:`gdal_raster_calc`: Perform raster algebra
    - :ref:`gdal_raster_clean_collar`: Clean the collar of a raster dataset, removing noise
    - :ref:`gdal_raster_clip`: Clip a raster dataset
    - :ref:`gdal_raster_color_map`: Use a grayscale raster to replace the intensity of a RGB/RGBA dataset
    - :ref:`gdal_raster_compare`: Compare two raster datasets
    - :ref:`gdal_raster_convert`: Convert a raster dataset
    - :ref:`gdal_raster_contour`: Builds vector contour lines from a raster elevation model
    - :ref:`gdal_raster_create`: Create a new raster dataset
    - :ref:`gdal_raster_edit`: Edit in place a raster dataset
    - :ref:`gdal_raster_footprint`: Compute the footprint of a raster dataset.
    - :ref:`gdal_raster_fill_nodata`: Fill raster regions by interpolation from edges.
    - :ref:`gdal_raster_hillshade`: Generate a shaded relief map
    - :ref:`gdal_raster_index`: Create a vector index of raster datasets
    - :ref:`gdal_raster_materialize`: Materialize a piped dataset on disk to increase the efficiency of the following steps
    - :ref:`gdal_raster_mosaic`: Build a mosaic, either virtual (VRT) or materialized.
    - :ref:`gdal_raster_neighbors`: Compute the value of each pixel from its neighbors (focal statistics).
    - :ref:`gdal_raster_nodata_to_alpha`: Replace nodata value(s) with an alpha band
    - :ref:`gdal_raster_overview`: Manage overviews of a raster dataset
    - :ref:`gdal_raster_overview_add`: Add overviews to a raster dataset
    - :ref:`gdal_raster_overview_delete`: Remove overviews of a raster dataset
    - :ref:`gdal_raster_overview_refresh`: Refresh overviews
    - :ref:`gdal_raster_pansharpen`: Perform a pansharpen operation
    - :ref:`gdal_raster_polygonize`: Create a polygon feature dataset from a raster band
    - :ref:`gdal_raster_pixel_info`: Return information on a pixel of a raster dataset
    - :ref:`gdal_raster_rgb_to_palette`: Convert a RGB image into a pseudo-color / paletted image
    - :ref:`gdal_raster_reclassify`: Reclassify a raster dataset
    - :ref:`gdal_raster_reproject`: Reproject a raster dataset
    - :ref:`gdal_raster_resize`: Resize a raster dataset without changing the georeferenced extents
    - :ref:`gdal_raster_roughness`: Generate a roughness map.
    - :ref:`gdal_raster_scale`: Scale the values of the bands of a raster dataset.
    - :ref:`gdal_raster_select`: Select a subset of bands from a raster dataset.
    - :ref:`gdal_raster_set_type`: Modify the data type of bands of a raster dataset
    - :ref:`gdal_raster_sieve`: Remove small raster polygons.
    - :ref:`gdal_raster_slope`: Generate a slope map.
    - :ref:`gdal_raster_stack`: Combine together input bands into a multi-band output, either virtual (VRT) or materialized.
    - :ref:`gdal_raster_tile`: Generate tiles in separate files from a raster dataset.
    - :ref:`gdal_raster_tpi`: Generate a Topographic Position Index (TPI) map.
    - :ref:`gdal_raster_tri`: Generate a Terrain Ruggedness Index (TRI) map.
    - :ref:`gdal_raster_unscale`: Convert scaled values of a raster dataset into unscaled values.
    - :ref:`gdal_raster_update`: Update the destination raster with the content of the input one.
    - :ref:`gdal_raster_viewshed`: Compute the viewshed of a raster dataset.
    - :ref:`gdal_raster_zonal_stats`: Compute raster zonal statistics

    Pipelines:

    - :ref:`gdal_raster_pipeline`: Process a raster dataset applying several steps

Vector commands
+++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal_vector
   gdal_vector_buffer
   gdal_vector_check_coverage
   gdal_vector_check_geometry
   gdal_vector_clean_coverage
   gdal_vector_clip
   gdal_vector_concat
   gdal_vector_convert
   gdal_vector_edit
   gdal_vector_filter
   gdal_vector_info
   gdal_vector_explode_collections
   gdal_vector_grid
   gdal_vector_index
   gdal_vector_layer_algebra
   gdal_vector_make_point
   gdal_vector_make_valid
   gdal_vector_materialize
   gdal_vector_partition
   gdal_vector_pipeline
   gdal_vector_rasterize
   gdal_vector_reproject
   gdal_vector_select
   gdal_vector_segmentize
   gdal_vector_set_field_type
   gdal_vector_set_geom_type
   gdal_vector_sort
   gdal_vector_simplify
   gdal_vector_simplify_coverage
   gdal_vector_sql
   gdal_vector_swap_xy
   gdal_vector_update

.. only:: html

    Single operations:

    - :ref:`gdal_vector`: Entry point for vector commands
    - :ref:`gdal_vector_buffer`: Compute a buffer around geometries of a vector dataset
    - :ref:`gdal_vector_check_coverage`: Check a polygon coverage for validity
    - :ref:`gdal_vector_check_geometry`: Check a dataset for invalid or non-simple geometries
    - :ref:`gdal_vector_clean_coverage`: Remove gaps and overlaps in a polygon dataset
    - :ref:`gdal_vector_clip`: Clip a vector dataset
    - :ref:`gdal_vector_concat`: Concatenate vector datasets
    - :ref:`gdal_vector_convert`: Convert a vector dataset
    - :ref:`gdal_vector_edit`: Edit metadata of a vector dataset
    - :ref:`gdal_vector_explode_collections`: Explode geometries of type collection of a vector dataset
    - :ref:`gdal_vector_filter`: Filter a vector dataset
    - :ref:`gdal_vector_grid`: Create a regular grid from scattered points
    - :ref:`gdal_vector_info`: Get information on a vector dataset
    - :ref:`gdal_vector_index`: Create a vector index of vector datasets
    - :ref:`gdal_vector_layer_algebra`: Perform algebraic operation between 2 layers.
    - :ref:`gdal_vector_make_point`: Create point geometries from coordinate fields
    - :ref:`gdal_vector_make_valid`: Fix validity of geometries of a vector dataset
    - :ref:`gdal_vector_materialize`: Materialize a piped dataset on disk to increase the efficiency of the following steps
    - :ref:`gdal_vector_partition`: Partition a vector dataset into multiple files
    - :ref:`gdal_vector_rasterize`: Burns vector geometries into a raster
    - :ref:`gdal_vector_reproject`: Reproject a vector dataset
    - :ref:`gdal_vector_segmentize`: Segmentize geometries of a vector dataset
    - :ref:`gdal_vector_select`: Select a subset of fields from a vector dataset.
    - :ref:`gdal_vector_set_field_type`: Modify the type of a field of a vector dataset
    - :ref:`gdal_vector_set_geom_type`: Modify the geometry type of a vector dataset
    - :ref:`gdal_vector_simplify`: Simplify geometries of a vector dataset
    - :ref:`gdal_vector_simplify_coverage`: Simplify shared boundaries of a polygonal vector dataset
    - :ref:`gdal_vector_sort`: Spatially sort a vector dataset
    - :ref:`gdal_vector_sql`: Apply SQL statement(s) to a dataset
    - :ref:`gdal_vector_swap_xy`: Swap X and Y coordinates of geometries of a vector dataset
    - :ref:`gdal_vector_update`: Update an existing vector dataset with an input vector dataset

    Pipelines:

    - :ref:`gdal_vector_pipeline`: Process a vector dataset applying several steps

Multidimensional raster commands
++++++++++++++++++++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal_mdim
   gdal_mdim_info
   gdal_mdim_convert
   gdal_mdim_mosaic

.. only:: html

    - :ref:`gdal_mdim`: Entry point for multidimensional commands
    - :ref:`gdal_mdim_info`: Get information on a multidimensional dataset
    - :ref:`gdal_mdim_convert`: Convert a multidimensional dataset
    - :ref:`gdal_mdim_mosaic`: Build a mosaic, either virtual (VRT) or materialized, from multidimensional datasets.

Dataset management commands
+++++++++++++++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal_dataset
   gdal_dataset_identify
   gdal_dataset_check
   gdal_dataset_copy
   gdal_dataset_rename
   gdal_dataset_delete

.. only:: html

    - :ref:`gdal_dataset`: Entry point for dataset management commands
    - :ref:`gdal_dataset_identify`: Identify driver opening dataset(s)
    - :ref:`gdal_dataset_check`: Check whether there are errors when reading the content of a dataset.
    - :ref:`gdal_dataset_copy`: Copy files of a dataset.
    - :ref:`gdal_dataset_rename`: Rename files of a dataset.
    - :ref:`gdal_dataset_delete`: Delete dataset(s)

Virtual System Interface (VSI) commands
+++++++++++++++++++++++++++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal_vsi
   gdal_vsi_copy
   gdal_vsi_delete
   gdal_vsi_list
   gdal_vsi_move
   gdal_vsi_sync
   gdal_vsi_sozip

.. only:: html

    - :ref:`gdal_vsi`: Entry point for GDAL Virtual System Interface (VSI) commands
    - :ref:`gdal_vsi_copy`: Copy files located on GDAL Virtual System Interface (VSI)
    - :ref:`gdal_vsi_delete`: Delete files located on GDAL Virtual System Interface (VSI)
    - :ref:`gdal_vsi_list`: List files of one of the GDAL Virtual System Interface (VSI)
    - :ref:`gdal_vsi_move`: Move/rename a file/directory located on GDAL Virtual System Interface (VSI)
    - :ref:`gdal_vsi_sync`: Synchronize source and target file/directory located on GDAL Virtual System Interface (VSI)
    - :ref:`gdal_vsi_sozip`: SOZIP (Seek-Optimized ZIP) related commands

Driver specific commands
++++++++++++++++++++++++

.. toctree::
   :maxdepth: 1
   :hidden:

   gdal_driver_gpkg_repack
   gdal_driver_gti_create
   gdal_driver_openfilegdb_repack
   gdal_driver_parquet_create_metadata_file
   gdal_driver_pdf_list_layers

.. only:: html

    - :ref:`gdal_driver_gpkg_repack`: Repack/vacuum in-place a GeoPackage dataset
    - :ref:`gdal_driver_gti_create`: Create an index of raster datasets compatible of the GDAL Tile Index (GTI) driver
    - :ref:`gdal_driver_openfilegdb_repack`: Repack in-place a FileGeodabase dataset
    - :ref:`gdal_driver_parquet_create_metadata_file`:  Create the _metadata file for a partitioned Parquet dataset
    - :ref:`gdal_driver_pdf_list_layers`: Return the list of layers of a PDF file.


.. _programs_traditional:

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
