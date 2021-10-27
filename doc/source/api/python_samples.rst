.. _python_samples:

================================================================================
Python Sample scripts
================================================================================

The following are sample scripts intended to give an idea how to use the
GDAL's Python interface. Please feel free to use them in your applications.

From gdal 3.2, Python utility scripts :ref:`programs` are located inside the `osgeo_utils` module.
From gdal 3.3, Python sample scripts are located inside the `osgeo_utils.samples` sub-module.

Python Raster Sample scripts
------------------------------

.. only:: html

    - .. _assemblepoly: Script demonstrates how to assemble polygons from arcs. Demonstrates various aspects of OGR Python API.
    - .. _fft: Script to perform forward and inverse two-dimensional fast Fourier transform.
    - .. _gdal2grd: Script to write out ASCII GRD rasters (used in Golden Software Surfer). from any source supported by GDAL.
    - .. _gdal_vrtmerge: Similar to gdal_merge, but produces a VRT file.
    - .. _gdalcopyproj: Duplicate the geotransform, projection and/or GCPs from one raster dataset to another,
            which can be useful after performing image manipulations with other software that ignores or discards georeferencing metadata.
    - .. _gdalfilter:  Example script for applying kernel based filters to an image using GDAL.
            Demonstrates use of virtual files as an intermediate representation.
    - .. _get_soundg: Script to copy the SOUNDG layer from an S-57 file to a Shapefile,
            splitting up features with MULTIPOINT geometries into many POINT features,
            and appending the point elevations as an attribute.
    - .. _histrep: Module to extract data from many rasters into one output.
    - .. _load2odbc: Load ODBC table to an ODBC datastore. Uses direct SQL since the ODBC driver is read-only for OGR.
    - .. _rel: Script to produce a shaded relief image from the elevation data. (similar functionality in gdaldem now)
    - .. _tigerpoly:  Script demonstrating how to assemble polygons from arcs in TIGER/Line datasource,
            writing results to a newly created shapefile.
    - .. _tolatlong: Script to read coordinate system and geotransformation matrix from input file and report
            latitude/longitude coordinates for the specified pixel.
    - .. _val_repl: Script to replace specified values from the input raster file with the new ones.
            May be useful in cases when you don't like value, used for NoData indication and want replace it with other value.
            Input file remains unchanged, results stored in other file.
    - .. _vec_tr: Example of applying some algorithm to all the geometries in the file, such as a fixed offset.
    - .. _vec_tr_spat: Example of using Intersect() to filter based on only those features that truly intersect a given rectangle.
            Easily extended to general polygon!
    - .. _classify: Demonstrates using numpy for simple range based classification of an image.
            This is only an example that has stuff hardcoded.
    - .. _gdal_lut: Read a LUT from a text file, and apply it to an image.
            Sort of a '1 band' version of pct2rgb.
    - .. _magphase: Example script computing magnitude and phase images from a complex image.
    - .. _hsv_merge: Merge greyscale image into RGB image as intensity in HSV space.
    - .. _gdal_ls: Display the list of files in a virtual directory, like /vsicurl or /vsizip
    - .. _gdal_cp: Copy a virtual file

Python Vector Sample scripts
------------------------------

.. only:: html

    - .. _ogrupdate: Update a target datasource with the features of a source datasource. Contrary to ogr2ogr,
            this script tries to match features between the datasources,
            to decide whether to create a new feature, or to update an existing one.
    - .. _ogr_layer_algebra: Application for executing OGR layer algebra operations.
    - .. _ogr_dispatch: Dispatch features into layers according to the value of some fields or the geometry type.
    - .. _wcs_virtds_params: Generates MapServer WCS layer definition from a tileindex with mixed SRS
    - .. _ogr_build_junction_table: Create junction tables for layers coming from GML datasources that
            reference other objects in _href fields
    - .. _gcps2ogr: Outputs GDAL GCPs as OGR points


Python Coordinate Reference System Sample scripts
------------------------------------------------------

.. only:: html

    - .. _crs2crs2grid: A script to produce PROJ.4 grid shift files from HTDP program.
            `<http://trac.osgeo.org/proj/wiki/HTDPGrids>`_

Python direct ports of c++ programs
---------------------------------------

.. only:: html

    - :ref:`gdalinfo`: A direct port of apps/gdalinfo.c
    - :ref:`ogrinfo`: A direct port of apps/ogrinfo.cpp
    - :ref:`ogr2ogr`: A direct port of apps/ogr2ogr.cpp
    - :ref:`gdallocationinfo`: A direct port of apps/gdallocationinfo.cpp

Python sample scripts that are now programs
----------------------------------------------

Sample scripts might be upgraded to proper gdal utilities (programs) in next versions
with added functionality and documentation.
The following samples from previous versions are now programs.

.. only:: html

    - :ref:`gdal2xyz`: Translates a raster file into xyz format.
    - :ref:`gdal_retile`: Script for restructuring data in a tree of regular tiles.
    - val_at_coord: see :ref:`gdallocationinfo`
