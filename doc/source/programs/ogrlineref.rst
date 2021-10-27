.. _ogrlineref:

================================================================================
ogrlineref
================================================================================

.. only:: html

    Create linear reference and provide some calculations using it.

.. Index:: ogrlineref

Synopsis
--------

.. code-block::

    ogrlineref [--help-general] [-progress] [-quiet]
           [-f format_name] [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE]...]
           [-create]
           [-l src_line_datasource_name] [-ln layer_name] [-lf field_name]
           [-p src_repers_datasource_name] [-pn layer_name] [-pm pos_field_name] [-pf field_name]
           [-r src_parts_datasource_name] [-rn layer_name]
           [-o dst_datasource_name] [-on layer_name]  [-of field_name] [-s step]
           [-get_pos] [-x long] [-y lat]
           [-get_coord] [-m position]
           [-get_subline] [-mb position] [-me position]

Description
-----------

The :program:`ogrlineref` program can be used for:

-  create linear reference file from input data

-  return the "linear referenced" distance for the projection of the
   input coordinates (point) on the path

-  return the coordinates (point) on the path according to the "linear
   referenced" distance

-  return the portion of the path according to the "linear referenced"
   begin and end distances

The :program:`ogrlineref` creates a linear reference - a file containing
a segments of special length (e.g. 1 km in reference units) and get coordinates,
linear referenced distances or sublines (subpaths) from this file.
The utility not required the ``M`` or ``Z`` values in geometry.
The results can be stored in any OGR supported format.
Also some information is written to the stdout.

.. option:: --help-general

    Show the usage.

.. option:: -progress

    Show progress.

.. option:: -quiet

    Suppress all messages except errors and results.

.. option:: -f <format_name>

    Select an output format name. The default is to create a shapefile.

.. option:: -dsco <NAME=VALUE>

    Dataset creation option (format specific)

.. option:: -lco <NAME=VALUE>

    Layer creation option (format specific).

.. option:: -create

    Create the linear reference file (linestring of parts).

.. option:: -l <src_line_datasource_name>

    The path to input linestring datasource (e.g. the road)

.. option:: -ln <layer_name>

    The layer name in datasource

.. option:: -lf <field_name>

    The field name of unique values to separate the input lines (e.g.
    the set of roads).

.. option:: -p <src_repers_datasource_name>

    The path to linear references points (e.g. the road mile-stones)

.. option:: -pn <layer_name>

    The layer name in datasource

.. option:: -pm <pos_field_name>

    The field name of distances along path (e.g. mile-stones values)

.. option:: -pf <field_name>

    The field name of unique values to map input reference points to lines.

.. option:: -r <src_parts_datasource_name>

    The path to linear reference file.

.. option:: -rn <layer_name>

    The layer name in datasource

.. option:: -o <dst_datasource_name>

    The path to output linear reference file (linestring datasource)

.. option:: -on <layer_name>

    The layer name in datasource

.. option:: -of <field_name>

    The field name for storing the unique values of input lines

.. option:: -s <step>

    The part size in linear units

.. option:: -get_pos

    Return linear referenced position for input X, Y

.. option:: -x <long>

    Input X coordinate

.. option:: -y <lat>

    Input Y coordinate

.. option:: -get_coord

    Return point on path for input linear distance

.. option:: -m <position>

    The input linear distance

.. option:: -get_subline

    Return the portion of the input path from and to input linear positions.

.. option:: -mb <position>

    The input begin linear distance

.. option:: -me <position>

    The input end linear distance

Example
-------

This example would create a shapefile (:file:`parts.shp`) containing
a data needed for linear referencing (1 km parts):

.. code-block::

    ogrlineref -create -l roads.shp -p references.shp -pm dist -o parts.shp -s 1000 -progress
