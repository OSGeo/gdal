.. _cookbook:

================================================================================
GDAL Command-line Cookbook
================================================================================

.. contents::
    :depth: 3

Introduction
------------

Welcome to the GDAL Users Cookbook. This guide provides practical examples for working with raster and vector data using the GDAL command-line tools.
It demonstrates common tasks such as raster analysis, vector geometry operations, and combining raster and vector workflows.

The cookbook uses a question and answer format, linking to relevant examples elsewhere in the GDAL documentation.

General How do I...
-------------------

... get the GDAL version?
   :example:`gdal-version`.

Raster How do I...
------------------

... resize a raster?
   :example:`gdal-raster-resize-cubic`.

Vector How do I...
------------------

... check if ``X`` vector driver is installed?
    :example:`gdal-vector-drivers`

... buffer geometries?
   :example:`gdal-vector-buffer-1km`

... buffer geometries using an attribute?
   :example:`gdal-pipeline-buffer-line`.

... list all layers in a GeoPackage with their geometry fields and types?
    :example:`gdal-vector-info-geom-name`

Raster and Vector How do I...
-----------------------------

... burn a vector dataset into a raster?
   :example:`gdal-vector-rasterize-burn`

... extract pixel values from a raster and apply them to a point dataset?
   :example:`gdal-raster-pixel-info-extract`
