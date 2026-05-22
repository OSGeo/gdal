================================================================================
Introduction
================================================================================

GDAL 3.11 introduced a new command line interface (CLI), simply called "gdal", supplementing the traditional well-known GDAL utilities (gdal_translate, ogr2ogr, etc.), to provide users with a more uniform, predictable and user-friendly experience.
This workshop will give the opportunity to participants to get a hands-on experience to discover the capabilities of the new CLI through a series of exercises, including how to leverage them from Python.

Target audience
---------------

Suitable for those new to GDAL, as well as those already experienced with the traditional utilities and wishing to get to speed with the new CLI. Some familiarity with geospatial raster and vector data and coordinate systems is assumed but not strictly required. Participants should not be afraid of command line use! Some SQL and Python knowledge will be useful for advanced exercises. 

Content
-------

We will explore the general principles of the CLI and apply them to the various algorithms it offers:

- exploring the contents and metadata of raster and vector datasets,
- performing file format transformations,
- subsetting, resampling, reprojection
- mosaicing and tiling raster datasets
- merging and partitioning vector datasets
- pixel operations
- DEM processing
- building virtual rasters and mosaics
- querying vector layers
- multidimensional dataset management
- Virtual System Interface (VSI) commands to list and copy files

We will also cover more advanced topics, such as basic processing pipelines,
replayable pipelines (.gdalg.json files), nested pipelines, tee operator, and
explore how to use the new capabilities from Python.

Copyright and License
---------------------

The material, test datasets excluded, is Copyright 2026, Even Rouault and
licensed under the MIT license.
