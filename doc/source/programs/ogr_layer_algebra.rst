.. _ogr_layer_algebra:

================================================================================
ogr_layer_algebra
================================================================================

.. versionadded:: 3.6

.. only:: html

    Performs various Vector layer algebraic operations.

.. Index:: ogr_layer_algebra

Synopsis
--------

.. code-block::

    ogr_layer_algebra [--help] [--help-general]
                        Union|Intersection|SymDifference|Identity|Update|Clip|Erase
                        -input_ds <path> [-input_lyr <name>]
                        -method_ds <path> [-method_lyr <name>]
                        -output_ds <path> [-output_lyr <name>] [-overwrite]
                        [-opt <NAME>=<VALUE>]...
                        [-f <format_name>] [-dsco <NAME>=<VALUE>]... [-lco <NAME>=<VALUE>]...
                        [-input_fields {NONE|ALL|<fld1>,<fl2>,...<fldN>}] [-method_fields {NONE|ALL|<fld1>,<fl2>,...<fldN>}]
                        [-nlt <geom_type>] [-a_srs <srs_def>]

Description
-----------

The :program:`ogr_layer_algebra` provides a command line utility to perform various vector layer algebraic operations. The utility takes a vector
input source and a method source and generates the output of the operation in the specified output file.

.. note::

    ogr_layer_algebra is a Python utility, and is only available if GDAL Python bindings are available.

.. program:: ogr_layer_algebra

.. include:: options/help_and_help_general.rst

.. option:: <mode>

    Where <mode> is one of the seven available modes:

    * ``Union``

        A union is a set of features, which represent areas that are in either of the operand layers.

    * ``Intersection``

        An intersection is a set of features, which represent the common areas of two layers.

    * ``SymDifference``

        A symmetric difference is a set of features, which represent areas that are in operand layers but which do not intersect.

    * ``Identity``

        The identity method identifies features in the input layer with features in the method layer possibly splitting features into several features.
        By default the result layer has attributes from both operand layers.

    * ``Update``

        The update method creates a layer, which add features into the input layer from the method layer possibly cutting features in the input layer.
        By default the result layer has attributes only from the input layer.

    * ``Clip``

        The clip method creates a layer, which has features from the input layer clipped to the areas of the features in the method layer.
        By default the result layer has attributes of the input layer.

    * ``Erase``

        The erase method creates a layer, which has features from the input layer whose areas are erased by the features in the method layer.
        By default the result layer has attributes of the input layer.

.. option:: -input_ds <path>

    Input dataset path for the operation to be performed.
    For operations involving two datasets, this is one of the datasets.

.. option:: -input_lyr <name>

    Layer name of the ``input_ds`` for which the operations have to be performed ( Optional )

.. option:: -method_ds <path>

    Method data set path for the operation to be performed.
    This is usually the conditional data set supplied to the operation ( ex: clip , erase , update )
    This is the Second data set in the operation ( ex : Union, Intersection , SymDifference )

.. option:: -method_lyr <name>

    Layer name of the ``method_ds`` for which the operations have to be performed ( Optional )

.. option:: -output_ds <path>

    Output data set path for writing the result of the operations performed by ``ogr_layer_algebra``.

.. option:: -output_lyr_name <name>

    Layer name of the ``output_lyr_name`` where the output vector has to be written. ( Optional )

.. option:: -overwrite

    Indicates whether the ``output_ds`` have to be overwritten with the generated result of ``ogr_layer_algebra``.

.. option:: -opt <NAME>=<VALUE>

    Attributes for which the operation has to run on ``input_ds`` and ``method_ds``.

.. option:: -f <format_name>

    Select the output format.If not specified,
    the format is guessed from the extension (previously was ESRI Shapefile).
    Use the short format name

.. option:: -dsco <NAME>=<VALUE>

    Dataset creation option (format specific).

.. option:: -lco <NAME>=<VALUE>

    Layer creation option (format specific).

.. option:: -input_fields {NONE|ALL|<fld1>,<fl2>,...<fldN>}

    Comma-delimited list of fields from input layer to copy to the output layer ,
    if eligible according to the operation.

.. option:: -method_fields {NONE|ALL|<fld1>,<fl2>,...<fldN>}

    Comma-delimited list of fields from method layer to copy to the output layer ,
    if eligible according to the operation.

.. option:: -nlt <geom_type>

    Define the geometry type for the created layer.
    One of NONE, GEOMETRY, POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION,
    MULTIPOINT, MULTIPOLYGON, GEOMETRY25D, POINT25D, LINESTRING25D, POLYGON25D,
    GEOMETRYCOLLECTION25D, MULTIPOINT25D, MULTIPOLYGON25D.

.. option:: -a_srs <srs_def>

    Assign an output SRS, but without reprojecting

    The coordinate reference systems that can be passed are anything supported by the
    OGRSpatialReference.SetFromUserInput() call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.
