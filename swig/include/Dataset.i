/******************************************************************************
 * $Id$
 *
 * Name:     Dataset.i
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

%{
/* Returned size is in bytes or 0 if an error occured */
static
int ComputeDatasetRasterIOSize (int buf_xsize, int buf_ysize, int nPixelSize,
                                int nBands, int* bandMap, int nBandMapArrayLength,
                                int nPixelSpace, int nLineSpace, int nBandSpace,
                                int bSpacingShouldBeMultipleOfPixelSize )
{
    const int MAX_INT = 0x7fffffff;
    if (buf_xsize <= 0 || buf_ysize <= 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Illegal values for buffer size");
        return 0;
    }

    if (nPixelSpace < 0 || nLineSpace < 0 || nBandSpace < 0)
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

    if( nBandSpace == 0 )
    {
        if (nLineSpace > MAX_INT / buf_ysize)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
            return 0;
        }
        nBandSpace = nLineSpace * buf_ysize;
    }
    else if ( bSpacingShouldBeMultipleOfPixelSize && (nBandSpace % nPixelSize) != 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "nLineSpace should be a multiple of nPixelSize");
        return 0;
    }

    if (nBands <= 0 || (bandMap != NULL && nBands > nBandMapArrayLength))
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Invalid band count");
        return 0;
    }

    if ((buf_ysize - 1) > MAX_INT / nLineSpace ||
        (buf_xsize - 1) > MAX_INT / nPixelSpace ||
        (nBands - 1) > MAX_INT / nBandSpace ||
        (buf_ysize - 1) * nLineSpace > MAX_INT - (buf_xsize - 1) * nPixelSpace ||
        (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace > MAX_INT - (nBands - 1) * nBandSpace ||
        (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace + (nBands - 1) * nBandSpace > MAX_INT - nPixelSize)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
        return 0;
    }

    return (buf_ysize - 1) * nLineSpace + (buf_xsize - 1) * nPixelSpace + (nBands - 1) * nBandSpace + nPixelSize;
}
%}

