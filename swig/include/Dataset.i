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
 * Revision 1.12  2005/07/15 19:00:55  kruland
 * Implement the SetMetadata/GetMetadata methods as in Band.i
 *
 * Revision 1.11  2005/07/15 16:55:21  kruland
 * Implemented SetDescription and GetDescription.
 *
 * Revision 1.10  2005/03/10 17:18:55  hobu
 * #ifdefs for csharp
 *
 * Revision 1.9  2005/02/23 21:37:18  kruland
 * Added GetProjectionRef().  Commented missing methods.
 *
 * Revision 1.8  2005/02/23 17:46:39  kruland
 * Added r/o attribute RasterCount.
 * Added AddBand method.
 * Added WriteRaster method.
 *
 * Revision 1.7  2005/02/21 14:51:32  kruland
 * Needed to rename GDALDriver to GDALDriverShadow in the last commit.
 *
 * Revision 1.6  2005/02/20 19:42:53  kruland
 * Rename the Swig shadow classes so the names do not give the impression that
 * they are any part of the GDAL/OSR apis.  There were no bugs with the old
 * names but they were confusing.
 *
 * Revision 1.5  2005/02/17 17:27:13  kruland
 * Changed the handling of fixed size double arrays to make it fit more
 * naturally with GDAL/OSR usage.  Declare as typedef double * double_17;
 * If used as return argument use:  function ( ... double_17 argout ... );
 * If used as value argument use: function (... double_17 argin ... );
 *
 * Revision 1.4  2005/02/16 17:41:19  kruland
 * Added a few more methods to Dataset and marked the ones still missing.
 *
 * Revision 1.3  2005/02/15 20:50:49  kruland
 * Added SetProjection.
 *
 * Revision 1.2  2005/02/15 16:53:36  kruland
 * Removed use of vector<double> in the ?etGeoTransform() methods.  Use fixed
 * length double array type instead.
 *
 * Revision 1.1  2005/02/15 05:56:49  kruland
 * Created the Dataset shadow class definition.  Does not rely on the C++ api
 * in gdal_priv.h.  Need to remove the vector<>s and replace with fixed
 * size arrays.
 *
 *
*/

//************************************************************************
//
// Define the extensions for Dataset (nee GDALDatasetShadow)
//
//************************************************************************

%rename (Dataset) GDALDatasetShadow;

