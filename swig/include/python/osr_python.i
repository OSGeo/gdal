/*
 * $Id$
 *
 * python specific code for ogr bindings.
 */

%feature("autodoc");

#ifndef FROM_GDAL_I
%{
#define MODULE_NAME           "osr"
%}

%include "python_exceptions.i"
%include "python_strings.i"
#endif

%include typemaps_python.i

%extend OSRSpatialReferenceShadow {
  %pythoncode %{

    def __init__(self, *args, **kwargs):
        """__init__(OSRSpatialReferenceShadow self, char const * wkt) -> SpatialReference"""
        oldval = _osr.GetUseExceptions()
        if not oldval:
            _osr.UseExceptions()
        try:
            this = _osr.new_SpatialReference(*args, **kwargs)
        finally:
            if not oldval:
                _osr.DontUseExceptions()
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this

  %}
}