#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
%{
static
CPLErr DSReadRaster_internal( GDALDatasetShadow *obj, 
                            int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            int *buf_size, char **buf,
                            int band_list, int *pband_list,
                            int pixel_space, int line_space, int band_space)
{
  CPLErr result;
  
  *buf_size = ComputeDatasetRasterIOSize (buf_xsize, buf_ysize, GDALGetDataTypeSize( buf_type ) / 8,
                                          band_list ? band_list : GDALGetRasterCount(obj), pband_list, band_list,
                                          pixel_space, line_space, band_space, FALSE);
  if (*buf_size == 0)
  {
      *buf = 0;
      return CE_Failure;
  }
  
  *buf = (char*) malloc( *buf_size );
  if (*buf)
  {
    result = GDALDatasetRasterIO(obj, GF_Read, xoff, yoff, xsize, ysize,
                                    (void*) *buf, buf_xsize, buf_ysize, buf_type,
                                    band_list, pband_list, pixel_space, line_space, band_space );
    if ( result != CE_None ) {
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
%}

#endif

//************************************************************************
//
// Define the extensions for GDALAsyncReader (nee GDALAsyncReaderShadow)
//
//************************************************************************
%rename (AsyncReader) GDALAsyncReaderShadow;

class GDALAsyncReaderShadow {
private:
  GDALAsyncReaderShadow();
  ~GDALAsyncReaderShadow(){}
public:
%extend {

    %apply (int *OUTPUT){int *xoff, int *yoff, int *buf_xsize, int *buf_ysize}
    GDALAsyncStatusType GetNextUpdatedRegion(double timeout, int* xoff, int* yoff, int* buf_xsize, int* buf_ysize )
    {
        return GDALARGetNextUpdatedRegion(self, timeout, xoff, yoff, buf_xsize, buf_ysize );
    }
    %clear (int *xoff, int *yoff, int *buf_xsize, int *buf_ysize);
    
    int LockBuffer( double timeout )
    {
        return GDALARLockBuffer(self,timeout);
    }
    
    void UnlockBuffer()
    {
        GDALARUnlockBuffer(self);
    }

    } /* extend */
}; /* GDALAsyncReaderShadow */ 

//************************************************************************
//
// Define the extensions for Dataset (nee GDALDatasetShadow)
//
//************************************************************************

%rename (Dataset) GDALDatasetShadow;

class GDALDatasetShadow : public GDALMajorObjectShadow {
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

  void GetGeoTransform( double argout[6] ) {
    if ( GDALGetGeoTransform( self, argout ) != 0 ) {
      argout[0] = 0.0;
      argout[1] = 1.0;
      argout[2] = 0.0;
      argout[3] = 0.0;
      argout[4] = 0.0;
      argout[5] = 1.0;
    }
  }

  CPLErr SetGeoTransform( double argin[6] ) {
    return GDALSetGeoTransform( self, argin );
  }

  // The (int,int*) arguments are typemapped.  The name of the first argument
  // becomes the kwarg name for it.
#ifndef SWIGCSHARP  
#ifndef SWIGJAVA
%feature("kwargs") BuildOverviews;
#endif
%apply (int nList, int* pList) { (int overviewlist, int *pOverviews) };
#else
%apply (void *buffer_ptr) {int *pOverviews};
#endif
#ifdef SWIGJAVA
%apply (const char* stringWithDefaultValue) {const char* resampling};
  int BuildOverviews( const char *resampling,
                      int overviewlist, int *pOverviews,
                      GDALProgressFunc callback = NULL,
                      void* callback_data=NULL ) {
#else
  int BuildOverviews( const char *resampling = "NEAREST",
                      int overviewlist = 0 , int *pOverviews = 0,
                      GDALProgressFunc callback = NULL,
                      void* callback_data=NULL ) {
#endif
    return GDALBuildOverviews(  self, 
                                resampling ? resampling : "NEAREST", 
                                overviewlist, 
                                pOverviews, 
                                0, 
                                0, 
                                callback, 
                                callback_data);
  }
#ifndef SWIGCSHARP
%clear (int overviewlist, int *pOverviews);
#else
%clear (int *pOverviews);
#endif
#ifdef SWIGJAVA
%clear (const char *resampling);
#endif

  int GetGCPCount() {
    return GDALGetGCPCount( self );
  }

  const char *GetGCPProjection() {
    return GDALGetGCPProjection( self );
  }
  
#ifndef SWIGCSHARP
  void GetGCPs( int *nGCPs, GDAL_GCP const **pGCPs ) {
    *nGCPs = GDALGetGCPCount( self );
    *pGCPs = GDALGetGCPs( self );
  }

  CPLErr SetGCPs( int nGCPs, GDAL_GCP const *pGCPs, const char *pszGCPProjection ) {
    return GDALSetGCPs( self, nGCPs, pGCPs, pszGCPProjection );
  }

#endif

  void FlushCache() {
    GDALFlushCache( self );
  }

#ifndef SWIGJAVA
%feature ("kwargs") AddBand;
#endif
/* uses the defined char **options typemap */
  CPLErr AddBand( GDALDataType datatype = GDT_Byte, char **options = 0 ) {
    return GDALAddBand( self, datatype, options );
  }

  CPLErr CreateMaskBand( int nFlags ) {
      return GDALCreateDatasetMaskBand( self, nFlags );
  }

#if defined(SWIGPYTHON) || defined (SWIGJAVA)
%apply (char **out_ppsz_and_free) {char **};
#else
/*  this is a required typemap (hi, Python and Java guys!) returned list is copied and CSLDestroy'ed */
%apply (char **CSL) {char **};
#endif
  char **GetFileList() {
    return GDALGetFileList( self );
  }
%clear char **;

#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
%feature("kwargs") WriteRaster;
%apply (int nLen, char *pBuf) { (int buf_len, char *buf_string) };
%apply (int *optional_int) { (int*) };
%apply (int *optional_int) { (GDALDataType *buf_type) };
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
	              int buf_len, char *buf_string,
                      int *buf_xsize = 0, int *buf_ysize = 0,
                      GDALDataType *buf_type = 0,
                      int band_list = 0, int *pband_list = 0,
                      int* buf_pixel_space = 0, int* buf_line_space = 0, int* buf_band_space = 0) {
    CPLErr eErr;
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype;
    if ( buf_type != 0 ) {
      ntype = (GDALDataType) *buf_type;
    } else {
      int lastband = GDALGetRasterCount( self ) - 1;
      if (lastband < 0)
        return CE_Failure;
      ntype = GDALGetRasterDataType( GDALGetRasterBand( self, lastband ) );
    }

    int pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    int line_space = (buf_line_space == 0) ? 0 : *buf_line_space;
    int band_space = (buf_band_space == 0) ? 0 : *buf_band_space;

    int min_buffer_size =
      ComputeDatasetRasterIOSize (nxsize, nysize, GDALGetDataTypeSize( ntype ) / 8,
                                  band_list ? band_list : GDALGetRasterCount(self), pband_list, band_list,
                                  pixel_space, line_space, band_space, FALSE);
    if (min_buffer_size == 0)
        return CE_Failure;

    if ( buf_len < min_buffer_size )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
        return CE_Failure;
    }
  
    eErr = GDALDatasetRasterIO( self, GF_Write, xoff, yoff, xsize, ysize,
                                (void*) buf_string, nxsize, nysize, ntype,
                                band_list, pband_list, pixel_space, line_space, band_space );

    return eErr;
  }
%clear (int band_list, int *pband_list );
%clear (GDALDataType *buf_type);
%clear (int*);
%clear (int buf_len, char *buf_string);
#endif

#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
%feature("kwargs") ReadRaster;
%apply (int *optional_int) { (GDALDataType *buf_type) };
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
%apply ( int *nLen, char **pBuf ) { (int *buf_len, char **buf ) };
%apply ( int *optional_int ) {(int*)};                            
CPLErr ReadRaster(  int xoff, int yoff, int xsize, int ysize,
	              int *buf_len, char **buf,
                      int *buf_xsize = 0, int *buf_ysize = 0,
                      GDALDataType *buf_type = 0,
                      int band_list = 0, int *pband_list = 0,
                      int* buf_pixel_space = 0, int* buf_line_space = 0, int* buf_band_space = 0 )
{
    CPLErr eErr;
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype;
    if ( buf_type != 0 ) {
      ntype = (GDALDataType) *buf_type;
    } else {
      int lastband = GDALGetRasterCount( self ) - 1;
      if (lastband < 0)
        return CE_Failure;
      ntype = GDALGetRasterDataType( GDALGetRasterBand( self, lastband ) );
    }

    int pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    int line_space = (buf_line_space == 0) ? 0 : *buf_line_space;
    int band_space = (buf_band_space == 0) ? 0 : *buf_band_space;
                            
    eErr = DSReadRaster_internal( self, xoff, yoff, xsize, ysize,
                                nxsize, nysize, ntype,
                                buf_len, buf, 
                                band_list, pband_list,
                                pixel_space, line_space, band_space);

    return eErr;
}
  
%clear (int *buf_len, char **buf );
%clear (int*);
#endif


/* NEEDED */
/* GetSubDatasets */
/* ReadAsArray */
/* AddBand */
/* AdviseRead */
/* ReadRaster */
  
#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
%feature("kwargs") BeginAsyncReader;
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
%apply (int *nLength, char **pBuffer ) { (int *buf_len, char **buf ) };
%apply(int *optional_int) { (int*) };  
  GDALAsyncReaderShadow* BeginAsyncReader( \
       int xOff, int yOff, int xSize, int ySize, int *buf_len, char **buf, \
       int buf_xsize, int buf_ysize, GDALDataType bufType = (GDALDataType)0, \
       int band_list = 0, int *pband_list = 0, int nPixelSpace = 0, \
       int nLineSpace = 0, int nBandSpace = 0, char **options = 0)  {
    
    if ((options != NULL) && (buf_xsize ==0) && (buf_ysize == 0))
    {
        // calculate an appropriate buffer size
        const char* pszLevel = CSLFetchNameValue(options, "LEVEL");
        if (pszLevel)
        {
            // round up
            int nLevel = atoi(pszLevel);
            int nRes = 2 << (nLevel - 1);
            buf_xsize = ceil(xSize / (1.0 * nRes));
            buf_ysize = ceil(ySize / (1.0 * nRes));
        }
    }
    
    int nxsize = (buf_xsize == 0) ? xSize : buf_xsize;
    int nysize = (buf_ysize == 0) ? ySize : buf_ysize;
    
    GDALDataType ntype;
    if (bufType != 0) {
        ntype = (GDALDataType) bufType;
    } 
    else {
        ntype = GDT_Byte;
    }
    
    bool myBandList = false;
    int nBCount;
    int* pBandList;
    
    if (band_list != 0){
        myBandList = false;
        nBCount = band_list;
        pBandList = pband_list;
    }        
    else
    {
        myBandList = true;
        nBCount = GDALGetRasterCount(self);
        pBandList = (int*)CPLMalloc(sizeof(int) * nBCount);
        for (int i = 0; i < nBCount; ++i) {
            pBandList[i] = i;
        }
    }
    
    // for python bindings create buffer 
    if (*buf == NULL)
    {
        // calculate buffer size
        // if type is byte typeSize is GDT_Int32 (4) since these are packed into an int (BGRA)
        if (ntype == GDT_Byte)
            *buf = (char*) VSIMalloc( nxsize * nysize * (int)GDALGetDataTypeSize(GDT_Int32) );
        else
            *buf = (char*) VSIMalloc( nxsize * nysize *  (int)GDALGetDataTypeSize(ntype)); 
    }
        
    // check we were able to create the buffer
    if (*buf)
    {
        return (GDALAsyncReader*) GDALBeginAsyncReader(self, xOff, yOff, xSize, ySize, (void*) *buf, nxsize, nysize, ntype, nBCount, pBandList, nPixelSpace, nLineSpace,
        nBandSpace, options);
    }
    else
    {
        *buf = 0;
        return NULL;
    }
    
    if ( myBandList ) {
       CPLFree( pBandList );
    }

  }

  %clear(int nBands, int *pband_list);
  %clear(int *buf_len, char **buf);
  %clear(int*);
#endif  

  void EndAsyncReader(GDALAsyncReaderShadow* ario){
    GDALEndAsyncReader(self, (GDALAsyncReaderH) ario);
  }

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

