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
/* Returned size is in bytes or 0 if an error occured */
static
int ComputeBandRasterIOSize (int buf_xsize, int buf_ysize, int nPixelSize,
                             int nPixelSpace, int nLineSpace,
                             int bSpacingShouldBeMultipleOfPixelSize )
{
    const int MAX_INT = 0x7fffffff;
    if (buf_xsize <= 0 || buf_ysize <= 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Illegal values for buffer size");
        return 0;
    }

    if (nPixelSpace < 0 || nLineSpace < 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Illegal values for space arguments");
        return 0;
    }

    if (nPixelSize == 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Illegal value for data type");
        return 0;
    }

    if( nPixelSpace == 0 )
        nPixelSpace = nPixelSize;
    else if ( bSpacingShouldBeMultipleOfPixelSize && (nPixelSpace % nPixelSize) != 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "nPixelSpace should be a multiple of nPixelSize");
        return 0;
    }

    if( nLineSpace == 0 )
    {
        if (nPixelSpace > MAX_INT / buf_xsize)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
            return 0;
        }
        nLineSpace = nPixelSpace * buf_xsize;
    }
    else if ( bSpacingShouldBeMultipleOfPixelSize && (nLineSpace % nPixelSize) != 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "nLineSpace should be a multiple of nPixelSize");
        return 0;
    }

    if ((buf_ysize - 1) > MAX_INT / nLineSpace ||
        (buf_xsize - 1) > MAX_INT / nPixelSpace ||
        (buf_ysize - 1) * nLineSpace > MAX_INT - (buf_xsize - 1) * nPixelSpace ||
        (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace > MAX_INT - nPixelSize)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
        return 0;
    }

    return (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace + nPixelSize;
}
%}

