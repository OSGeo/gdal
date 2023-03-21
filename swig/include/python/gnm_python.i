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
  // Will be turned on for GDAL 4.0
  // UseExceptions();

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

// Start: to be removed in GDAL 4.0

// Issue a FutureWarning in a number of functions and methods that will
// be impacted when exceptions are enabled by default

%pythoncode %{
hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions = False

def _WarnIfUserHasNotSpecifiedIfUsingExceptions():
    global hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions
    if not hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions and not _UserHasSpecifiedIfUsingExceptions():
        hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions = True
        import warnings
        warnings.warn(
            "Neither gnm.UseExceptions() nor gnm.DontUseExceptions() has been explicitly called. " +
            "In GDAL 4.0, exceptions will be enabled by default. " +
            "You may also call gdal.UseExceptionsAllModules() to enable exceptions in all GDAL related modules.", FutureWarning)
%}

%pythonprepend CastToNetwork %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythonprepend CastToGenericNetwork %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

// End: to be removed in GDAL 4.0

%extend GDALMajorObjectShadow {
%pythoncode %{
  def GetMetadata( self, domain = '' ):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List( domain )
    return self.GetMetadata_Dict( domain )
%}
}
#endif

