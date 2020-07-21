.. _gdaltransform:

================================================================================
gdaltransform
================================================================================

.. only:: html

    Transforms coordinates.

.. Index:: gdaltransform

Synopsis
--------

.. code-block::

    gdaltransform [--help-general]
        [-i] [-s_srs srs_def] [-t_srs srs_def] [-to "NAME=VALUE"]
        [-ct proj_string] [-order n] [-tps] [-rpc] [-geoloc]
        [-gcp pixel line easting northing [elevation]]* [-output_xy]
        [srcfile [dstfile]]

Description
-----------

The gdaltransform utility reprojects a list of coordinates into any supported
projection,including GCP-based transformations.

.. program:: gdaltransform

.. option:: -s_srs <srs_def>

    Set source spatial reference.
    The coordinate systems that can be passed are anything supported by the
    OGRSpatialReference.SetFromUserInput() call, which includes EPSG PCS and GCSes
    (i.e. EPSG:4296), PROJ.4 declarations (as above), or the name of a .prj file
    containing well known text.

.. option:: -t_srs <srs_def>

    set target spatial reference.
    The coordinate systems that can be passed are anything supported by the
    OGRSpatialReference.SetFromUserInput() call, which includes EPSG PCS and GCSes
    (i.e. EPSG:4296), PROJ.4 declarations (as above), or the name of a .prj file
    containing well known text.

.. option:: -ct <string>

    A PROJ string (single step operation or multiple step string
    starting with +proj=pipeline), a WKT2 string describing a CoordinateOperation,
    or a urn:ogc:def:coordinateOperation:EPSG::XXXX URN overriding the default
    transformation from the source to the target CRS. It must take into account the
    axis order of the source and target CRS.

    .. versionadded:: 3.0

.. option:: -to NAME=VALUE

    set a transformer option suitable to pass to :cpp:func:`GDALCreateGenImgProjTransformer2`.

.. option:: -order <n>

    order of polynomial used for warping (1 to 3). The default is to select a
    polynomial order based on the number of GCPs.

.. option:: -tps

    Force use of thin plate spline transformer based on available GCPs.

.. option:: -rpc

    Force use of RPCs.

.. option:: -geoloc

    Force use of Geolocation Arrays.

.. option:: -i

    Inverse transformation: from destination to source.

.. option:: -gcp <pixel> <line> <easting> <northing> [<elevation>]

    Provide a GCP to be used for transformation (generally three or more are required)

.. option:: -output_xy

    Restrict output to "x y" instead of "x y z"

.. option:: <srcfile>

    File with source projection definition or GCP's. If
    not given, source projection is read from the command-line :option:`-s_srs`
    or :option:`-gcp` parameters 

.. option:: <dstfile>

    File with destination projection definition. 

Coordinates are read as pairs, triples (for 3D,) or (since GDAL 3.0.0,) quadruplets
(for X,Y,Z,time) of numbers per line from standard
input, transformed, and written out to standard output in the same way. All
transformations offered by gdalwarp are handled, including gcp-based ones.

Note that input and output must always be in decimal form.  There is currently
no support for DMS input or output.

If an input image file is provided, input is in pixel/line coordinates on that
image.  If an output file is provided, output is in pixel/line coordinates
on that image.

Examples
--------

Reprojection Example
++++++++++++++++++++

Simple reprojection from one projected coordinate system to another:

::

    gdaltransform -s_srs EPSG:28992 -t_srs EPSG:31370
    177502 311865

Produces the following output in meters in the "Belge 1972 / Belgian Lambert
72" projection:

::

    244510.77404604 166154.532871342 -1046.79270555763

Image RPC Example
+++++++++++++++++

The following command requests an RPC based transformation using the RPC
model associated with the named file.  Because the -i (inverse) flag is
used, the transformation is from output georeferenced (WGS84) coordinates
back to image coordinates.


::

    gdaltransform -i -rpc 06OCT20025052-P2AS-005553965230_01_P001.TIF
    125.67206 39.85307 50

Produces this output measured in pixels and lines on the image:

::

    3499.49282422381 2910.83892848414 50

X,Y,Z,time transform
++++++++++++++++++++

15-term time-dependent Helmert coordinate transformation from ITRF2000 to ITRF93
for a coordinate at epoch 2000.0

::

    gdaltransform -ct "+proj=pipeline +step +proj=unitconvert +xy_in=deg \
    +xy_out=rad +step +proj=cart +step +proj=helmert +convention=position_vector \
    +x=0.0127 +dx=-0.0029 +rx=-0.00039 +drx=-0.00011 +y=0.0065 +dy=-0.0002 \
    +ry=0.00080 +dry=-0.00019 +z=-0.0209 +dz=-0.0006 +rz=-0.00114 +drz=0.00007 \
    +s=0.00195 +ds=0.00001 +t_epoch=1988.0 +step +proj=cart +inv +step \
    +proj=unitconvert +xy_in=rad +xy_out=deg"
    2 49 0 2000

Produces this output measured in longitude degrees, latitude degrees and ellipsoid height in metre:

::

    2.0000005420366 49.0000003766711 -0.0222802283242345
