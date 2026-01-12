/*
 *
 * python specific code for ogr bindings.
 */

// Disable C-style signatures in Python docstrings (see #12177)
%feature("autodoc", "0");

%init %{
  // Will be turned on for GDAL 4.0
  // UseExceptions();
%}

#ifndef FROM_GDAL_I
%{
#define MODULE_NAME           "osr"
%}

%include "osr_docs.i"
%include "osr_spatialreference_docs.i"
%include "osr_coordinatetransformation_docs.i"
%include "python_exceptions.i"
%include "python_strings.i"

#endif

%include typemaps_python.i

// Start: to be removed in GDAL 4.0

// Issue a FutureWarning in a number of functions and methods that will
// be impacted when exceptions are enabled by default

%pythoncode %{
hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions = False

def _WarnIfUserHasNotSpecifiedIfUsingExceptions():
    from . import gdal
    if not hasattr(gdal, "hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions") and not _UserHasSpecifiedIfUsingExceptions():
        gdal.hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions = True
        import warnings
        warnings.warn(
            "Neither osr.UseExceptions() nor osr.DontUseExceptions() has been explicitly called. " +
            "In GDAL 4.0, exceptions will be enabled by default.", FutureWarning)
%}

// End: to be removed in GDAL 4.0

%extend OSRSpatialReferenceShadow {
  %pythoncode %{

    def __init__(self, *args, **kwargs):
        """__init__(OSRSpatialReferenceShadow self, char const * wkt) -> SpatialReference"""

        _WarnIfUserHasNotSpecifiedIfUsingExceptions()

        user_input = None

        if len(args) + len(kwargs) > 1:
            # We could pass additional kwargs as options to SetFromUserInput,
            # but that is not commonly needed and limits our ability to
            # validate arguments here.
            raise ValueError("Unexpected argument to SpatialReference")

        if kwargs:
            if "wkt" in kwargs:
                user_input = kwargs["wkt"]
            elif "name" in kwargs:
                user_input = kwargs["name"]
            elif "epsg" in kwargs:
                user_input = f'EPSG:{kwargs["epsg"]}'
            else:
                raise ValueError("Unexpected argument to SpatialReference")

        if args:
            if type(args[0]) is dict:
                import json
                user_input = json.dumps(args[0])
            else:
                user_input = args[0]

        try:
            with ExceptionMgr(useExceptions=True):
                this = _osr.new_SpatialReference()
        finally:
            pass
        if hasattr(_osr, "SpatialReference_swiginit"):
            # SWIG 4 way
            _osr.SpatialReference_swiginit(self, this)
        else:
            # SWIG < 4 way
            try:
                self.this.append(this)
            except __builtin__.Exception:
                self.this = this

        if user_input:
            self.SetFromUserInput(user_input)

  %}

%feature("shadow") ImportFromCF1 %{
    def ImportFromCF1(self, keyValues, units = ""):
        """ Import a CRS from netCDF CF-1 definitions.

        http://cfconventions.org/cf-conventions/cf-conventions.html#appendix-grid-mappings
        """
        import copy
        keyValues = copy.deepcopy(keyValues)
        for key in keyValues:
            val = keyValues[key]
            if isinstance(val, list):
                val = ','.join(["%.18g" % x for x in val])
                keyValues[key] = val
        return $action(self, keyValues, units)
%}

%feature("shadow") ExportToCF1 %{
    def ExportToCF1(self, options = {}):
        """ Export a CRS to netCDF CF-1 definitions.

        http://cfconventions.org/cf-conventions/cf-conventions.html#appendix-grid-mappings
        """
        keyValues = $action(self, options)
        for key in keyValues:
            val = keyValues[key]
            try:
                val = float(val)
                keyValues[key] = val
            except:
                try:
                    val = [float(x) for x in val.split(',')]
                    keyValues[key] = val
                except:
                    pass
        return keyValues
%}
}

%extend OSRCoordinateTransformationShadow {

%feature("shadow") TransformPoint %{

def TransformPoint(self, *args):
    """
    TransformPoint(CoordinateTransformation self, double [3] inout)
    TransformPoint(CoordinateTransformation self, double [4] inout)
    TransformPoint(CoordinateTransformation self, double x, double y, double z=0.0)
    TransformPoint(CoordinateTransformation self, double x, double y, double z, double t)

    Transform a single point.

    See :cpp:func:`OCTTransform`.

    Returns
    -------
    tuple
        A tuple of (x, y, z) or (x, y, z, t) values, depending on the input.

    Examples
    --------
    >>> wgs84 = osr.SpatialReference()
    >>> wgs84.ImportFromEPSG(4326)
    0
    >>> vt_sp = osr.SpatialReference()
    >>> vt_sp.ImportFromEPSG(5646)
    0
    >>> ct = osr.CoordinateTransformation(wgs84, vt_sp)
    >>> # Transform a point from WGS84 lat/long to Vermont State Plane easting/northing
    >>> ct.TransformPoint(44.26, -72.58)
    (1619458.11, 641509.19, 0.0) # rtol: 1e-6
    >>> ct.TransformPoint(44.26, -72.58, 103)
    (1619458.11, 641509.19, 103.0) # rtol: 1e-6
    """

    import collections.abc
    if len(args) == 1 and isinstance(args[0], collections.abc.Sequence):
        len_args = len(args[0])
        if len_args == 3:
            return self._TransformPoint3Double(args[0])
        elif len_args == 4:
            return self._TransformPoint4Double(args[0])

    return $action(self, *args)
%}

}
