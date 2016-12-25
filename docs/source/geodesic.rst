.. _geodesic:

================================================================================
Geodesic Calculations
================================================================================

.. contents:: Contents
   :depth: 3
   :backlinks: none


Geodesic Calculations
--------------------------------------------------------------------------------

Geodesic calculations are calculations along lines (great circle) on the
surface of the earth. They can answer questions like:

 * What is the distance between these two points?
 * If I travel X meters from point A at bearing phi, where will I be.  They are
   done in native lat-long coordinates, rather than in projected coordinates.

Relevant mailing list threads
................................................................................

 * http://thread.gmane.org/gmane.comp.gis.proj-4.devel/3361
 * http://thread.gmane.org/gmane.comp.gis.proj-4.devel/3375
 * http://thread.gmane.org/gmane.comp.gis.proj-4.devel/3435
 * http://thread.gmane.org/gmane.comp.gis.proj-4.devel/3588
 * http://thread.gmane.org/gmane.comp.gis.proj-4.devel/3925
 * http://thread.gmane.org/gmane.comp.gis.proj-4.devel/4047
 * http://thread.gmane.org/gmane.comp.gis.proj-4.devel/4083

Terminology
--------------------------------------------------------------------------------

The shortest distance on the surface of a solid is generally termed a geodesic,
be it an ellipsoid of revolution, aposphere, etc.  On a sphere, the geodesic is
termed a Great Circle.

HOWEVER, when computing the distance between two points using a projected
coordinate system, that is a conformal projection such as Transverse Mercator,
Oblique Mercator, Normal Mercator, Stereographic, or Lambert Conformal Conic -
that then is a GRID distance which can be converted to an equivalent GEODETIC
distance using the function for "Scale Factor at a Point."  The conversion is
then termed "Grid Distance to Geodetic Distance," even though it will not be as
exactly correct as a true ellipsoidal geodesic.  Closer to the truth with a TM
than with a Lambert or other conformal projection, but still not exactly "on."


So, it can be termed "geodetic distance" or a  "geodesic distance," depending
on just how you got there ...


The Math
--------------------------------------------------------------------------------

Spherical Approximation
................................................................................

The simplest way to compute geodesics is using a sphere as an approximation for
the earth. This from Mikael Rittri on the Proj mailing list:

    If 1 percent accuracy is enough, I think you can use spherical formulas
    with a fixed Earth radius.  You can find good formulas in the Aviation
    Formulary of Ed Williams, http://williams.best.vwh.net/avform.htm.

    For the fixed Earth radius, I would choose the average of the:

        c   = radius of curvature at the poles,
        b^2^ / a = radius of curvature in a meridian plane at the equator,

    since these are the extreme values for the local radius of curvature of the
    earth ellipsoid.

    If your coordinates are given in WGS84, then

        c   = 6 399 593.626 m,
        b^2^ / a = 6 335 439.327 m,

    (see http://home.online.no/~sigurdhu/WGS84_Eng.html) so their average is 6,367,516.477 m.
    The maximal error for distance calculation should then be less than 0.51 percent.

    When computing the azimuth between two points by the spherical formulas,  I
    think the maximal error on WGS84 will be 0.2 degrees, at least if the
    points are not too far away (less than 1000 km apart, say). The error
    should be maximal near the equator, for azimuths near northeast etc.

I am not sure about the spherical errors for the forward geodetic problem:
point positioning given initial point, distance and azimuth.

Ellipsoidal Approximation
................................................................................

For more accuracy, the earth can be approximated with an ellipsoid,
complicating the math somewhat.  See the wikipedia page, `Geodesics on an
ellipsoid <https://en.wikipedia.org/wiki/Geodesics_on_an_ellipsoid>`__, for
more information.

Thaddeus Vincenty's method, April 1975
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For a very good procedure to calculate inter point distances see:

http://www.ngs.noaa.gov/PC_PROD/Inv_Fwd/ (Fortan code, DOS executables, and an online app)

and algorithm details published in: `Vincenty, T. (1975) <http://www.ngs.noaa.gov/PUBS_LIB/inverse.pdf>`__

Javascript code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Chris Veness has coded Vincenty's formulas as !JavaScript.

distance: http://www.movable-type.co.uk/scripts/latlong-vincenty.html

direct:   http://www.movable-type.co.uk/scripts/latlong-vincenty-direct.html

C code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

From Gerald Evenden: a library of the converted NGS Vincenty geodesic procedure
and an application program, 'geodesic'.  In the case of a spherical earth
Snyder's preferred equations are used.

* http://article.gmane.org/gmane.comp.gis.proj-4.devel/3588/

The link in this message is broken.  The correct URL is
http://home.comcast.net/~gevenden56/proj/

Earlier Mr. Evenden had posted to the PROJ.4 mailing list this code for
determination of true distance and respective forward and back azimuths between
two points on the ellipsoid.  Good for any pair of points that are not
antipodal.
Later he posted that this was not in fact the translation of NGS FORTRAN code,
but something else. But, for what it's worth, here is the posted code (source
unknown):