class GDALDatasetShadow {
private:
  GDALDatasetShadow();
public:
%extend {

%immutable;
  int RasterXSize;
  int RasterYSize;
  int RasterCount;
//
// Needed
// _band list?
%mutable;

  ~GDALDatasetShadow() {
    if ( GDALDereferenceDataset( self ) <= 0 ) {
      GDALClose(self);
    }
  }

  GDALDriverShadow* GetDriver() {
    return (GDALDriverShadow*) GDALGetDatasetDriver( self );
  }

  GDALRasterBandShadow* GetRasterBand(int nBand ) {
    return (GDALRasterBandShadow*) GDALGetRasterBand( self, nBand );
  }

  char const *GetProjection() {
    return GDALGetProjectionRef( self );
  }

  char const *GetProjectionRef() {
    return GDALGetProjectionRef( self );
  }

  CPLErr SetProjection( char const *prj ) {
    return GDALSetProjection( self, prj );
  }

  void GetGeoTransform( double_6 argout ) {
    if ( GDALGetGeoTransform( self, argout ) != 0 ) {
      argout[0] = 0.0;
      argout[1] = 1.0;
      argout[2] = 0.0;
      argout[3] = 0.0;
      argout[4] = 0.0;
      argout[5] = 1.0;
    }
  }

  CPLErr SetGeoTransform( double_6 argin ) {
    return GDALSetGeoTransform( self, argin );
  }

%apply (char **dict) { char ** };
  char ** GetMetadata_Dict( const char * pszDomain = "" ) {
    return GDALGetMetadata( self, pszDomain );
  }
%clear char **;

%apply (char **options) {char **};
  char **GetMetadata_List( const char *pszDomain = "" ) {
    return GDALGetMetadata( self, pszDomain );
  }
%clear char **;

#ifdef SWIGPYTHON
%pythoncode {
  def GetMetadata( self, domain = '' ):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List( domain )
    return self.GetMetadata_Dict( domain )
}
#else
%rename GetMetadata_Dict (GetMetadata)
#endif

%apply (char **dict) { char ** papszMetadata };
  CPLErr SetMetadata( char ** papszMetadata, const char * pszDomain = "" ) {
    return GDALSetMetadata( self, papszMetadata, pszDomain );
  }
%clear char **papszMetadata;

  CPLErr SetMetadata( char * pszMetadataString , const char *pszDomain = "" ) {
    char *tmpList[2];
    tmpList[0] = pszMetadataString;
    tmpList[1] = 0;
    return GDALSetMetadata( self, tmpList, pszDomain );
  }

  // The (int,int*) arguments are typemapped.  The name of the first argument
  // becomes the kwarg name for it.
%feature("kwargs") BuildOverviews;
%apply (int nList, int* pList) { (int overviewlist, int *pOverviews) };
  int BuildOverviews( const char *resampling = "NEAREST",
                      int overviewlist = 0 , int *pOverviews = 0 ) {
    return GDALBuildOverviews( self, resampling, overviewlist, pOverviews, 0, 0, 0, 0);
  }
%clear (int overviewlist, int *pOverviews);

  int GetGCPCount() {
    return GDALGetGCPCount( self );
  }

  const char *GetGCPProjection() {
    return GDALGetGCPProjection( self );
  }

  void GetGCPs( int *nGCPs, GDAL_GCP const **pGCPs ) {
    *nGCPs = GDALGetGCPCount( self );
    *pGCPs = GDALGetGCPs( self );
  }

  CPLErr SetGCPs( int nGCPs, GDAL_GCP const *pGCPs, const char *pszGCPProjection ) {
    return GDALSetGCPs( self, nGCPs, pGCPs, pszGCPProjection );
  }

  void FlushCache() {
    GDALFlushCache( self );
  }

%feature ("kwargs") AddBand;
/* uses the defined char **options typemap */
  CPLErr AddBand( GDALDataType datatype = GDT_Byte, char **options = 0 ) {
    return GDALAddBand( self, datatype, options );
  }

%feature("kwargs") WriteRaster;
%apply (int nLen, char *pBuf) { (int buf_len, char *buf_string) };
%apply (int *optional_int) { (int*) };
%apply (int *optional_int) { (GDALDataType *buf_type) };
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
	              int buf_len, char *buf_string,
                      int *buf_xsize = 0, int *buf_ysize = 0,
                      GDALDataType *buf_type = 0,
                      int band_list = 0, int *pband_list = 0 ) {
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype;
    if ( buf_type != 0 ) {
      ntype = (GDALDataType) *buf_type;
    } else {
      int lastband = GDALGetRasterCount( self ) - 1;
      ntype = GDALGetRasterDataType( GDALGetRasterBand( self, lastband ) );
    }
    bool myBandList = false;
    int nBandCount;
    int *pBandList;
    if ( band_list != 0 ) {
      myBandList = false;
      nBandCount = band_list;
      pBandList = pband_list;
    }
    else {
      myBandList = true;
      nBandCount = GDALGetRasterCount( self );
      pBandList = (int*) CPLMalloc( sizeof(int) * nBandCount );
      for( int i = 0; i< nBandCount; ++i ) {
        pBandList[i] = i;
      }
    }
    return GDALDatasetRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                (void*) buf_string, nxsize, nysize, ntype,
                                band_list, pband_list, 0, 0, 0 );
    if ( myBandList ) {
       CPLFree( pBandList );
    }
  }
%clear (int band_list, int *pband_list );
%clear (GDALDataType *buf_type);
%clear (int*);
%clear (int buf_len, char *buf_string);

  const char *GetDescription() {
    return GDALGetDescription( self );
  }

  void SetDescription( const char *pszNewDesc ) {
    GDALSetDescription( self, pszNewDesc );
  }

/* NEEDED */
/* GetSubDatasets */
/* ReadAsArray */
/* AddBand */
/* AdviseRead */
/* ReadRaster */
  
} /* extend */
}; /* GDALDatasetShadow */

%{
int GDALDatasetShadow_RasterXSize_get( GDALDatasetShadow *h ) {
  return GDALGetRasterXSize( h );
}
int GDALDatasetShadow_RasterYSize_get( GDALDatasetShadow *h ) {
  return GDALGetRasterYSize( h );
}
int GDALDatasetShadow_RasterCount_get( GDALDatasetShadow *h ) {
  return GDALGetRasterCount( h );
}
%}

#ifdef SWIGCSHARP
%{
int GDALDatasetShadow_get_RasterXSize( GDALDatasetShadow *h ) {
  return GDALGetRasterXSize( h );
}
int GDALDatasetShadow_get_RasterYSize( GDALDatasetShadow *h ) {
  return GDALGetRasterYSize( h );
}
int GDALDatasetShadow_get_RasterCount( GDALDatasetShadow *h ) {
  return GDALGetRasterCount( h );
}
%}
#endif