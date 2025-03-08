%feature("docstring") OSRCoordinateTransformationShadow "
Python proxy of an :cpp:class:`OGRCoordinateTransformation`.
";

%extend OSRCoordinateTransformationShadow {

%feature("docstring") TransformBounds "

Transform a boundary, densifying the edges to account for nonlinear
transformations along these edges.

See :cpp:func:`OCTTransformBounds`.

Parameters
----------
minx : float
    Minimum bounding coordinate of the first axis in source CRS
miny : float
    Minimum bounding coordinate of the second axis in source CRS
maxx : float
    Maximum bounding coordinate of the first axis in source CRS
maxy : float
    Maximum bounding coordinate of the second axis in source CRS
densify_pts : int
    The number of points to use to densify the bounding polygon.
    Recommended to use 21.

Returns
-------
tuple
    Transformed values of xmin, ymin, xmax, ymax

Examples
--------
>>> wgs84 = osr.SpatialReference()
>>> wgs84.ImportFromEPSG(4326)
0
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> ct = osr.CoordinateTransformation(wgs84, vt_sp)
>>> ct.TransformBounds(44.2,-72.5, 44.3, -72.4, 21)
(1640416.6667, 619626.4287465283, 1666641.4901271078, 656096.7597360199)

";

// TransformPoint is documented inline

%feature("docstring") TransformPointWithErrorCode "

Variant of :py:meth:`TransformPoint` that provides an error code.

See :cpp:func:`OCTTransformEx`.

Parameters
----------
x : float
y : float
z : float
t : float

Returns
-------
tuple
    tuple of (x, y, z, t, error) values

";

%feature("docstring") TransformPoints "

Transform multiple points.

See :cpp:func:`OCTTransform`.

Parameters
----------
arg
    A list of tuples, or a 2xN, 3xN, or 4xN numpy array

Returns
-------
list
    A list of tuples of (x, y, z) or (x, y, z, t) values, depending on the input.

Examples
--------
>>> wgs84 = osr.SpatialReference()
>>> wgs84.ImportFromEPSG(4326)
0
>>> vt_sp = osr.SpatialReference()
>>> vt_sp.ImportFromEPSG(5646)
0
>>> ct = osr.CoordinateTransformation(wgs84, vt_sp)
>>> # Transform two points from WGS84 lat/long to Vermont State Plane easting/northing
>>> ct.TransformPoints([(44.26, -72.58), (44.26, -72.59)])
[(1619458.1108559777, 641509.1883246159, 0.0), (1616838.2913193079, 641511.9008312856, 0.0)]

>>> import numpy as np
>>> ct.TransformPoints(np.array([[44.26, -72.58], [44.26, -72.59]]))
[(1619458.1108559777, 641509.1883246159, 0.0), (1616838.2913193079, 641511.9008312856, 0.0)]

";

}
