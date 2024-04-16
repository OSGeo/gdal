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
>>> ct.TransformBounds(-72.5, 44.2, -72.4, 44.3, 21)
(7415356.140468472, -51238192.683464445, 7454323.154814391, -51210287.42581475)

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
>>> ct.TransformPoints([(-72.58, 44.26), (-72.59, 44.26)])
[(7390620.052019633, -51202148.77747277, 0.0), (7387261.070131293, -51200373.68798984, 0.0)]

>>> import numpy as np
>>> ct.TransformPoints(np.array([[-72.58, 44.26], [-72.59, 44.26]]))
[(7390620.052019633, -51202148.77747277, 0.0), (7387261.070131293, -51200373.68798984, 0.0)]


";

}
