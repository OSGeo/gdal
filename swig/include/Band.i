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
char *py_ReadRaster( GDALRasterBand *obj,
                     int xoff, int yoff, int xsize, int ysize,
                     int buf_xsize, int buf_ysize, GDALDataType buf_type )
{

  int result_size = buf_xsize * buf_ysize * GDALGetDataTypeSize( buf_type ) / 8;
  void * result = malloc( result_size );
  if ( GDALRasterIO( obj, GF_Read, xoff, yoff, xsize, ysize,
                result, buf_xsize, buf_ysize, buf_type, 0, 0 ) != CE_None ) {
    free( result );
    result = 0;
  }
  return (char*)result;
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

  double GetNoDataValue() {
    int rcode;
    return GDALGetRasterNoDataValue( self, &rcode );
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

  std::vector<double> ComputeRasterMinMax( int approx_ok = 0 ) {
    double c_minmax[2] = {0.0, 0.0};
    GDALComputeRasterMinMax( self, approx_ok, c_minmax );
    std::vector<double> retval(2);
    retval[0] = c_minmax[0];
    retval[1] = c_minmax[1];
    return retval;
  }

%newobject ReadRaster;
  char *ReadRaster( int xoff, int yoff, int xsize, int ysize,
                    int buf_xsize, int buf_ysize, GDALDataType buf_type ) {
    return py_ReadRaster( self, xoff, yoff, xsize, ysize,
                           buf_xsize, buf_ysize, buf_type );
  }

  char *ReadRaster( int xoff, int yoff, int xsize, int ysize,
                    int buf_xsize, int buf_ysize ) {
    return py_ReadRaster( self, xoff, yoff, xsize, ysize,
                           buf_xsize, buf_ysize, GDALGetRasterDataType(self) );
  }

  char *ReadRaster( int xoff, int yoff, int xsize, int ysize ) {
    return py_ReadRaster( self, xoff, yoff, xsize, ysize,
                           xsize, ysize, GDALGetRasterDataType(self) );
  }

}
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
