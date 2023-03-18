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
        try:
            with ExceptionMgr(useExceptions=True):
                this = _osr.new_SpatialReference(*args, **kwargs)
        finally:
            pass
        try:
            self.this.append(this)
        except __builtin__.Exception:
            self.this = this

  %}
}
