/******************************************************************************
 * $Id$
 *
 * Name:     Dataset.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.1  2005/02/15 05:56:49  kruland
 * Created the Dataset shadow class definition.  Does not rely on the C++ api
 * in gdal_priv.h.  Need to remove the vector<>s and replace with fixed
 * size arrays.
 *
 *
*/

//************************************************************************
//
// Define the extensions for Dataset (nee GDALDataset)
//
//************************************************************************

%rename (Dataset) GDALDataset;

class GDALDataset {
private:
  GDALDataset();
public:
%extend {

%immutable;
  int RasterXSize;
  int RasterYSize;
%mutable;

  GDALRasterBand* GetRasterBand(int nBand ) {
    return (GDALRasterBand*) GDALGetRasterBand( self, nBand );
  }

  char const *GetProjection() {
    return GDALGetProjectionRef( self );
  }

  std::vector<double>
  GetGeoTransform() {
    double c_transform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    std::vector<double> retval(6);
    if ( GDALGetGeoTransform( self, c_transform ) != 0 ) {
      retval[1] = 1.0;
      retval[5] = 1.0;
    }
    else {
      std::copy( c_transform, c_transform+6, retval.begin() );
    }
    return retval;
  }

  int SetGeoTransform( std::vector<double> trans ) {
    double c_transform[6];
    std::copy( trans.begin(), trans.begin()+6, c_transform );
    return GDALSetGeoTransform( self, c_transform );
  }

%apply (char **dict) { char ** };
  char ** GetMetadata( const char * pszDomain = "" ) {
    return GDALGetMetadata( self, pszDomain );
  }
%clear char **;

%apply (char **dict) { char ** papszMetadata };
  CPLErr SetMetadata( char ** papszMetadata, const char * pszDomain = "" ) {
    return GDALSetMetadata( self, papszMetadata, pszDomain );
  }
%clear char **papszMetadata;

  // The int,int* arguments are typemapped.  The name of the first argument
  // becomes the kwarg name for it.
%feature("kwargs") BuildOverviews;
%apply (int nList, int* pList) { (int overviewlist, int *pOverviews) };
  int BuildOverviews( const char *resampling = "NEAREST", int overviewlist = 0 , int *pOverviews = 0 ) {
    return GDALBuildOverviews( self, resampling, overviewlist, pOverviews, 0, 0, 0, 0);
  }
%clear (int overviewlist, int *pOverviews);

  const char *GetGCPProjection() {
    return GDALGetGCPProjection( self );
  }

  void GetGCPs( int *nGCPs, GDAL_GCP const **pGCPs ) {
    *nGCPs = GDALGetGCPCount( self );
    *pGCPs = GDALGetGCPs( self );
  }
}
};

%{
int GDALDataset_RasterXSize_get( GDALDataset *h ) {
  return GDALGetRasterXSize( h );
}
int GDALDataset_RasterYSize_get( GDALDataset *h ) {
  return GDALGetRasterYSize( h );
}
%}
