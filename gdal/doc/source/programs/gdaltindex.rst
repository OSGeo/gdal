.. _gdaltindex:

================================================================================
gdaltindex
================================================================================

.. only:: html

    Creates an OGR-supported dataset as a raster tileindex.

.. Index:: gdaltindex

Synopsis
--------

.. code-block::

    gdaltindex [-f format] [-tileindex field_name] [-write_absolute_path]
            [-skip_different_projection] [-t_srs target_srs]
            [-src_srs_name field_name] [-src_srs_format [AUTO|WKT|EPSG|PROJ]
            [-lyr_name name] index_file [gdal_file]*

Description
-----------

This program creates an OGR-supported dataset with a record for each input raster file,
an attribute containing the filename, and a polygon geometry outlining the
raster.  This output is suitable for use with `MapServer <http://mapserver.org/>`__ as a raster
tileindex.

.. program:: ogrtindex

.. option:: -f <format>

    The OGR format of the output tile index file. Starting with
    GDAL 2.3, if not specified, the format is guessed from the extension (previously
    was ESRI Shapefile).

.. option:: -tileindex <field_name>

    The output field name to hold the file path/location to the indexed
    rasters. The default tile index field name is ``location``.

.. option:: -write_absolute_path

    The absolute path to the raster files is stored in the tile index file.
    By default the raster filenames will be put in the file exactly as they
    are specified on the command line.

.. option:: -skip_different_projection

    Only files with same projection as files already inserted in the tileindex
    will be inserted (unless :option:`-t_srs` is specified). Default does not
    check projection and accepts all inputs.

.. option:: -t_srs <target_srs>:

    Geometries of input files will be transformed to the desired target
    coordinate reference system.
    Default creates simple rectangular polygons in the same coordinate reference
    system as the input rasters.

.. option:: -src_srs_name <field_name>

    The name of the field to store the SRS of each tile. This field name can be
    used as the value of the TILESRS keyword in MapServer

.. option:: -src_srs_format <type>

    The format in which the SRS of each tile must be written. Types can be
    AUTO, WKT, EPSG, PROJ.

.. option:: -lyr_name <name>

    Layer name to create/append to in the output tile index file.

.. option:: index_file

    The name of the output file to create/append to. The default dataset will
    be created if it doesn't already exist, otherwise it will append to the
    existing dataset.

.. option:: <gdal_file>

    The input GDAL raster files, can be multiple files separated by spaces.
    Wildcards my also be used. Stores the file locations in the same style as
    specified here, unless :option:`-write_absolute_path` option is also used.

Examples
--------

- Produce a shapefile (``doq_index.shp``) with a record for every
  image that the utility found in the ``doq`` folder. Each record holds
  information that points to the location of the image and also a bounding rectangle
  shape showing the bounds of the image:

::

    gdaltindex doq_index.shp doq/*.tif

- Perform the same command as before, but now we create a GeoPackage instead of a Shapefile. 

::

    gdaltindex -f GPKG doq_index.gpkg doq/*.tif

- The :option:`-t_srs` option can also be used to transform all input rasters
  into the same output projection:

::

    gdaltindex -t_srs EPSG:4326 -src_srs_name src_srs tile_index_mixed_srs.shp *.tif

- Make a tile index from files listed in a text file :

::

    gdaltindex doq_index.shp --optfile my_list.txt

See also
--------

:ref:`raster_common_options` for other command-line options, and in particular the
:ref:`--optfile <raster_common_options_optfile>` switch that can be used to specify a list of input datasets.
