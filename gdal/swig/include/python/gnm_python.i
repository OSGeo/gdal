/*
 * $Id$
 *
 * python specific code for ogr bindings.
 */

%feature("autodoc");

#ifndef FROM_GDAL_I
%init %{

  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }

%}
#endif

%rename (RegisterAll) OGRRegisterAll();

#ifndef FROM_GDAL_I
%{
#define MODULE_NAME           "gnm"
%}
%include "python_exceptions.i"
%include "python_strings.i"

%import typemaps_python.i

%include "callback.i"


%extend GDALMajorObjectShadow {
%pythoncode %{
  def GetMetadata( self, domain = '' ):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List( domain )
    return self.GetMetadata_Dict( domain )
%}
}
#endif

