.. _ogrmerge:

================================================================================
ogrmerge.py
================================================================================

.. only:: html

    Merge several vector datasets into a single one.

.. Index:: ogrmerge

Synopsis
--------

.. code-block::

    ogrmerge.py -o out_dsname src_dsname [src_dsname]*
                [-f format] [-single] [-nln layer_name_template]
                [-update | -overwrite_ds] [-append | -overwrite_layer]
                [-src_geom_type geom_type_name[,geom_type_name]*]
                [-dsco NAME=VALUE]* [-lco NAME=VALUE]*
                [-s_srs srs_def] [-t_srs srs_def | -a_srs srs_def]
                [-progress] [-skipfailures] [--help-general]

Options specific to the :ref:`-single <ogrmerge_single_option>` option:

.. code-block::

                [-field_strategy FirstLayer|Union|Intersection]
                [-src_layer_field_name name]
                [-src_layer_field_content layer_name_template]

Description
-----------

.. versionadded:: 2.2

:program:`ogrmerge.py` script takes as input several vector datasets,
each of them having one or several vector layers, and copy them in
a target dataset.

There are essential two modes:

*  the default one, where each input vector layer, is copied as a
   separate layer into the target dataset

*  another one, activated with the -single switch, where the content of
   all input vector layers is appended into a single target layer. This
   assumes that the schema of those vector layers is more or less the
   same.

Internally this generates a :ref:`vector.vrt` file, and if the
output format is not VRT, final translation is done with :program:`ogr2ogr`
or :py:func:`gdal.VectorTranslate`. So, for advanced uses, output to VRT,
potential manual editing of it and :program:`ogr2ogr` can be done.

.. program:: ogrmerge.py

.. option:: -o <out_dsname>

    Output dataset name. Required.

.. option:: <src_dsname>

    One or several input vector datasets. Required

.. option:: -f <format>

    Select the output format. Starting with GDAL 2.3, if not specified,
    the format is guessed from the extension (previously was ESRI
    Shapefile). Use the short format name

.. _ogrmerge_single_option:
.. option:: -single

    If specified, all input vector layers will be merged into a single one.

.. option:: -nln <layer_name_template>

    Name of the output vector layer (in single mode, and the default is
    "merged"), or template to name the output vector layers in default
    mode (the default value is ``{AUTO_NAME}``). The template can be a
    string with the following variables that will be susbstitued with a
    value computed from the input layer being processed:

    -  ``{AUTO_NAME}``: equivalent to ``{DS_BASENAME}_{LAYER_NAME}`` if both
       values are different, or ``{LAYER_NAME}`` when they are identical
       (case of shapefile). 'different
    -  ``{DS_NAME}``: name of the source dataset
    -  ``{DS_BASENAME}``: base name of the source dataset
    -  ``{DS_INDEX}``: index of the source dataset
    -  ``{LAYER_NAME}``: name of the source layer
    -  ``{LAYER_INDEX}``: index of the source layer

.. option:: -update

    Open an existing dataset in update mode.

.. option:: -overwrite_ds

    Overwrite the existing dataset if it already exists (for file based
    datasets)

.. option:: -append

    Open an existing dataset in update mode, and if output layers
    already exist, append the content of input layers to them.

.. option:: -overwrite_layer

    Open an existing dataset in update mode, and if output layers
    already exist, replace their content with the one of the input
    layer.

.. option:: -src_geom_type <geom_type_name[,geom_type_name]\*]>

    Only take into account input layers whose geometry type match the
    type(s) specified. Valid values for geom_type_name are GEOMETRY,
    POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON,
    GEOMETRYCOLLECTION, CIRCULARSTRING, CURVEPOLYGON, MULTICURVE,
    MULTISURFACE, CURVE, SURFACE, TRIANGLE, POLYHEDRALSURFACE and TIN.

.. option:: -dsco <NAME=VALUE>

    Dataset creation option (format specific)

.. option:: -lco <NAME=VALUE>

    Layer creation option (format specific)

.. option:: -a_srs <srs_def>

    Assign an output SRS

.. option:: -t_srs <srs_def>

    Reproject/transform to this SRS on output

.. option:: -s_srs <srs_def>

    Override source SRS

.. option:: -progress

    Display progress on terminal. Only works if input layers have the
    "fast feature count" capability.

.. option:: -skipfailures

    Continue after a failure, skipping the failed feature.

.. option:: -field_strategy FirstLayer|Union|Intersection

    Only used with :option:`-single`. Determines how the schema of the target
    layer is built from the schemas of the input layers. May be
    FirstLayer to use the fields from the first layer found, Union to
    use a super-set of all the fields from all source layers, or
    Intersection to use a sub-set of all the common fields from all
    source layers. Defaults to Union.

.. option:: -src_layer_field_name <name>

    Only used with :option:`-single`. If specified, the schema of the target layer
    will be extended with a new field 'name', whose content is
    determined by -src_layer_field_content.

.. option:: -src_layer_field_content <layer_name_template>

    Only used with :option:`-single`. If specified, the schema of the target layer
    will be extended with a new field (whose name is given by
    :option:`-src_layer_field_name`, or 'source_ds_lyr' otherwise), whose
    content is determined by ``layer_name_template``. The syntax of
    ``layer_name_template`` is the same as for :option:`-nln`.

Examples
--------

Create a VRT with a layer for each input shapefiles

.. code-block::

    ogrmerge.py -f VRT -o merged.vrt *.shp

Same, but creates a GeoPackage file

.. code-block::

    ogrmerge.py -f GPKG -o merged.gpkg *.shp

Concatenate the content of france.shp and germany.shp in merged.shp,
and adds a 'country' field to each feature whose value is 'france' or
'germany' depending where it comes from.

.. code-block::

    ogrmerge.py -single -o merged.shp france.shp germany.shp -src_layer_field_name country
