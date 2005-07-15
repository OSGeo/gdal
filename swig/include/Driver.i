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
 * Revision 1.9  2005/07/15 16:55:46  kruland
 * Implemented SetDescription and GetDescription.
 *
 * Revision 1.8  2005/03/10 17:18:42  hobu
 * #ifdefs for csharp
 *
 * Revision 1.7  2005/02/24 16:33:07  kruland
 * Marked missing methods.
 *
 * Revision 1.6  2005/02/20 19:42:53  kruland
 * Rename the Swig shadow classes so the names do not give the impression that
 * they are any part of the GDAL/OSR apis.  There were no bugs with the old
 * names but they were confusing.
 *
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
*  Define the extensions for Driver (nee GDALDriverShadow)
*
*************************************************************************/


%rename (Driver) GDALDriverShadow;

class GDALDriverShadow {
  ~GDALDriverShadow();
  GDALDriverShadow();
public:
%extend {

%immutable;
  char const *ShortName;
  char const *LongName;
  char const *HelpTopic;
%mutable;

    
%newobject Create;
%feature( "kwargs" ) Create;
  GDALDatasetShadow *Create( const char *name, int xsize, int ysize, int bands =1,
                       GDALDataType eType=GDT_Byte, char **options = 0 ) {
    GDALDatasetShadow* ds = (GDALDatasetShadow*) GDALCreate( self, name, xsize, ysize, bands, eType, options );
    if ( ds != 0 )
      GDALDereferenceDataset( ds );
    return ds;
  }

%newobject CreateCopy;
%feature( "kwargs" ) CreateCopy;
  GDALDatasetShadow *CreateCopy( const char *name, GDALDatasetShadow* src, int strict =1, char **options = 0 ) {
    GDALDatasetShadow *ds = (GDALDatasetShadow*) GDALCreateCopy(self, name, src, strict, 0, 0, 0 );
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

  const char *GetDescription() {
    return GDALGetDescription( self );
  }

  void SetDescription( const char *pszNewDesc ) {
    GDALSetDescription( self, pszNewDesc );
  }

// NEEDED
// Register
// Deregister

}
};

%{
char const *GDALDriverShadow_ShortName_get( GDALDriverShadow *h ) {
  return GDALGetDriverShortName( h );
}
char const *GDALDriverShadow_LongName_get( GDALDriverShadow *h ) {
  return GDALGetDriverLongName( h );
}
char const *GDALDriverShadow_HelpTopic_get( GDALDriverShadow *h ) {
  return GDALGetDriverHelpTopic( h );
}
%}

#ifdef SWIGCSHARP
%{
char const *GDALDriverShadow_get_ShortName( GDALDriverShadow *h ) {
  return GDALGetDriverShortName( h );
}
char const *GDALDriverShadow_get_LongName( GDALDriverShadow *h ) {
  return GDALGetDriverLongName( h );
}
char const *GDALDriverShadow_get_HelpTopic( GDALDriverShadow *h ) {
  return GDALGetDriverHelpTopic( h );
}
%}
#endif