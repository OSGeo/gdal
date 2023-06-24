/*
 * $Id$
 *
 * python specific code for ogr bindings.
 */

%feature("autodoc");

%init %{
  // Will be turned on for GDAL 4.0
  // UseExceptions();
%}

#ifndef FROM_GDAL_I
%{
#define MODULE_NAME           "osr"
%}

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

        try:
            with ExceptionMgr(useExceptions=True):
                this = _osr.new_SpatialReference(*args, **kwargs)
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

  %}
}
