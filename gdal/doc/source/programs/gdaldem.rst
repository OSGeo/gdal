.. _gdaldem:

================================================================================
gdaldem
================================================================================

.. only:: html

    Tools to analyze and visualize DEMs.

.. Index:: gdaldem

Synopsis
--------

.. code-block::

    gdaldem <mode> <input> <output> <options>

Generate a shaded relief map from any GDAL-supported elevation raster:

.. code-block::

    gdaldem hillshade input_dem output_hillshade
                [-z ZFactor (default=1)] [-s scale* (default=1)]
                [-az Azimuth (default=315)] [-alt Altitude (default=45)]
                [-alg Horn|ZevenbergenThorne] [-combined | -multidirectional | -igor]
                [-compute_edges] [-b Band (default=1)] [-of format] [-co "NAME=VALUE"]* [-q]

Generate a slope map from any GDAL-supported elevation raster:

.. code-block::

    gdaldem slope input_dem output_slope_map
                [-p use percent slope (default=degrees)] [-s scale* (default=1)]
                [-alg Horn|ZevenbergenThorne]
                [-compute_edges] [-b Band (default=1)] [-of format] [-co "NAME=VALUE"]* [-q]

Generate an aspect map from any GDAL-supported elevation raster,
outputs a 32-bit float raster with pixel values from 0-360 indicating azimuth:

.. code-block::

    gdaldem aspect input_dem output_aspect_map
                [-trigonometric] [-zero_for_flat]
                [-alg Horn|ZevenbergenThorne]
                [-compute_edges] [-b Band (default=1)] [-of format] [-co "NAME=VALUE"]* [-q]

Generate a color relief map from any GDAL-supported elevation raster:

.. code-block::

    gdaldem color-relief input_dem color_text_file output_color_relief_map
                [-alpha] [-exact_color_entry | -nearest_color_entry]
                [-b Band (default=1)] [-of format] [-co "NAME=VALUE"]* [-q]
    where color_text_file contains lines of the format "elevation_value red green blue"

Generate a Terrain Ruggedness Index (TRI) map from any GDAL-supported elevation raster:

.. code-block::

    gdaldem TRI input_dem output_TRI_map
                [-alg Wilson|Riley]
                [-compute_edges] [-b Band (default=1)] [-of format] [-q]

Generate a Topographic Position Index (TPI) map from any GDAL-supported elevation raster:

.. code-block::

    gdaldem TPI input_dem output_TPI_map
                [-compute_edges] [-b Band (default=1)] [-of format] [-q]

Generate a roughness map from any GDAL-supported elevation raster:

.. code-block::

    gdaldem roughness input_dem output_roughness_map
                [-compute_edges] [-b Band (default=1)] [-of format] [-q]

Description
-----------

The :program:`gdaldem` generally assumes that x, y and z units are identical.
If x (east-west) and y (north-south) units are identical, but z (elevation)
units are different, the scale (-s) option can be used to set the ratio of
vertical units to horizontal.
For LatLong projections near the equator, where units of latitude and units of
longitude are similar, elevation (z) units can be converted to be compatible
by using scale=370400 (if elevation is in feet) or scale=111120 (if elevation is in
meters).  For locations not near the equator, it would be best to reproject your
grid using gdalwarp before using gdaldem.

.. option:: <mode>

    Where <mode> is one of the seven available modes:

    * ``hillshade``

        Generate a shaded relief map from any GDAL-supported elevation raster.

    * ``slope``

        Generate a slope map from any GDAL-supported elevation raster.

    * ``aspect``

        Generate an aspect map from any GDAL-supported elevation raster.

    * ``color-relief``

        Generate a color relief map from any GDAL-supported elevation raster.

    * ``TRI``

        Generate a map of Terrain Ruggedness Index from any GDAL-supported elevation raster.

    * ``TPI``

        Generate a map of Topographic Position Index from any GDAL-supported elevation raster.

    * ``roughness``

        Generate a map of roughness from any GDAL-supported elevation raster.

The following general options are available:

.. option:: input_dem

    The input DEM raster to be processed

.. option:: output_xxx_map

    The output raster produced

.. option:: -of <format>

    Select the output format.

    .. versionadded:: 2.3.0

        If not specified, the format is guessed from the extension
        (previously was :ref:`raster.gtiff`). Use the short format name.

