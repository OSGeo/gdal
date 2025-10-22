.. _python_samples:

================================================================================
Sample scripts
================================================================================

The following are sample scripts intended to give an idea how to use the
GDAL's Python interface. Please feel free to use them in your applications.

From GDAL 3.2, Python utility scripts :ref:`programs` are located inside the ``osgeo_utils`` module.
From GDAL 3.3, Python sample scripts are located inside the ``osgeo_utils.samples`` sub-module.

Python Raster Sample scripts
------------------------------

.. only:: html

    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/assemblepoly.py`: Script demonstrates how to assemble polygons from arcs. Demonstrates various aspects of OGR Python API.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/fft.py`: Script to perform forward and inverse two-dimensional fast Fourier transform.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal2grd.py`: Script to write out ASCII GRD rasters (used in Golden Software Surfer). from any source supported by GDAL.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_vrtmerge.py`: Similar to gdal_merge, but produces a VRT file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalcopyproj.py`: Duplicate the geotransform, projection and/or GCPs from one raster dataset to another,
      which can be useful after performing image manipulations with other software that ignores or discards georeferencing metadata.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalfilter.py`: Example script for applying kernel based filters to an image using GDAL.
      Demonstrates use of virtual files as an intermediate representation.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/get_soundg.py`: Script to copy the SOUNDG layer from an S-57 file to a Shapefile,
      splitting up features with MULTIPOINT geometries into many POINT features,
      and appending the point elevations as an attribute.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/histrep.py`: Module to extract data from many rasters into one output.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/load2odbc.py`: Load ODBC table to an ODBC datastore. Uses direct SQL since the ODBC driver is read-only for OGR.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/rel.py`: Script to produce a shaded relief image from the elevation data. (similar functionality in gdaldem now)
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/tigerpoly.py`: Script demonstrating how to assemble polygons from arcs in TIGER/Line datasource,
      writing results to a newly created shapefile.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/tolatlong.py`: Script to read coordinate system and geotransformation matrix from input file and report
      latitude/longitude coordinates for the specified pixel.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/val_repl.py`: Script to replace specified values from the input raster file with the new ones.
      May be useful in cases when you don't like value, used for NoData indication and want replace it with other value.
      Input file remains unchanged, results stored in other file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/vec_tr.py`: Example of applying some algorithm to all the geometries in the file, such as a fixed offset.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/vec_tr_spat.py`: Example of using Intersect() to filter based on only those features that truly intersect a given rectangle.
      Easily extended to general polygon!
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/classify.py`: Demonstrates using numpy for simple range based classification of an image.
      This is only an example that has stuff hardcoded.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_lut.py`: Read a LUT from a text file, and apply it to an image.
      Sort of a '1 band' version of pct2rgb.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/magphase.py`: Example script computing magnitude and phase images from a complex image.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/hsv_merge.py`: Merge greyscale image into RGB image as intensity in HSV space.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_ls.py`: Display the list of files in a virtual directory, like /vsicurl or /vsizip
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_cp.py`: Copy a virtual file
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_minmax_location.py`: returns the location where min/max values of a raster are hit.

Python Vector Sample scripts
------------------------------

.. only:: html

    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/ogrupdate.py`: Update a target datasource with the features of a source datasource. Contrary to ogr2ogr,
      this script tries to match features between the datasources, to decide whether to create a new feature, or to update an existing one.
    - :source_file:`swig/python/gdal-utils/scripts/ogr_layer_algebra.py`: Application for executing OGR layer algebra operations.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/ogr_dispatch.py`: Dispatch features into layers according to the value of some fields or the geometry type.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/wcs_virtds_params.py`: Generates MapServer WCS layer definition from a tileindex with mixed SRS
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/ogr_build_junction_table.py`: Create junction tables for layers coming from GML datasources that
      reference other objects in _href fields
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gcps2ogr.py`: Outputs GDAL GCPs as OGR points


Python Coordinate Reference System Sample scripts
------------------------------------------------------

.. only:: html

    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/crs2crs2grid.py`: A script to produce PROJ.4 grid shift files from HTDP program.

Python direct ports of c++ programs
---------------------------------------

.. only:: html

    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalinfo.py`: See :ref:`gdalinfo`. A direct port of :source_file:`apps/gdalinfo_bin.cpp`.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/ogrinfo.py`: See :ref:`ogrinfo`. A direct port of :source_file:`apps/ogrinfo_bin.cpp`.

Python sample scripts that are now programs
----------------------------------------------

Sample scripts might be upgraded to proper gdal utilities (programs) in next versions
with added functionality and documentation.
The following samples from previous versions are now programs.

.. only:: html

    - :source_file:`swig/python/gdal-utils/osgeo_utils/gdal2xyz.py`: See :ref:`gdal2xyz`. Translates a raster file into xyz format.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/gdal_retile.py`: See :ref:`gdal_retile`. Script for restructuring data in a tree of regular tiles.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdallocationinfo.py`: See :ref:`gdallocationinfo`. Query information about a pixel given its location.
