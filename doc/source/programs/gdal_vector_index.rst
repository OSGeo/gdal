.. _gdal_vector_index:

================================================================================
``gdal vector index``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Create a vector index of vector datasets.

.. Index:: gdal vector index

Synopsis
--------

.. program-output:: gdal vector index --help-doc

Description
-----------

:program:`gdal vector index` creates a vector dataset with a record for each
input vector file, an attribute containing the filename (suffixed with comma
and the index of the layer within the dataset) and a polygon geometry
outlining the (rectangular) extent.
This output is suitable for use with `MapServer <http://mapserver.org/>`__ as a
vector tileindex

Program-Specific Options
------------------------

.. option:: --absolute-path

    The absolute path to the vector files is stored in the index file.
    By default the vector filenames will be put in the file exactly as they
    are specified on the command line.

.. option:: --accept-different-crs

    Whether layers with different CRS are accepted.
    By default, unless :option:`--dst-crs` is specified, layers that do not have
    the same CRS as the first index layer will be skipped. Setting :option:`--accept-different-crs`
    may be useful to avoid the CRS consistency check.

.. option:: --accept-different-schemas

    Whether layers with different schemas are accepted.
    By default, layers that do not have the same schemas as the first index layer
    will be skipped.

.. option:: --dataset-name-only

    Whether to write the dataset name only, instead of suffixed with the layer index.

    .. warning:: Setting this option will generate a location not compatible of MapServer.

.. option:: --dst-crs <DST-CRS>

    Envelopes of the input files will be transformed to the desired target
    coordinate reference system.
    Default creates simple rectangular polygons in the same coordinate reference
    system as the input vectors.

.. option:: --filename-filter <FILENAME-FILTER>

    Pattern that the filenames contained in input directories should follow.
    '*' is a wildcard character that matches any number of any characters
    including none. '?' is a wildcard character that matches a single character.
    Comparisons are done in a case insensitive way.
    Several filters may be specified.

    For example: ``--filename-filter "*.shp,*.gpkg"``

.. option:: --location-name <LOCATION-NAME>

    The output field name to hold the file path/location to the indexed
    vectors. The default field name is ``location``.

.. option:: --metadata <KEY>=<VALUE>

    Write an arbitrary layer metadata item, for formats that support layer
    metadata.
    This option may be repeated.

.. option:: --recursive

    Whether input directories should be explored recursively.

.. option:: --source-crs-field-name <SOURCE-CRS-FIELD-NAME>

    The name of the field to store the CRS of each tile. This field name can be
    used as the value of the TILESRS keyword in MapServer

.. option:: --source-crs-format auto|WKT|EPSG|PROJ

    The format in which the CRS of each tile must be written. Types can be
    ``auto``, ``WKT``, ``EPSG``, ``PROJ``.
    This option should be used together with :option:`--source-crs-field-name`.

.. option:: --source-layer-name <SOURCE-LAYER-NAME>

    Add layer of specified name from each source file in the tile index [may be repeated]
    If none of

.. option::  --source-layer-index <SOURCE-LAYER-INDEX>

    Add layer of specified index (0-based) from each source file in the tile index [may be repeated]


Standard Options
----------------

.. include:: gdal_options/append_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/lco.rst

.. include:: gdal_options/of_vector.rst

.. option:: --nln, --output-layer <LAYER>

    Provides a name for the output vector layer.

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/overwrite_layer.rst

.. include:: gdal_options/update.rst

Examples
--------

.. example::

   Produce a GeoPackage with a record for every
   layer that the utility found in the ``countries`` folder. Each record holds
   information that points to the location of each input vector file and also a bounding rectangle
   shape showing its bounds:

   .. code-block:: bash

      gdal vector index countries/*.shp index.gpkg

.. example::

   The :option:`--dst-crs` option can also be used to transform all input vector
   envelopes into the same output projection:

   .. code-block:: bash

       gdal vector index --dst-crs EPSG:4326 --source-crs-field-name=src_srs countries/*.shp index.gpkg