.. option:: -compute_edges

    Do the computation at raster edges and near nodata values

.. option:: -b <band>

    Select an input band to be processed. Bands are numbered from 1.

.. include:: options/co.rst

.. option:: -q

    Suppress progress monitor and other non-error output.

For all algorithms, except color-relief, a nodata value in the target dataset
will be emitted if at least one pixel set to the nodata value is found in the
3x3 window centered around each source pixel. The consequence is that there
will be a 1-pixel border around each image set with nodata value.

    If :option:`-compute_edges` is specified, gdaldem will compute values
    at image edges or if a nodata value is found in the 3x3 window,
    by interpolating missing values.

Modes
-----

hillshade
^^^^^^^^^

This command outputs an 8-bit raster with a nice shaded relief effect. It’s very useful for visualizing the terrain. You can optionally specify the azimuth and altitude of the light source, a vertical exaggeration factor and a scaling factor to account for differences between vertical and horizontal units.

The value 0 is used as the output nodata value.

The following specific options are available :

.. option:: -alg Horn|ZevenbergenThorne

    The literature suggests Zevenbergen & Thorne to be more suited to smooth landscapes, whereas Horn's formula to perform better on rougher terrain.

.. option:: -z <factor>

    Vertical exaggeration used to pre-multiply the elevations

.. option:: -s <scale>

    Ratio of vertical units to horizontal. If the horizontal unit of the source DEM is degrees (e.g Lat/Long WGS84 projection), you can use scale=111120 if the vertical units are meters (or scale=370400 if they are in feet)

.. option:: -az <azimuth>

    Azimuth of the light, in degrees. 0 if it comes from the top of the raster, 90 from the east, ... The default value, 315, should rarely be changed as it is the value generally used to generate shaded maps.

.. option:: -alt <altitude>

    Altitude of the light, in degrees. 90 if the light comes from above the DEM, 0 if it is raking light.

.. option:: -combined

    combined shading, a combination of slope and oblique shading.

.. option:: -multidirectional

    multidirectional shading, a combination of hillshading illuminated from 225 deg, 270 deg, 315 deg, and 360 deg azimuth.

    .. versionadded:: 2.2

.. option:: -igor

    shading which tries to minimize effects on other map features beneath. Can't be used with -alt option.

    .. versionadded:: 3.0

Multidirectional hillshading applies the formula of http://pubs.usgs.gov/of/1992/of92-422/of92-422.pdf.

Igor's hillshading uses formula from Maperitive http://maperitive.net/docs/Commands/GenerateReliefImageIgor.html.

slope
^^^^^

This command will take a DEM raster and output a 32-bit float raster with slope values. You have the option of specifying the type of slope value you want: degrees or percent slope. In cases where the horizontal units differ from the vertical units, you can also supply a scaling factor.

The value `-9999` is used as the output nodata value.

The following specific options are available :

.. option:: -alg Horn|ZevenbergenThorne

    The literature suggests Zevenbergen & Thorne to be more suited to smooth landscapes, whereas Horn's formula to perform better on rougher terrain.

.. option:: -p

    If specified, the slope will be expressed as percent slope. Otherwise, it is expressed as degrees

:option:`-s`

    Ratio of vertical units to horizontal. If the horizontal unit of the source DEM is degrees (e.g Lat/Long WGS84 projection), you can use scale=111120 if the vertical units are meters (or scale=370400 if they are in feet).

aspect
^^^^^^

This command outputs a 32-bit float raster with values between 0° and 360° representing the azimuth that slopes are facing. The definition of the azimuth is such that : 0° means that the slope is facing the North, 90° it's facing the East, 180° it's facing the South and 270° it's facing the West (provided that the top of your input raster is north oriented). The aspect value -9999 is used as the nodata value to indicate undefined aspect in flat areas with slope=0.

The following specifics options are available :

.. option:: -alg Horn|ZevenbergenThorne

    The literature suggests Zevenbergen & Thorne to be more suited to smooth landscapes, whereas Horn's formula to perform better on rougher terrain.

.. option:: -trigonometric

    Return trigonometric angle instead of azimuth. Thus 0° means East, 90° North, 180° West, 270° South.

.. option:: -zero_for_flat

    Return 0 for flat areas with slope=0, instead of -9999.

