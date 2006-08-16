/******************************************************************************
 * $Id$
 *
 * Name:     Band.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.23  2006/08/16 14:41:01  fwarmerdam
 * Fix up GetBlockSize().
 *
 * Revision 1.22  2006/08/05 00:58:31  fwarmerdam
 * Added GetBlockSize().
 *
 * Revision 1.21  2006/07/13 16:05:53  fwarmerdam
 * Added GetStatistics() and SetStatistics().
 *
 * Revision 1.20  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 * Revision 1.19  2005/08/06 20:51:58  kruland
 * Instead of using double_## defines and SWIG macros, use typemaps with
 * [ANY] specified and use $dim0 to extract the dimension.  This makes the
 * code quite a bit more readable.
 *
 * Revision 1.18  2005/08/04 20:48:32  kruland
 * Recoded GetNoDataValue(), GetMaximum(), GetMinimum(), GetOffset(), GetScale() to have
 * access to both the double return value and the has value flag.
 *
 * Revision 1.17  2005/08/04 19:16:06  kruland
 * GetRasterColorTable does not return a newobject.
 *
 * Revision 1.16  2005/07/20 16:28:26  kruland
 * Merge the two definitions of Checksum into one.  Use the pointer trick
 * to allow the passing of optional xsize,ysize parameters.
 *
 * Revision 1.15  2005/07/18 16:13:31  kruland
 * Added MajorObject.i an interface specification to the MajorObject baseclass.
 * Used inheritance in Band.i, Driver.i, and Dataset.i to access MajorObject
 * functionality.
 * Adjusted Makefile to have PYTHON be a variable, gdal wrapper depend on
 * MajorObject.i, use rm (instead of libtool's wrapped RM) for removal because
 * the libtool didn't accept -r.
 *
 * Revision 1.14  2005/07/18 15:41:27  kruland
 * Added Set/Get Description.  Some comment changes.
 *
 * Revision 1.13  2005/07/15 19:00:24  kruland
 * Implement two GetMetadata methods, one which returns a dict and one
 * which returns a List.  Have python code determine which to use.
 *
 * Revision 1.12  2005/07/15 18:33:57  kruland
 * Added SetMetadata with single string argument.
 *
 * Revision 1.11  2005/03/10 17:19:08  hobu
 * #ifdefs for csharp
 *
 * Revision 1.10  2005/02/23 17:44:00  kruland
 * Added GetMetadata, SetMetadata, SetRasterColorInterpretation.
 *
 * Revision 1.9  2005/02/22 23:27:39  kruland
 * Implement GetRasterColorTable/SetRasterColorTable.
 *
 * Revision 1.8  2005/02/20 19:42:53  kruland
 * Rename the Swig shadow classes so the names do not give the impression that
 * they are any part of the GDAL/OSR apis.  There were no bugs with the old
 * names but they were confusing.
 *
 * Revision 1.7  2005/02/17 17:27:13  kruland
 * Changed the handling of fixed size double arrays to make it fit more
 * naturally with GDAL/OSR usage.  Declare as typedef double * double_17;
 * If used as return argument use:  function ( ... double_17 argout ... );
 * If used as value argument use: function (... double_17 argin ... );
 *
 * Revision 1.6  2005/02/17 04:12:09  kruland
 * Reimplement Band::ReadRaster Band::WriteRaster so they use optional int
 * argument.  This allows keyword arguments in some languages.  Added a
 * sanity check to WriteRaster to ensure the buffer string is long enough.
 * Fixed FlushCache by moving it into the class.
 *
 * Revision 1.5  2005/02/16 18:41:14  kruland
 * Implemented more methods.  Commented the ones still missing.
 *
 * Revision 1.4  2005/02/15 19:50:39  kruland
 * Fixed ReadRaster/WriteRasters.  They need to use buffers of char * with
 * explicit length because they can contain '\0'.
 *
 * Revision 1.3  2005/02/15 18:56:52  kruland
 * Added support for WriteRaster().
 *
 * Revision 1.2  2005/02/15 16:56:46  kruland
 * Remove use of vector<double> in ComputeRasterMinMax.  Use double_2 instead.
 *
 * Revision 1.1  2005/02/15 06:23:48  kruland
 * Extracted Band class (GDALRasterBandShadow) into seperate .i file.  Does not use
 * C++ API.
 *
 *
*/

/************************************************************************
 *
 * Define the extensions for Band (nee GDALRasterBandShadow)
 *
*************************************************************************/
%{
static
CPLErr ReadRaster_internal( GDALRasterBandShadow *obj, 
                            int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            int *buf_size, char **buf )
{

  *buf_size = buf_xsize * buf_ysize * GDALGetDataTypeSize( buf_type ) / 8;
  *buf = (char*) malloc( *buf_size );
  CPLErr result =  GDALRasterIO( obj, GF_Read, xoff, yoff, xsize, ysize,
                                 (void *) *buf, buf_xsize, buf_ysize,
                                 buf_type, 0, 0 );
  if ( result != CE_None ) {
    free( *buf );
    *buf = 0;
    *buf_size = 0;
  }
  return result;
}

static
CPLErr WriteRaster_internal( GDALRasterBandShadow *obj,
                             int xoff, int yoff, int xsize, int ysize,
                             int buf_xsize, int buf_ysize,
                             GDALDataType buf_type,
                             int buf_size, char *buffer )
{
    if ( buf_size < buf_xsize * buf_ysize * GDALGetDataTypeSize( buf_type) /8 ) {
      return CE_Failure;
    }

    return GDALRasterIO( obj, GF_Write, xoff, yoff, xsize, ysize, 
		        (void *) buffer, buf_xsize, buf_ysize, buf_type, 0, 0 );
}
%}

