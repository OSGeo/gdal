/******************************************************************************
 * $Id$
 *
 * Name:     Driver.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.5  2005/02/16 17:11:27  kruland
 * Added r/o data members for LongName, ShortName, and HelpTopic.
 *
 * Revision 1.4  2005/02/15 06:24:31  kruland
 * Added out typemap for GetMetadata.  (Removed extra log from comments)
 *
 * Revision 1.3  2005/02/15 05:57:43  kruland
 * Moved the swig %newobject and %feature decls to immedately before the function
 * def.  Improves readability.
 *
 * Revision 1.2  2005/02/14 23:58:46  hobu
 * Added log info and C99-style comments
 *
*/

/*************************************************************************
*
*  Define the extensions for Driver (nee GDALDriver)
*
*************************************************************************/


%rename (Driver) GDALDriver;

class GDALDriver {
public:
%extend {

%immutable;
  char const *ShortName;
  char const *LongName;
  char const *HelpTopic;
%mutable;

    
%newobject Create;
%feature( "kwargs" ) Create;
  GDALDataset *Create( const char *name, int xsize, int ysize, int bands =1,
                       GDALDataType eType=GDT_Byte, char **options = 0 ) {
    GDALDataset* ds = (GDALDataset*) GDALCreate( self, name, xsize, ysize, bands, eType, options );
    if ( ds != 0 )
      GDALDereferenceDataset( ds );
    return ds;
  }

%newobject CreateCopy;
%feature( "kwargs" ) CreateCopy;
  GDALDataset *CreateCopy( const char *name, GDALDataset* src, int strict =1, char **options = 0 ) {
    GDALDataset *ds = (GDALDataset*) GDALCreateCopy(self, name, src, strict, 0, 0, 0 );
    if ( ds != 0 )
      GDALDereferenceDataset( ds );
    return ds;
  }

  int Delete( const char *name ) {
    return GDALDeleteDataset( self, name );
  }

%apply (char **dict) { char ** };
  char **GetMetadata( const char * pszDomain = "" ) {
    return GDALGetMetadata( self, pszDomain );
  }
%clear char **;

}
};

%{
char const *GDALDriver_ShortName_get( GDALDriver *h ) {
  return GDALGetDriverShortName( h );
}
char const *GDALDriver_LongName_get( GDALDriver *h ) {
  return GDALGetDriverLongName( h );
}
char const *GDALDriver_HelpTopic_get( GDALDriver *h ) {
  return GDALGetDriverHelpTopic( h );
}
%}