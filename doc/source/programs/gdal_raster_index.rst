.. _gdal_raster_index:

================================================================================
``gdal raster index``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Create a vector index of raster datasets.

.. Index:: gdal raster index

Synopsis
--------

.. program-output:: gdal raster index --help-doc

Description
-----------

:program:`gdal raster index` creates a vector dataset with a record for each
input raster file, an attribute containing the filename, and a polygon geometry
outlining the raster.
This output is suitable for use with `MapServer <http://mapserver.org/>`__ as a
raster tileindex

See :ref:`gdal_driver_gti_create` for an extension of this command
that create files to be used as input for the :ref:`GTI <raster.gti>` driver.

The following options are available:

Standard options
++++++++++++++++


.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. option:: --update

    Whether the output dataset must be opened in update mode. Implies that
    it already exists.

.. option:: --overwrite-layer

    Whether overwriting the existing output vector layer is allowed.

.. option:: --append

    Whether appending features to the existing output vector layer is allowed.

.. option:: -l, --nln, --layer <LAYER>

    Provides a name for the output vector layer.

.. option:: --recursive

    Whether input directories should be explored recursively.

.. option:: --filename-filter <FILENAME-FILTER>

    Pattern that the filenames contained in input directories should follow.
    '*' is a wildcard character that matches any number of any characters
    including none. '?' is a wildcard character that matches a single character.
    Comparisons are done in a case insensitive way.
    Several filters may be specified.

    For example: ``--filename-filter "*.tif,*.tiff"``

.. option:: --min-pixel-size <val>

    Minimum pixel size in term of geospatial extent per pixel (resolution) that
    a raster should have to be selected. The pixel size
    is evaluated after reprojection of its extent to the target CRS defined
    by :option:`--dst-crs`.

.. option:: --max-pixel-size <val>

    Maximum pixel size in term of geospatial extent per pixel (resolution) that
    a raster should have to be selected. The pixel size
    is evaluated after reprojection of its extent to the target CRS defined
    by :option:`--dst-crs`.

.. option:: --location-name <LOCATION-NAME>

    The output field name to hold the file path/location to the indexed
    rasters. The default field name is ``location``.

.. option:: --absolute-path

    The absolute path to the raster files is stored in the index file.
    By default the raster filenames will be put in the file exactly as they
    are specified on the command line.

.. option:: --dst-crs <DST-CRS>

    Geometries of input files will be transformed to the desired target
    coordinate reference system.
    Default creates simple rectangular polygons in the same coordinate reference
    system as the input rasters.

.. option:: --source-crs-field-name <SOURCE-CRS-FIELD-NAME>

    The name of the field to store the CRS of each tile. This field name can be
    used as the value of the TILESRS keyword in MapServer

.. option:: --source-crs-format auto|WKT|EPSG|PROJ

    The format in which the CRS of each tile must be written. Types can be
    ``auto``, ``WKT``, ``EPSG``, ``PROJ``.
    This option should be used together with :option:`--source-crs-field-name`.

.. option:: --metadata <KEY>=<VALUE>

    Write an arbitrary layer metadata item, for formats that support layer
    metadata.
    This option may be repeated.

.. option:: --skip-errors

    .. versionadded:: 3.12

    Skip errors related to input datasets.

Examples
--------

.. example::

   Produce a GeoPackage with a record for every
   image that the utility found in the ``doq`` folder. Each record holds
   information that points to the location of the image and also a bounding rectangle
   shape showing the bounds of the image:

   .. code-block:: bash

      gdal raster index doq/*.tif doq_index.gpkg

.. example::

   The :option:`--dst-crs` option can also be used to transform all input raster
   geometries into the same output projection:

   .. code-block:: bash

       gdal raster index --dst-crs EPSG:4326 --source-crs-field-name=src_srs *.tif tile_index_mixed_crs.gpkg
