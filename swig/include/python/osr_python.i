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

%pythoncode %{

import contextlib
@contextlib.contextmanager
def enable_exceptions():
    """Temporarily enable exceptions.

       Note: this will only affect the osgeo.osr module. For gdal or ogr
       modules, use respectively osgeo.gdal.enable_exceptions() and
       osgeo.ogr.enable_exceptions().

       Returns
       -------
            A context manager

       Example
       -------

           with osr.enable_exceptions():
               srs = osr.SpatialReference()
               srs.ImportFromEPSG(code)
    """
    if GetUseExceptions():
        yield
    else:
        UseExceptions()
        try:
            yield
        finally:
            DontUseExceptions()

%}