%rename (Band) GDALRasterBandShadow;

class GDALRasterBandShadow : public GDALMajorObjectShadow {
private:
  GDALRasterBandShadow();
  ~GDALRasterBandShadow();
public:
%extend {

%immutable;
  int XSize;
  int YSize;
  GDALDataType DataType;
%mutable;

%apply (int *OUTPUT){int *pnBlockXSize, int *pnBlockYSize}

  void GetBlockSize(int *pnBlockXSize, int *pnBlockYSize) {
      GDALGetBlockSize(self, pnBlockXSize, pnBlockYSize);
  }

  GDALColorInterp GetRasterColorInterpretation() {
    return GDALGetRasterColorInterpretation( self );
  }

  CPLErr SetRasterColorInterpretation( GDALColorInterp val ) {
    return GDALSetRasterColorInterpretation( self, val );
  }

  void GetNoDataValue( double *val, int *hasval ) {
    *val = GDALGetRasterNoDataValue( self, hasval );
  }

  CPLErr SetNoDataValue( double d) {
    return GDALSetRasterNoDataValue( self, d );
  }

  void GetMinimum( double *val, int *hasval ) {
    *val = GDALGetRasterMinimum( self, hasval );
  }

  void GetMaximum( double *val, int *hasval ) {
    *val = GDALGetRasterMaximum( self, hasval );
  }

  void GetOffset( double *val, int *hasval ) {
    *val = GDALGetRasterOffset( self, hasval );
  }

  void GetScale( double *val, int *hasval ) {
    *val = GDALGetRasterScale( self, hasval );
  }

  CPLErr GetStatistics( int approx_ok, int force, 
                      double *min, double *max, double *mean, double *stddev ){
    return GDALGetRasterStatistics( self, approx_ok, force, 
				    min, max, mean, stddev );
  }

  CPLErr SetStatistics( double min, double max, double mean, double stddev ) {
    return GDALSetRasterStatistics( self, min, max, mean, stddev );
  }

  int GetOverviewCount() {
    return GDALGetOverviewCount( self );
  }

  GDALRasterBandShadow *GetOverview(int i) {
    return (GDALRasterBandShadow*) GDALGetOverview( self, i );
  }

%apply (int *optional_int) {(int*)};
%feature ("kwargs") Checksum;
  int Checksum( int xoff = 0, int yoff = 0, int *xsize = 0, int *ysize = 0) {
    int nxsize = (xsize!=0) ? *xsize : GDALGetRasterBandXSize( self );
    int nysize = (ysize!=0) ? *ysize : GDALGetRasterBandYSize( self );
    return GDALChecksumImage( self, xoff, yoff, nxsize, nysize );
  }
%clear (int*);

  void ComputeRasterMinMax( double argout[2], int approx_ok = 0) {
    GDALComputeRasterMinMax( self, approx_ok, argout );
  }

  CPLErr Fill( double real_fill, double imag_fill =0.0 ) {
    return GDALFillRaster( self, real_fill, imag_fill );
  }

%apply ( int *nLen, char **pBuf ) { (int *buf_len, char **buf ) };
%apply ( int *optional_int ) {(int*)};
%feature( "kwargs" ) ReadRaster;
  CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                     int *buf_len, char **buf,
                     int *buf_xsize = 0,
                     int *buf_ysize = 0,
                     int *buf_type = 0 ) {
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(self)
                                        : (GDALDataType)*buf_type;
    return ReadRaster_internal( self, xoff, yoff, xsize, ysize,
                                nxsize, nysize, ntype, buf_len, buf );
  }
%clear (int *buf_len, char **buf );
%clear (int*);

%apply (int nLen, char *pBuf) { (int buf_len, char *buf_string) };
%apply ( int *optional_int ) {(int*)};
%feature( "kwargs" ) WriteRaster;
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                      int buf_len, char *buf_string,
                      int *buf_xsize = 0,
                      int *buf_ysize = 0,
                      int *buf_type = 0 ) {
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(self)
                                        : (GDALDataType)*buf_type;
    return WriteRaster_internal( self, xoff, yoff, xsize, ysize,
                                 nxsize, nysize, ntype, buf_len, buf_string );
  }
%clear (int buf_len, char *buf_string);
%clear (int*);

  void FlushCache() {
    GDALFlushRasterCache( self );
  }

  GDALColorTable *GetRasterColorTable() {
    return (GDALColorTable*) GDALGetRasterColorTable( self );
  }

  int SetRasterColorTable( GDALColorTable *arg ) {
    return GDALSetRasterColorTable( self, arg );
  }

/* NEEDED */
/* ReadAsArray */
/* WriteArray */
/* GetHistogram */
/* GetStatistics */
/* SetStatistics */
/* ComputeStatistics */
/* AdviseRead */

} /* %extend */

};

%{
GDALDataType GDALRasterBandShadow_DataType_get( GDALRasterBandShadow *h ) {
  return GDALGetRasterDataType( h );
}
int GDALRasterBandShadow_XSize_get( GDALRasterBandShadow *h ) {
  return GDALGetRasterBandXSize( h );
}
int GDALRasterBandShadow_YSize_get( GDALRasterBandShadow *h ) {
  return GDALGetRasterBandYSize( h );
}
%}
