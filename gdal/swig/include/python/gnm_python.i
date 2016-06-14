/*
 * $Id: ogr_python.i 33763 2016-03-21 16:31:17Z rouault $
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