By using those 2 options, the aspect returned by gdaldem aspect should be
identical to the one of GRASS r.slope.aspect. Otherwise, it's identical to
the one of Matthew Perry's :file:`aspect.cpp` utility.

color-relief
^^^^^^^^^^^^

This command outputs a 3-band (RGB) or 4-band (RGBA) raster with values are computed from the elevation and a text-based color configuration file, containing the association between various elevation values and the corresponding wished color. By default, the colors between the given elevation values are blended smoothly and the result is a nice colorized DEM. The -exact_color_entry or -nearest_color_entry options can be used to avoid that linear interpolation for values that don't match an index of the color configuration file.

The following specifics options are available :

.. option:: color_text_file

    Text-based color configuration file

.. option:: -alpha

    Add an alpha channel to the output raster

.. option:: -exact_color_entry

    Use strict matching when searching in the color configuration file.
    If none matching color entry is found, the "0,0,0,0" RGBA quadruplet will be used

.. option:: -nearest_color_entry

    Use the RGBA quadruplet corresponding to the closest entry in the color configuration file.

The color-relief mode is the only mode that supports VRT as output format.
In that case, it will translate the color configuration file into appropriate
LUT elements. Note that elevations specified as percentage will be translated
as absolute values, which must be taken into account when the statistics of
the source raster differ from the one that was used when building the VRT.

The text-based color configuration file generally contains 4 columns
per line: the elevation value and the corresponding Red, Green, Blue
component (between 0 and 255). The elevation value can be any floating
point value, or the nv keyword for the nodata value.
The elevation can also be expressed as a percentage: 0% being the minimum
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


To implement a "round to the floor value" mode, the elevation value can be
duplicate with a new value being slightly above the threshold.
For example to have red in [0,10], green in ]10,20] and blue in ]20,30]:

::

    0       red
    10      red
    10.001  green
    20      green
    20.001  blue
    30      blue

TRI
^^^

This command outputs a single-band raster with values computed from the elevation.
`TRI` stands for Terrain Ruggedness Index, which measures the difference
between a central pixel and its surrounding cells.

The value -9999 is used as the output nodata value.

The following option is available:

.. option:: -alg Wilson|Riley

    Starting with GDAL 3.3, the Riley algorithm (see Riley, S.J.,
    De Gloria, S.D., Elliot, R. (1999): A Terrain Ruggedness that Quantifies Topographic Heterogeneity.
    Intermountain Journal of Science, Vol.5, No.1-4, pp.23-27) is available and
    the new default value. This algorithm uses the
    square root of the sum of the square of the difference between a central pixel
    and its surrounding cells. This is recommended for terrestrial use cases.

    The Wilson (see Wilson et al 2007, Marine Geodesy 30:3-35) algorithm
    uses the mean difference between a central pixel and its surrounding cells.
    This is recommended for bathymetric use cases.

TPI
^^^

This command outputs a single-band raster with values computed from the elevation.
`TPI` stands for Topographic Position Index, which is defined as the difference
between a central pixel and the mean of its surrounding cells (see Wilson et al
2007, Marine Geodesy 30:3-35).

The value -9999 is used as the output nodata value.

There are no specific options.

roughness
^^^^^^^^^

This command outputs a single-band raster with values computed from the elevation.
Roughness is the largest inter-cell difference of a central pixel and its surrounding
cell, as defined in Wilson et al (2007, Marine Geodesy 30:3-35).

The value -9999 is used as the output nodata value.

There are no specific options.

C API
-----

This utility is also callable from C with :cpp:func:`GDALDEMProcessing`.

.. versionadded:: 2.1

Authors
-------

Matthew Perry perrygeo@gmail.com, Even Rouault even.rouault@spatialys.com,
Howard Butler hobu.inc@gmail.com, Chris Yesson chris.yesson@ioz.ac.uk

Derived from code by Michael Shapiro, Olga Waupotitsch, Marjorie Larson, Jim Westervelt:
U.S. Army CERL, 1993. GRASS 4.1 Reference Manual. U.S. Army Corps of Engineers,
Construction Engineering Research Laboratories, Champaign, Illinois, 1-425.

See also
--------

Documentation of related GRASS utilities:

https://grass.osgeo.org/grass79/manuals/r.slope.aspect.html

https://grass.osgeo.org/grass79/manuals/r.relief.html

https://grass.osgeo.org/grass79/manuals/r.colors.html
