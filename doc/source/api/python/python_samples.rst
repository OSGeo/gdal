.. _python_samples:

================================================================================
Sample scripts
================================================================================

The following are sample scripts intended to give an idea of how to use GDAL's Python interface. Please feel free to use them in your applications.

From GDAL 3.2, Python utility scripts :ref:`programs` are located inside the ``osgeo_utils`` module.
From GDAL 3.3, Python sample scripts are located inside the ``osgeo_utils.samples`` sub-module.

.. warning:: Several of the sample scripts on this page have not been updated since they were created and may use outdated coding styles or deprecated APIs.

Python Sample Scripts
---------------------

The scripts provide examples of both raster and vector usage of the GDAL Python API.

.. only:: html

    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/assemblepoly.py`: Script demonstrates how to assemble polygons from arcs. Demonstrates various aspects of OGR Python API.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/build_jp2_from_xml.py`: Build a JPEG2000 file from the XML structure dumped
      by :source_file:`swig/python/gdal-utils/osgeo_utils/samples/dump_jp2.py`.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/classify.py`: Demonstrates using numpy for simple range based classification of an image.
      This is only an example that has stuff hardcoded.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/crs2crs2grid.py`: A script to produce PROJ.4 grid shift files from HTDP program.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/densify.py`: Densifies linestrings by a tolerance.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/dump_jp2.py`: Dump JPEG2000 file structure.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/epsg_tr.py`: Script to create WKT and PROJ.4 dictionaries for EPSG GCS/PCS codes.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/esri2wkt.py`: Simple command line program for translating ESRI .prj files into WKT.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/fft.py`: Script to perform forward and inverse two-dimensional fast Fourier transform.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/fix_gpkg.py`: Fix invalid GeoPackage files.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gcps2ogr.py`: Outputs GDAL GCPs as OGR points
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gcps2vec.py`: Convert GCPs to a point layer.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gcps2wld.py`: Translate the set of GCPs on a file into first order approximation in world file format.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalbuildvrtofvrt.py`: Create a 2-level hierarchy of VRTs for very large collections.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalchksum.py`: Application to checksum a GDAL image file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalcopyproj.py`: Duplicate the geotransform, projection and/or GCPs from one raster dataset to another,
      which can be useful after performing image manipulations with other software that ignores or discards georeferencing metadata.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalfilter.py`: Example script for applying kernel based filters to an image using GDAL.
      Demonstrates use of virtual files as an intermediate representation.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalident.py`: Application to identify files by format.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalimport.py`: Import a GDAL supported file to Tiled GeoTIFF, and build overviews.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_auth.py`: Application for Google web service authentication.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_create_pdf.py`: Create a PDF from a XML composition file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_lut.py`: Read a LUT from a text file, and apply it to an image.
      Sort of a '1 band' version of pct2rgb.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_minmax_location.py`: returns the location where min/max values of a raster are hit.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_mkdir.py`: Create a virtual directory.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_remove_towgs84.py`: Remove TOWGS84[] clause from dataset SRS definitions.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_rm.py`: Delete a virtual file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_rmdir.py`: Delete a virtual directory.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/get_soundg.py`: Script to copy the SOUNDG layer from an S-57 file to a Shapefile,
      splitting up features with MULTIPOINT geometries into many POINT features,
      and appending the point elevations as an attribute.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/histrep.py`: Module to extract data from many rasters into one output.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/jpeg_in_tiff_extract.py`: Extract a JPEG file from a JPEG-in-TIFF tile/strip.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/load2odbc.py`: Load ODBC table to an ODBC datastore. Uses direct SQL since the ODBC driver is read-only for OGR.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/loslas2ntv2.py`: Translate one or many LOS/LAS sets into an NTv2 datum shift grid file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/magphase.py`: Example script computing magnitude and phase images from a complex image.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/make_fuzzer_friendly_archive.py`: Make fuzzer friendly archive (only works in DEBUG mode).
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/mkgraticule.py`: Produce a graticule (grid) dataset.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/ogr2vrt.py`: Produce a graticule (grid) dataset.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/ogr_build_junction_table.py`: Create junction tables for layers coming from GML datasources that
      reference other objects in _href fields
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/rel.py`: Script to produce a shaded relief image from the elevation data. (similar functionality in gdaldem now)
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/tigerpoly.py`: Script demonstrating how to assemble polygons from arcs in TIGER/Line datasource,
      writing results to a newly created shapefile.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/tile_extent_from_raster.py`: Generate the extent of each raster tile in a overview as a vector layer.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/tolatlong.py`: Script to read coordinate system and geotransformation matrix from input file and report
      latitude/longitude coordinates for the specified pixel.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/validate_cloud_optimized_geotiff.py`: Validate Cloud Optimized GeoTIFF file structure.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/validate_geoparquet.py`: Test compliance of GeoParquet file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/validate_gpkg.py`: Test compliance of GeoPackage database w.r.t GeoPackage spec.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/validate_jp2.py`: Validate JPEG2000 file structure.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/val_repl.py`: Script to replace specified values from the input raster file with the new ones.
      May be useful in cases when you don't like value, used for NoData indication and want replace it with other value.
      Input file remains unchanged, results stored in other file.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/vec_tr.py`: Example of applying some algorithm to all the geometries in the file, such as a fixed offset.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/vec_tr_spat.py`: Example of using Intersect() to filter based on only those features that truly intersect a given rectangle.
      Easily extended to general polygon!
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/wcs_virtds_params.py`: Generates MapServer WCS layer definition from a tileindex with mixed SRS.

Legacy Python Sample Scripts
----------------------------

The scripts below have been replaced by updated GDAL utilities (programs) that include additional functionality and improved documentation.
This list is provided to help you locate equivalent functionality when upgrading GDAL.

.. warning:: These scripts are provided for reference only and are not guaranteed to follow current best practices. They may be removed in future versions of GDAL.


.. only:: html

    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdalinfo.py`: See :ref:`gdalinfo`. A direct port of :source_file:`apps/gdalinfo_bin.cpp`.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/ogrinfo.py`: See :ref:`ogrinfo`. A direct port of :source_file:`apps/ogrinfo_bin.cpp`.
    - :source_file:`swig/python/gdal-utils/scripts/ogr_layer_algebra.py`: See :ref:`gdal_vector_layer_algebra`. Application for executing OGR layer algebra operations.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/gdal2xyz.py`: See :ref:`gdal2xyz`. Translates a raster file into xyz format.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/gdal_retile.py`: See :ref:`gdal_retile`. Script for restructuring data in a tree of regular tiles.
    - :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdallocationinfo.py`: See :ref:`gdallocationinfo`. Query information about a pixel given its location.

