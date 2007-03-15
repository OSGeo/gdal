/******************************************************************************
 * $Id$
 *
 * Name:     Band.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

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

%apply (double *OUTPUT){double *min, double *max, double *mean, double *stddev};
%apply (IF_ERROR_RETURN_NONE) { (CPLErr) };
  CPLErr GetStatistics( int approx_ok, int force, 
                      double *min, double *max, double *mean, double *stddev ){
    return GDALGetRasterStatistics( self, approx_ok, force, 
				    min, max, mean, stddev );
  }
%clear (CPLErr);

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

  void ComputeBandStats( double argout[2], int samplestep = 1) {
    GDALComputeBandStats( self, samplestep, argout+0, argout+1, 
                          NULL, NULL );
  }

  CPLErr Fill( double real_fill, double imag_fill =0.0 ) {
    return GDALFillRaster( self, real_fill, imag_fill );
  }

#ifndef SWIGCSHARP
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
#endif /* SWIGCSHARP */

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
