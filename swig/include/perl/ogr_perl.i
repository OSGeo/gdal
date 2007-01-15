/*
 * $Id$
 *
 * perl specific code for ogr bindings.
 */

/*
 * $Log$
 * Revision 1.6  2006/11/19 20:07:35  ajolma
 * instead of renaming, create GetField as a copy of GetFieldAsString
 *
 * Revision 1.5  2006/11/19 17:42:24  ajolma
 * There is no sense in having typed versions of GetField in Perl, renamed GetFieldAsString to GetField
 *
 * Revision 1.4  2005/09/21 19:04:12  kruland
 * Need to %include cpl_exceptions.i
 *
 * Revision 1.3  2005/09/21 18:00:05  kruland
 * Turn on UseExceptions in ogr init code.
 *
 * Revision 1.2  2005/09/13 17:36:28  kruland
 * Whoops!  import typemaps_perl.i.
 *
 * Revision 1.1  2005/09/13 16:08:45  kruland
 * Added perl specific modifications for gdal and ogr.
 *
 *
 */

%init %{

  UseExceptions();
  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
  
%}

%include cpl_exceptions.i

%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

%import typemaps_perl.i

%extend OGRFeatureShadow {

  const char* GetField(int id) {
    return (const char *) OGR_F_GetFieldAsString(self, id);
  }

  const char* GetField(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetField");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return (const char *) OGR_F_GetFieldAsString(self, i);
    }
    return NULL;
  }

}