The following scripts have been replaced by equivalent functionality in GDAL, and have been removed from the repository.

.. only:: html

    - `hsv_merge.py`: Merge greyscale image into RGB image as intensity in HSV space. Equivalent functionality available in :ref:`gdal_raster_blend`.
    - `gdal_ls.py`: Display the list of files in a virtual directory, like /vsicurl or /vsizip. Equivalent functionality available in :ref:`gdal_vsi_list`.
    - `gdal_cp.py`: Copy a virtual file. Equivalent functionality available in :ref:`gdal_dataset_copy`.
    - `gdal_vrtmerge.py`: Similar to gdal_merge, but produces a VRT file. Equivalent functionality available in :ref:`gdal_raster_mosaic`.
    - `gdal2grd.py`: Script to write out ASCII GRD rasters (used in Golden Software Surfer). from any source supported by GDAL. Equivalent functionality available in the :ref:`raster.gsag` driver.
    - `ogr_dispatch.py`: Dispatch features into layers according to the value of some fields or the geometry type. Equivalent functionality available in :ref:`gdal_vector_partition`.
    - `ogrupdate.py`: Update a target datasource with the features of a source datasource. Equivalent functionality available in :ref:`gdal_vector_update`.

.. note::

    For scripts that have been migrated to the new unified ``gdal`` command-line interface, you can run these in Python using ``gdal.Run``. For example:

    .. code-block:: python

        >>> import os
        >>> from osgeo import gdal
        >>> os.environ["AWS_NO_SIGN_REQUEST"] = "YES"
        >>> gdal.Run("vsi", "list", filename="/vsis3/overturemaps-us-west-2/release/2025-10-22.0/theme=buildings/type=building").Output()
        ['part-00000-c5e0b5f2-08ff-4192-af19-c572ecc088f1-c000.zstd.parquet', ...]
