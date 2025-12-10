.. _gdal_raster_color_map:

================================================================================
``gdal raster color-map``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate a RGB or RGBA dataset from a single band, using a color map.

.. Index:: gdal raster color-map

Synopsis
--------

.. program-output:: gdal raster color-map --help-doc

Description
-----------

:program:`gdal raster color-map` generates a RGB or RGBA dataset from a
single band, using a color map, either attached directly to a raster band,
or from an external text file. It is typically used to create :term:`hypsometric` maps
from a DEM.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Program-Specific Options
------------------------

.. option:: --add-alpha

    Adds an alpha mask band to the output.

.. option:: -b, --band <BAND>

    Index (starting at 1) of the band to process.

.. option:: --color-map <COLOR-MAP>

    Color map filename containing the association between various raster
    values and the corresponding wished color.

    This option must be specified if the input raster band has no attached color table.

    The text-based color configuration file generally contains 4 columns
    per line: the raster value and the corresponding Red, Green, Blue
    component (between 0 and 255). The raster value can be any floating
    point value, or the ``nv`` keyword for the nodata value.
    The raster can also be expressed as a percentage: 0% being the minimum
    value found in the raster, 100% the maximum value.

    An extra column can be optionally added for the alpha component.
    If it is not specified, full opacity (255) is assumed.

    Various field separators are accepted: comma, tabulation, spaces, ':'.

    Common colors used by GRASS can also be specified by using their name,
    instead of the RGB triplet. The supported list is: white, black, red,
    green, blue, yellow, magenta, cyan, aqua, grey/gray, orange, brown,
    purple/violet and indigo.

        GMT :file:`.cpt` palette files are also supported (COLOR_MODEL = RGB only).

    Note: the syntax of the color configuration file is derived from the one
    supported by GRASS r.colors utility. ESRI HDR color table files (.clr)
    also match that syntax. The alpha component and the support of tab and
    comma as separators are GDAL specific extensions.

    For example:

    ::

        3500   white
        2500   235:220:175
        50%   190 185 135
        700    240 250 150
        0      50  180  50
        nv     0   0   0   0


    To implement a "round to the floor value" mode, the raster value can be
    duplicate with a new value being slightly above the threshold.
    For example to have red in [0,10], green in ]10,20] and blue in ]20,30]:

    ::

        0       red
        10      red
        10.001  green
        20      green
        20.001  blue
        30      blue

.. option:: --color-selection interpolate|exact|nearest

    How to compute output colors from input values:

    - ``interpolate`` (default): apply linear interpolation when a raster value
      falls between two entries of the color map.

    - ``exact``: only input raster values matching exacting an entry of the
      color map will be colorized. If none matching color entry is found,
      the "0,0,0,0" RGBA quadruplet will be used.

    - ``nearest``: input raster values will be assigned the entry of the color
      map that is the closest.

    .. note::

        This option is only taken into account when :option:`--color-map`
        is specified.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Generates a RGB dataset from a DTED0 file using an external color map

   .. code-block:: bash

        $ gdal raster color-map --color-map=color-map.txt n43.dt0 out.tif --overwrite


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    hypsometric