* http://article.gmane.org/gmane.comp.gis.proj-4.devel/3478


PROJ.4 - geod program
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The PROJ.4 [wiki:man_geod geod] program can be used for great circle distances
on an ellipsoid.  As of proj verion 4.9.0, this uses a translation of
GeographicLib::Geodesic (see below) into C.  The underlying geodesic
calculation API is exposed as part of the PROJ.4 library (via the geodesic.h
header).  Prior to version 4.9.0, the algorithm documented here was used:
`
Paul D. Thomas, 1970
Spheroidal Geodesics, Reference Systems, and Local Geometry"
U.S. Naval Oceanographic Office, p. 162
Engineering Library 526.3 T36s

http://handle.dtic.mil/100.2/AD0703541

GeographicLib::Geodesic
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Charles Karney has written a C++ class to do geodesic calculations and a
utility GeodSolve to call it.  See

* http://geographiclib.sourceforge.net/geod.html

An online version of GeodSolve is available at

* http://geographiclib.sourceforge.net/cgi-bin/GeodSolve

This is an attempt to do geodesic calculations "right", i.e.,

* accurate to round-off (i.e., about 15 nm);
* inverse solution always succeeds (even for near anti-podal points);
* reasonably fast (comparable in speed to Vincenty);
* differential properties of geodesics are computed (these give the scales of
  geodesic projections);
* the area between a geodesic and the equator is computed (allowing the
  area of geodesic polygons to be found);
* included also is an implementation in terms of elliptic integrals which
  can deal with ellipsoids with 0.01 < b/a < 100.

A JavaScript implementation is included, see

* `geo-calc <http://geographiclib.sourceforge.net/scripts/geod-calc.html>`__,
   a text interface to geodesic calculations;
* `geod-google <http://geographiclib.sourceforge.net/scripts/geod-google.html>`__,
   a tool for drawing geodesics on Google Maps.

Implementations in `Python <http://pypi.python.org/pypi/geographiclib>`__,
`Matlab <http://www.mathworks.com/matlabcentral/fileexchange/39108>`__,
`C <http://geographiclib.sourceforge.net/html/C/>`__,
`Fortran <http://geographiclib.sourceforge.net/html/Fortran/>`__ , and
`Java <http://geographiclib.sourceforge.net/html/java/>`__ are also available.

The algorithms are described in
 * C. F. F. Karney, `Algorithms for gedesics <http://dx.doi.org/10.1007/s00190-012-0578-z>`__,
   J. Geodesy '''87'''(1), 43-55 (2013),
   DOI: `10.1007/s00190-012-0578-z <http://dx.doi.org/10.1007/s00190-012-0578-z>`__; `geo-addenda.html <http://geographiclib.sf.net/geod-addenda.html>`__.

Triaxial Ellipsoid
................................................................................

A triaxial ellipsoid is a marginally better approximation to the shape of the earth
than an ellipsoid of revolution.
The problem of geodesics on a triaxial ellipsoid was solved by Jacobi in 1838.
For a discussion of this problem see
* http://geographiclib.sourceforge.net/html/triaxial.html
* the wikipedia entry: `Geodesics on a triaxial ellipsoid <https://en.wikipedia.org/wiki/Geodesics_on_an_ellipsoid#Geodesics_on_a_triaxial_ellipsoid>`__

The History
--------------------------------------------------------------------------------

The bibliography of papers on the geodesic problem for an ellipsoid is
available at

* http://geographiclib.sourceforge.net/geodesic-papers/biblio.html

this includes links to online copies of the papers.
