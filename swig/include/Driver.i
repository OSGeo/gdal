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
 * Revision 1.2  2005/02/14 23:58:46  hobu
 * Added log info and C99-style comments
 *
 * Revision 1.4  2005/02/14 23:50:16  hobu
 * Added log info
 *
*/

/*************************************************************************
*
*  Define the extensions for Driver (nee GDALDriver)
*
*************************************************************************/


%rename (Driver) GDALDriver;
%newobject GDALDriver::Create;
%feature( "kwargs" ) GDALDriver::Create;
%newobject GDALDriver::CreateCopy;
%feature( "kwargs" ) GDALDriver::CreateCopy;

class GDALDriver {
public:
  %extend {
    
  GDALDataset *Create( const char *name, int xsize, int ysize, int bands =1,
                       GDALDataType eType=GDT_Byte, char **options = 0 ) {
    GDALDataset* ds = (GDALDataset*) GDALCreate( self, name, xsize, ysize, bands, eType, options );
    if ( ds != 0 )
      GDALDereferenceDataset( ds );
    return ds;
  }

  GDALDataset *CreateCopy( const char *name, GDALDataset* src, int strict =1, char **options = 0 ) {
    GDALDataset *ds = (GDALDataset*) GDALCreateCopy(self, name, src, strict, 0, 0, 0 );
    if ( ds != 0 )
      GDALDereferenceDataset( ds );
    return ds;
  }

  int Delete( const char *name ) {
    return GDALDeleteDataset( self, name );
  }

  char **GetMetadata( const char * pszDomain = "" ) {
    return GDALGetMetadata( self, pszDomain );
  }
}
};

