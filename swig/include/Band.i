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
 * Extracted Band class (GDALRasterBand) into seperate .i file.  Does not use
 * C++ API.
 *
 *
*/

//************************************************************************
//
// Define the extensions for Band (nee GDALRasterBand)
//
//************************************************************************
%{
static
CPLErr ReadRaster_internal( GDALRasterBand *obj, 
                            int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize, GDALDataType buf_type,
                            int *buf_size, char **buf )
{

  *buf_size = buf_xsize * buf_ysize * GDALGetDataTypeSize( buf_type ) / 8;
  *buf = (char*) malloc( *buf_size );
  CPLErr result =  GDALRasterIO( obj, GF_Read, xoff, yoff, xsize, ysize,
                                 (void *) *buf, buf_xsize, buf_ysize, buf_type, 0, 0 );
  if ( result != CE_None ) {
    free( *buf );
    *buf = 0;
    *buf_size = 0;
  }
  return result;
}

static
CPLErr WriteRaster_internal( GDALRasterBand *obj,
                             int xoff, int yoff, int xsize, int ysize,
                             char *buffer, int buf_xsize, int buf_ysize, GDALDataType buf_type )
{
    return GDALRasterIO( obj, GF_Write, xoff, yoff, xsize, ysize, 
		        (void *) buffer, buf_xsize, buf_ysize, buf_type, 0, 0 );
}
%}

%rename (Band) GDALRasterBand;

class GDALRasterBand {
private:
  GDALRasterBand();
public:
%extend {

%immutable;
  int XSize;
  int YSize;
  GDALDataType DataType;
%mutable;

  GDALColorInterp GetRasterColorInterpretation() {
    return GDALGetRasterColorInterpretation( self );
  }

  double GetNoDataValue() {
    int noval;
    return GDALGetRasterNoDataValue( self, &noval );
  }

  double GetMinimum() {
    int noval;
    return GDALGetRasterMinimum( self, &noval );
  }

  double GetMaximum() {
    int noval;
    return GDALGetRasterMaximum( self, &noval );
  }

  double GetOffset() {
    int noval;
    return GDALGetRasterOffset( self, &noval );
  }

  double GetScale() {
    int noval;
    return GDALGetRasterScale( self, &noval );
  }

  CPLErr SetNoDataValue( double d) {
    return GDALSetRasterNoDataValue( self, d );
  }

  int GetOverviewCount() {
    return GDALGetOverviewCount( self );
  }

  GDALRasterBand *GetOverview(int i) {
    return (GDALRasterBand*) GDALGetOverview( self, i );
  }

  int Checksum( int xoff, int yoff, int xsize, int ysize) {
    return GDALChecksumImage( self, xoff, yoff, xsize, ysize );
  }

  int Checksum( int xoff = 0, int yoff = 0 ) {
    int xsize = GDALGetRasterBandXSize( self );
    int ysize = GDALGetRasterBandYSize( self );
    return GDALChecksumImage( self, xoff, yoff, xsize, ysize );
  }

  void ComputeRasterMinMax( double_2 *c_minmax, int approx_ok = 0) {
    GDALComputeRasterMinMax( self, approx_ok, &(*c_minmax)[0] );
  }

  CPLErr Fill( double real_fill, double imag_fill =0.0 ) {
    return GDALFillRaster( self, real_fill, imag_fill );
  }

%apply ( int *nLen, char **pBuf ) { (int *buf_len, char **buf ) };
  CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                     int buf_xsize, int buf_ysize, GDALDataType buf_type,
                     int *buf_len, char **buf ) {
    return ReadRaster_internal( self, xoff, yoff, xsize, ysize,
                                buf_xsize, buf_ysize, buf_type, buf_len, buf );
  }

  CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                     int buf_xsize, int buf_ysize,
                     int *buf_len, char **buf ) {
    return ReadRaster_internal( self, xoff, yoff, xsize, ysize,
                           buf_xsize, buf_ysize, GDALGetRasterDataType(self), buf_len, buf );
  }

  CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                     int *buf_len, char **buf ) {
    return ReadRaster_internal( self, xoff, yoff, xsize, ysize,
                           xsize, ysize, GDALGetRasterDataType(self), buf_len, buf );
  }
%clear (int *buf_len, char **buf );

%apply (int nLen, char *pBuf) { (int buf_len, char *buf_string) };
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                      int buf_len, char *buf_string, int buf_xsize, int buf_ysize, GDALDataType buf_type ) {
    return WriteRaster_internal( self, xoff, yoff, xsize, ysize,
                                 buf_string, buf_xsize, buf_ysize, buf_type );
  }

  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                      int buf_len, char *buf_string, int buf_xsize, int buf_ysize ) {
    return WriteRaster_internal( self, xoff, yoff, xsize, ysize,
                                 buf_string, buf_xsize, buf_ysize, GDALGetRasterDataType(self) );
  }

  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                      int buf_len, char *buf_string ) {
    return WriteRaster_internal( self, xoff, yoff, xsize, ysize,
                                 buf_string, xsize, ysize, GDALGetRasterDataType(self) );
  }
%clear (int buf_len, char *buf_string);
}

  void FlushCache() {
    GDALFlushRasterCache( self );
  }

/* NEEDED */
/* ReadAsArray */
/* WriteArray */
/* GetRasterColorInterpretation */
/* GetRasterColorTable */
/* SetRasterColorTable */
/* GetHistogram */
/* ComputeBandStats */
/* AdviseRead */

};

%{
GDALDataType GDALRasterBand_DataType_get( GDALRasterBand *h ) {
  return GDALGetRasterDataType( h );
}
int GDALRasterBand_XSize_get( GDALRasterBand *h ) {
  return GDALGetRasterBandXSize( h );
}
int GDALRasterBand_YSize_get( GDALRasterBand *h ) {
  return GDALGetRasterBandYSize( h );
}
%}
