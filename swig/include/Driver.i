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
 * Revision 1.13  2006/12/05 02:02:33  fwarmerdam
 * fix options support for CreateCopy()
 *
 * Revision 1.12  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 * Revision 1.11  2005/08/04 19:17:19  kruland
 * The Open() and OpenShared() methods were incrementing the gdal internal
 * reference count by mistake.
 *
 * Revision 1.10  2005/07/18 16:13:31  kruland
 * Added MajorObject.i an interface specification to the MajorObject baseclass.
 * Used inheritance in Band.i, Driver.i, and Dataset.i to access MajorObject
 * functionality.
 * Adjusted Makefile to have PYTHON be a variable, gdal wrapper depend on
 * MajorObject.i, use rm (instead of libtool's wrapped RM) for removal because
 * the libtool didn't accept -r.
 *
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

class GDALDriverShadow : public GDALMajorObjectShadow {
private:
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
    return ds;
  }

%newobject CreateCopy;
%feature( "kwargs" ) CreateCopy;
  GDALDatasetShadow *CreateCopy( const char *name, GDALDatasetShadow* src, int strict =1, char **options = 0 ) {
    GDALDatasetShadow *ds = (GDALDatasetShadow*) GDALCreateCopy(self, name, src, strict, options, 0, 0 );
    return ds;
  }

  int Delete( const char *name ) {
    return GDALDeleteDataset( self, name );
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