#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
%{
static
CPLErr ReadRaster_internal( GDALRasterBandShadow *obj, 
                            int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            int *buf_size, char **buf,
                            int pixel_space, int line_space )
{
  CPLErr result;
  
  *buf_size = ComputeBandRasterIOSize( buf_xsize, buf_ysize, GDALGetDataTypeSize( buf_type ) / 8,
                                       pixel_space, line_space, FALSE );
  
  if ( *buf_size == 0 )
  {
      *buf = 0;
      return CE_Failure;
  }
  
  *buf = (char*) malloc( *buf_size );
  if ( *buf )
  {
    result =  GDALRasterIO( obj, GF_Read, xoff, yoff, xsize, ysize,
                                    (void *) *buf, buf_xsize, buf_ysize,
                                    buf_type, pixel_space, line_space );
    if ( result != CE_None )
    {
        free( *buf );
        *buf = 0;
        *buf_size = 0;
    }
  }
  else
  {
    CPLError(CE_Failure, CPLE_OutOfMemory, "Not enough memory to allocate %d bytes", *buf_size);
    result = CE_Failure;
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
                             int buf_size, char *buffer,
                             int pixel_space, int line_space)
{
    int min_buffer_size = ComputeBandRasterIOSize (buf_xsize, buf_ysize, GDALGetDataTypeSize( buf_type ) / 8,
                                                   pixel_space, line_space, FALSE );
    if ( min_buffer_size == 0 )
      return CE_Failure;
      
    if ( buf_size < min_buffer_size ) {
      CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
      return CE_Failure;
    }

    return GDALRasterIO( obj, GF_Write, xoff, yoff, xsize, ysize, 
		        (void *) buffer, buf_xsize, buf_ysize, buf_type, pixel_space, line_space );
}
%}

#endif

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

  /* Interface method added for GDAL 1.7.0 */
  int GetBand()
  {
    return GDALGetBandNumber(self);
  }

%apply (int *OUTPUT){int *pnBlockXSize, int *pnBlockYSize}

  void GetBlockSize(int *pnBlockXSize, int *pnBlockYSize) {
      GDALGetBlockSize(self, pnBlockXSize, pnBlockYSize);
  }

  // Preferred name to match C++ API
  /* Interface method added for GDAL 1.7.0 */
  GDALColorInterp GetColorInterpretation() {
    return GDALGetRasterColorInterpretation( self );
  }

  // Deprecated name
  GDALColorInterp GetRasterColorInterpretation() {
    return GDALGetRasterColorInterpretation( self );
  }

  // Preferred name to match C++ API
  /* Interface method added for GDAL 1.7.0 */
  CPLErr SetColorInterpretation( GDALColorInterp val ) {
    return GDALSetRasterColorInterpretation( self, val );
  }

  // Deprecated name
  CPLErr SetRasterColorInterpretation( GDALColorInterp val ) {
    return GDALSetRasterColorInterpretation( self, val );
  }

  void GetNoDataValue( double *val, int *hasval ) {
    *val = GDALGetRasterNoDataValue( self, hasval );
  }

  CPLErr SetNoDataValue( double d) {
    return GDALSetRasterNoDataValue( self, d );
  }
  
  /* Interface method added for GDAL 1.7.0 */
  const char* GetUnitType() {
      return GDALGetRasterUnitType( self );
  }
  
  %apply (char **options) { (char **) };
  char** GetRasterCategoryNames( ) {
    return GDALGetRasterCategoryNames( self );
  }
  %clear (char **);
  
  %apply (char **options) { (char **names) };
  CPLErr SetRasterCategoryNames( char **names ) {
    return GDALSetRasterCategoryNames( self, names );
  }
  %clear (char **names);

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

  /* Interface method added for GDAL 1.8.0 */
  CPLErr SetOffset( double val ) {
    return GDALSetRasterOffset( self, val );
  }

  /* Interface method added for GDAL 1.8.0 */
  CPLErr SetScale( double val ) {
    return GDALSetRasterScale( self, val );
  }
  
%apply (double *OUTPUT){double *min, double *max, double *mean, double *stddev};
%apply (IF_ERROR_RETURN_NONE) { (CPLErr) }; 
  CPLErr GetStatistics( int approx_ok, int force, 
                      double *min, double *max, double *mean, double *stddev ){
    return GDALGetRasterStatistics( self, approx_ok, force, 
				    min, max, mean, stddev );
  }
%clear (CPLErr);

  /* Interface method added for GDAL 1.7.0 */
%apply (double *OUTPUT){double *min, double *max, double *mean, double *stddev};
%apply (IF_ERROR_RETURN_NONE) { (CPLErr) }; 
  CPLErr ComputeStatistics( bool approx_ok, double *min = NULL, double *max = NULL, double *mean = NULL, double *stddev = NULL,
                            GDALProgressFunc callback = NULL, void* callback_data=NULL){
    return GDALComputeRasterStatistics( self, approx_ok, min, max, mean, stddev, callback, callback_data );
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

#if defined (SWIGJAVA)
  int Checksum( int xoff, int yoff, int xsize, int ysize) {
    return GDALChecksumImage( self, xoff, yoff, xsize, ysize );
  }
#else
%apply (int *optional_int) {(int*)};
%feature ("kwargs") Checksum;
  int Checksum( int xoff = 0, int yoff = 0, int *xsize = 0, int *ysize = 0) {
    int nxsize = (xsize!=0) ? *xsize : GDALGetRasterBandXSize( self );
    int nysize = (ysize!=0) ? *ysize : GDALGetRasterBandYSize( self );
    return GDALChecksumImage( self, xoff, yoff, nxsize, nysize );
  }
%clear (int*);
#endif

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

#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
%apply ( int *nLen, char **pBuf ) { (int *buf_len, char **buf ) };
%apply ( int *optional_int ) {(int*)};
%feature( "kwargs" ) ReadRaster;
  CPLErr ReadRaster( int xoff, int yoff, int xsize, int ysize,
                     int *buf_len, char **buf,
                     int *buf_xsize = 0,
                     int *buf_ysize = 0,
                     int *buf_type = 0,
                     int *buf_pixel_space = 0,
                     int *buf_line_space = 0) {
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(self)
                                        : (GDALDataType)*buf_type;
    int pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    int line_space = (buf_line_space == 0) ? 0 : *buf_line_space;
    return ReadRaster_internal( self, xoff, yoff, xsize, ysize,
                                nxsize, nysize, ntype, buf_len, buf, pixel_space, line_space );
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
                      int *buf_type = 0,
                      int *buf_pixel_space = 0,
                      int *buf_line_space = 0) {
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(self)
                                        : (GDALDataType)*buf_type;
    int pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    int line_space = (buf_line_space == 0) ? 0 : *buf_line_space;
    return WriteRaster_internal( self, xoff, yoff, xsize, ysize,
                                 nxsize, nysize, ntype, buf_len, buf_string, pixel_space, line_space );
  }
%clear (int buf_len, char *buf_string);
%clear (int*);
#endif /* !defined(SWIGCSHARP) && !defined(SWIGJAVA) */

  void FlushCache() {
    GDALFlushRasterCache( self );
  }

  // Deprecated name
  GDALColorTableShadow *GetRasterColorTable() {
    return (GDALColorTableShadow*) GDALGetRasterColorTable( self );
  }

  // Preferred name to match C++ API
  GDALColorTableShadow *GetColorTable() {
    return (GDALColorTableShadow*) GDALGetRasterColorTable( self );
  }

  // Deprecated name
  int SetRasterColorTable( GDALColorTableShadow *arg ) {
    return GDALSetRasterColorTable( self, arg );
  }
 
  // Preferred name to match C++ API
  int SetColorTable( GDALColorTableShadow *arg ) {
    return GDALSetRasterColorTable( self, arg );
  }
 
  GDALRasterAttributeTableShadow *GetDefaultRAT() { 
      return (GDALRasterAttributeTableShadow*) GDALGetDefaultRAT(self);
  }

  int SetDefaultRAT( GDALRasterAttributeTableShadow *table ) {
      return GDALSetDefaultRAT(self, table);
  }

  GDALRasterBandShadow *GetMaskBand() {
      return (GDALRasterBandShadow *) GDALGetMaskBand( self );
  }

  int GetMaskFlags() {
      return GDALGetMaskFlags( self );
  }

  CPLErr CreateMaskBand( int nFlags ) {
      return GDALCreateMaskBand( self, nFlags );
  }

#ifndef SWIGJAVA
#if defined(SWIGCSHARP)
%apply (int inout[ANY]) {int *panHistogram};
#elif defined(SWIGPERL)
%apply (int len, int *output) {(int buckets, int *panHistogram)};
%apply (IF_ERROR_RETURN_NONE) { (CPLErr) }; 
#endif
%feature( "kwargs" ) GetHistogram;
  CPLErr GetHistogram( double min=-0.5,
                     double max=255.5,
                     int buckets=256,
                     int *panHistogram = NULL,
                     int include_out_of_range = 0,
                     int approx_ok = 1,
                     GDALProgressFunc callback = NULL,
                     void* callback_data=NULL ) {
    CPLErrorReset(); 
    CPLErr err = GDALGetRasterHistogram( self, min, max, buckets, panHistogram,
                                         include_out_of_range, approx_ok,
                                         callback, callback_data );
    return err;
  }
#if defined(SWIGCSHARP)
%clear int *panHistogram;
#elif defined(SWIGPERL)
%clear (int buckets, int *panHistogram);
%clear (CPLErr);
#endif
#endif

#ifndef SWIGJAVA
#if defined(SWIGPERL)
%apply (double *OUTPUT){double *min_ret, double *max_ret}
%apply (int *nLen, const int **pList) {(int *buckets_ret, int **ppanHistogram)};
%apply (IF_ERROR_RETURN_NONE) { (CPLErr) }; 
#endif
%feature ("kwargs") GetDefaultHistogram;
CPLErr GetDefaultHistogram( double *min_ret=NULL, double *max_ret=NULL, int *buckets_ret = NULL, 
                            int **ppanHistogram = NULL, int force = 1, 
			    GDALProgressFunc callback = NULL,
                            void* callback_data=NULL ) {
    return GDALGetDefaultHistogram( self, min_ret, max_ret, buckets_ret,
                                    ppanHistogram, force, 
                                    callback, callback_data );
}
#if defined(SWIGPERL)
%clear (double *min_ret, double *max_ret);
%clear (int *buckets_ret, int **ppanHistogram);
%clear (CPLErr);
#endif
#endif

#if defined(SWIGPERL) || defined(SWIGPYTHON) || defined(SWIGJAVA)
%apply (int nList, int* pList) {(int buckets_in, int *panHistogram_in)}
#endif
CPLErr SetDefaultHistogram( double min, double max, 
       			    int buckets_in, int *panHistogram_in ) {
    return GDALSetDefaultHistogram( self, min, max, 
    	   			    buckets_in, panHistogram_in );
}
#if defined(SWIGPERL) || defined(SWIGPYTHON) || defined(SWIGJAVA)
%clear (int buckets_in, int *panHistogram_in);
#endif

  /* Interface method added for GDAL 1.7.0 */
  bool HasArbitraryOverviews() {
      return (GDALHasArbitraryOverviews( self ) != 0) ? true : false;
  }

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
