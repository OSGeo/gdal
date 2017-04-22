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
/* Returned size is in bytes or 0 if an error occurred. */
static
GIntBig ComputeDatasetRasterIOSize (int buf_xsize, int buf_ysize, int nPixelSize,
                                int nBands, int* bandMap, int nBandMapArrayLength,
                                GIntBig nPixelSpace, GIntBig nLineSpace, GIntBig nBandSpace,
                                int bSpacingShouldBeMultipleOfPixelSize )
{
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
        nLineSpace = nPixelSpace * buf_xsize;
    }
    else if ( bSpacingShouldBeMultipleOfPixelSize && (nLineSpace % nPixelSize) != 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "nLineSpace should be a multiple of nPixelSize");
        return 0;
    }

    if( nBandSpace == 0 )
    {
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

    GIntBig nRet = (GIntBig)(buf_ysize - 1) * nLineSpace + (GIntBig)(buf_xsize - 1) * nPixelSpace + (GIntBig)(nBands - 1) * nBandSpace + nPixelSize;
#if SIZEOF_VOIDP == 4
    if (nRet > INT_MAX)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
        return 0;
    }
#endif

    return nRet;
}
%}

#if defined(SWIGPERL)
%{
static
CPLErr DSReadRaster_internal( GDALDatasetShadow *obj,
                            int xoff, int yoff, int xsize, int ysize,
                            int buf_xsize, int buf_ysize,
                            GDALDataType buf_type,
                            GIntBig *buf_size, char **buf,
                            int band_list, int *pband_list,
                            GIntBig pixel_space, GIntBig line_space, GIntBig band_space,
                            GDALRasterIOExtraArg* psExtraArg)
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
    result = GDALDatasetRasterIOEx(obj, GF_Read, xoff, yoff, xsize, ysize,
                                    (void*) *buf, buf_xsize, buf_ysize, buf_type,
                                    band_list, pband_list, pixel_space, line_space, band_space,
                                    psExtraArg );
    if ( result != CE_None ) {
        free( *buf );
        *buf = 0;
        *buf_size = 0;
    }
  }
  else
  {
    CPLError(CE_Failure, CPLE_OutOfMemory, "Not enough memory to allocate " CPL_FRMT_GIB " bytes", *buf_size);
    result = CE_Failure;
    *buf = 0;
    *buf_size = 0;
  }
  return result;
}
%}

#endif

//************************************************************************/
//
// Define the extensions for GDALAsyncReader (nee GDALAsyncReaderShadow)
//
//************************************************************************/
%rename (AsyncReader) GDALAsyncReaderShadow;


%{
typedef struct
{
    GDALAsyncReaderH  hAsyncReader;
    void             *pyObject;
} GDALAsyncReaderWrapper;

typedef void* GDALAsyncReaderWrapperH;

static GDALAsyncReaderH AsyncReaderWrapperGetReader(GDALAsyncReaderWrapperH hWrapper)
{
    GDALAsyncReaderWrapper* psWrapper = (GDALAsyncReaderWrapper*)hWrapper;
    if (psWrapper->hAsyncReader == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AsyncReader object is defunct");
    }
    return psWrapper->hAsyncReader;
}

#if defined(SWIGPYTHON)
static void* AsyncReaderWrapperGetPyObject(GDALAsyncReaderWrapperH hWrapper)
{
    GDALAsyncReaderWrapper* psWrapper = (GDALAsyncReaderWrapper*)hWrapper;
    return psWrapper->pyObject;
}
#endif

static void DeleteAsyncReaderWrapper(GDALAsyncReaderWrapperH hWrapper)
{
    GDALAsyncReaderWrapper* psWrapper = (GDALAsyncReaderWrapper*)hWrapper;
    if (psWrapper->hAsyncReader != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Native AsyncReader object will leak. EndAsyncReader() should have been called before");
    }
    CPLFree(psWrapper);
}

%}

#if defined(SWIGPYTHON)

%nothread;

%{

static GDALAsyncReaderWrapper* CreateAsyncReaderWrapper(GDALAsyncReaderH  hAsyncReader,
                                                        void             *pyObject)
{
    GDALAsyncReaderWrapper* psWrapper = (GDALAsyncReaderWrapper* )CPLMalloc(sizeof(GDALAsyncReaderWrapper));
    psWrapper->hAsyncReader = hAsyncReader;
    psWrapper->pyObject = pyObject;
    Py_INCREF((PyObject*) psWrapper->pyObject);
    return psWrapper;
}

static void DisableAsyncReaderWrapper(GDALAsyncReaderWrapperH hWrapper)
{
    GDALAsyncReaderWrapper* psWrapper = (GDALAsyncReaderWrapper*)hWrapper;
    if (psWrapper->pyObject)
    {
        Py_XDECREF((PyObject*) psWrapper->pyObject);
    }
    psWrapper->pyObject = NULL;
    psWrapper->hAsyncReader = NULL;
}
%}

%thread;

#endif

class GDALAsyncReaderShadow {
private:
  GDALAsyncReaderShadow();
public:
%extend {
    ~GDALAsyncReaderShadow()
    {
        DeleteAsyncReaderWrapper(self);
    }

    %apply (int *OUTPUT) {(int *)};
    GDALAsyncStatusType GetNextUpdatedRegion(double timeout, int* xoff, int* yoff, int* buf_xsize, int* buf_ysize )
    {
        GDALAsyncReaderH hReader = AsyncReaderWrapperGetReader(self);
        if (hReader == NULL)
        {
            *xoff = 0;
            *yoff = 0;
            *buf_xsize = 0;
            *buf_ysize = 0;
            return GARIO_ERROR;
        }
        return GDALARGetNextUpdatedRegion(hReader, timeout, xoff, yoff, buf_xsize, buf_ysize );
    }
    %clear (int *);

#if defined(SWIGPYTHON)
    %apply ( void **outPythonObject ) { (void** ppRetPyObject ) };
    void GetBuffer(void** ppRetPyObject)
    {
        GDALAsyncReaderH hReader = AsyncReaderWrapperGetReader(self);
        if (hReader == NULL)
        {
            *ppRetPyObject = NULL;
            return;
        }
        *ppRetPyObject = AsyncReaderWrapperGetPyObject(self);
        Py_INCREF((PyObject*)*ppRetPyObject);
    }
    %clear (void** ppRetPyObject );
#endif

    int LockBuffer( double timeout )
    {
        GDALAsyncReaderH hReader = AsyncReaderWrapperGetReader(self);
        if (hReader == NULL)
        {
            return 0;
        }
        return GDALARLockBuffer(hReader,timeout);
    }

    void UnlockBuffer()
    {
        GDALAsyncReaderH hReader = AsyncReaderWrapperGetReader(self);
        if (hReader == NULL)
        {
            return;
        }
        GDALARUnlockBuffer(hReader);
    }

    } /* extend */
}; /* GDALAsyncReaderShadow */

//************************************************************************/
//
// Define the extensions for Dataset (nee GDALDatasetShadow)
//
//************************************************************************/

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

  %apply Pointer NONNULL {char const *prj};
  CPLErr SetProjection( char const *prj ) {
    return GDALSetProjection( self, prj );
  }
  %clear char const *prj;

#ifdef SWIGPYTHON
%feature("kwargs") GetGeoTransform;
%apply (int *optional_int) { (int*) };
  void GetGeoTransform( double argout[6], int* isvalid, int* can_return_null = 0 ) {
    if (can_return_null && *can_return_null)
    {
        *isvalid = (GDALGetGeoTransform( self, argout ) == CE_None );
    }
    else
    {
        *isvalid = TRUE;
        if ( GDALGetGeoTransform( self, argout ) != CE_None ) {
            argout[0] = 0.0;
            argout[1] = 1.0;
            argout[2] = 0.0;
            argout[3] = 0.0;
            argout[4] = 0.0;
            argout[5] = 1.0;
        }
    }
  }
%clear (int*);
#else
  void GetGeoTransform( double argout[6] ) {
    if ( GDALGetGeoTransform( self, argout ) != CE_None ) {
      argout[0] = 0.0;
      argout[1] = 1.0;
      argout[2] = 0.0;
      argout[3] = 0.0;
      argout[4] = 0.0;
      argout[5] = 1.0;
    }
  }
#endif

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

%apply (char **CSL) {char **};
  char **GetFileList() {
    return GDALGetFileList( self );
  }
%clear char **;

#if defined(SWIGPYTHON) || defined(SWIGPERL)
%feature("kwargs") WriteRaster;
%apply (GIntBig nLen, char *pBuf) { (GIntBig buf_len, char *buf_string) };
%apply (GIntBig *optional_GIntBig) { (GIntBig*) };
%apply (int *optional_int) { (int*) };
%apply (int *optional_int) { (GDALDataType *buf_type) };
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
  CPLErr WriteRaster( int xoff, int yoff, int xsize, int ysize,
                      GIntBig buf_len, char *buf_string,
                      int *buf_xsize = 0, int *buf_ysize = 0,
                      GDALDataType *buf_type = 0,
                      int band_list = 0, int *pband_list = 0,
                      GIntBig* buf_pixel_space = 0, GIntBig* buf_line_space = 0, GIntBig* buf_band_space = 0) {
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

    GIntBig pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    GIntBig line_space = (buf_line_space == 0) ? 0 : *buf_line_space;
    GIntBig band_space = (buf_band_space == 0) ? 0 : *buf_band_space;

    GIntBig min_buffer_size =
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

    GDALRasterIOExtraArg* psExtraArg = NULL;

    eErr = GDALDatasetRasterIOEx( self, GF_Write, xoff, yoff, xsize, ysize,
                                  (void*) buf_string, nxsize, nysize, ntype,
                                  band_list, pband_list, pixel_space, line_space, band_space, psExtraArg );

    return eErr;
  }
%clear (int band_list, int *pband_list );
%clear (GDALDataType *buf_type);
%clear (int*);
%clear (GIntBig*);
%clear (GIntBig buf_len, char *buf_string);
#endif

#if defined(SWIGPERL)
%feature("kwargs") ReadRaster;
%apply (int *optional_int) { (GDALDataType *buf_type) };
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
%apply (GIntBig *nLen, char **pBuf) { (GIntBig *buf_len, char **buf) };
%apply ( int *optional_int ) {(int*)};
%apply ( GIntBig *optional_GIntBig ) {(GIntBig*)};
CPLErr ReadRaster(  int xoff, int yoff, int xsize, int ysize,
	              GIntBig *buf_len, char **buf,
                      int *buf_xsize = 0, int *buf_ysize = 0,
                      GDALDataType *buf_type = 0,
                      int band_list = 0, int *pband_list = 0,
                      GIntBig* buf_pixel_space = 0, GIntBig* buf_line_space = 0, GIntBig* buf_band_space = 0,
                      GDALRIOResampleAlg resample_alg = GRIORA_NearestNeighbour,
                      GDALProgressFunc callback = NULL,
                      void* callback_data=NULL )
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

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.eResampleAlg = resample_alg;
    sExtraArg.pfnProgress = callback;
    sExtraArg.pProgressData = callback_data;

    GIntBig pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    GIntBig line_space = (buf_line_space == 0) ? 0 : *buf_line_space;
    GIntBig band_space = (buf_band_space == 0) ? 0 : *buf_band_space;

    eErr = DSReadRaster_internal( self, xoff, yoff, xsize, ysize,
                                nxsize, nysize, ntype,
                                buf_len, buf,
                                band_list, pband_list,
                                pixel_space, line_space, band_space, &sExtraArg);

    return eErr;
}

%clear (GDALDataType *buf_type);
%clear (int band_list, int *pband_list );
%clear (int *buf_len, char **buf );
%clear (int*);
%clear (GIntBig*);
#endif


/* NEEDED */
/* GetSubDatasets */
/* ReadAsArray */
/* AddBand */
/* AdviseRead */
/* ReadRaster */

#if defined(SWIGPYTHON)
%feature("kwargs") BeginAsyncReader;
%newobject BeginAsyncReader;
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
%apply (int nLenKeepObject, char *pBufKeepObject, void* pyObject) { (int buf_len, char *buf_string, void* pyObject) };
%apply (int *optional_int) { (int*) };
  GDALAsyncReaderShadow* BeginAsyncReader(
       int xOff, int yOff, int xSize, int ySize,
       int buf_len, char *buf_string, void* pyObject,
       int buf_xsize, int buf_ysize, GDALDataType bufType = (GDALDataType)0,
       int band_list = 0, int *pband_list = 0, int nPixelSpace = 0,
       int nLineSpace = 0, int nBandSpace = 0, char **options = 0)  {

    if ((options != NULL) && (buf_xsize ==0) && (buf_ysize == 0))
    {
        // calculate an appropriate buffer size
        const char* pszLevel = CSLFetchNameValue(options, "LEVEL");
        if (pszLevel)
        {
            // round up
            int nLevel = atoi(pszLevel);
            if( nLevel < 0 || nLevel > 30 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid LEVEL: %d", nLevel);
            }
            else
            {
                int nRes = 1 << nLevel;
                buf_xsize = static_cast<int>(ceil(xSize / (1.0 * nRes)));
                buf_ysize = static_cast<int>(ceil(ySize / (1.0 * nRes)));
            }
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

    int nBCount = (band_list) != 0 ? band_list : GDALGetRasterCount(self);
    int nMinSize = nxsize * nysize * nBCount * (GDALGetDataTypeSize(ntype) / 8);
    if (buf_string == NULL || buf_len < nMinSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Buffer is too small");
        return NULL;
    }

    bool myBandList = false;
    int* pBandList;

    if (band_list != 0){
        myBandList = false;
        pBandList = pband_list;
    }
    else
    {
        myBandList = true;
        pBandList = (int*)CPLMalloc(sizeof(int) * nBCount);
        for (int i = 0; i < nBCount; ++i) {
            pBandList[i] = i + 1;
        }
    }

    GDALAsyncReaderH hAsyncReader =
            GDALBeginAsyncReader(self, xOff, yOff, xSize, ySize, (void*) buf_string, nxsize, nysize, ntype, nBCount, pBandList, nPixelSpace, nLineSpace,
    nBandSpace, options);

    if ( myBandList ) {
       CPLFree( pBandList );
    }

    if (hAsyncReader)
    {
        return (GDALAsyncReader*) CreateAsyncReaderWrapper(hAsyncReader, pyObject);
    }
    else
    {
        return NULL;
    }

  }

%clear(int band_list, int *pband_list);
%clear (int buf_len, char *buf_string, void* pyObject);
%clear(int*);

  void EndAsyncReader(GDALAsyncReaderShadow* ario){
    if( ario == NULL ) return;
    GDALAsyncReaderH hReader = AsyncReaderWrapperGetReader(ario);
    if (hReader == NULL)
    {
        return;
    }
    GDALEndAsyncReader(self, hReader);
    DisableAsyncReaderWrapper(ario);
  }

%feature( "kwargs" ) GetVirtualMem;
%newobject GetVirtualMem;
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
  CPLVirtualMemShadow* GetVirtualMem( GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      int band_list, int *pband_list,
                                      int bIsBandSequential,
                                      size_t nCacheSize,
                                      size_t nPageSizeHint,
                                      char** options = NULL )
    {
        int nPixelSpace;
        int nBandSpace;
        if( bIsBandSequential != 0 && bIsBandSequential != 1 )
            return NULL;
        if( band_list == 0 )
            return NULL;
        if( bIsBandSequential || band_list == 1 )
        {
            nPixelSpace = 0;
            nBandSpace = 0;
        }
        else
        {
            nBandSpace = GDALGetDataTypeSize(eBufType) / 8;
            nPixelSpace = nBandSpace * band_list;
        }
        CPLVirtualMem* vmem = GDALDatasetGetVirtualMem( self,
                                         eRWFlag,
                                         nXOff, nYOff,
                                         nXSize, nYSize,
                                         nBufXSize, nBufYSize,
                                         eBufType,
                                         band_list, pband_list,
                                         nPixelSpace,
                                         0,
                                         nBandSpace,
                                         nCacheSize,
                                         nPageSizeHint,
                                         FALSE,
                                         options );
        if( vmem == NULL )
            return NULL;
        CPLVirtualMemShadow* vmemshadow = (CPLVirtualMemShadow*)calloc(1, sizeof(CPLVirtualMemShadow));
        vmemshadow->vmem = vmem;
        vmemshadow->eBufType = eBufType;
        vmemshadow->bIsBandSequential = bIsBandSequential;
        vmemshadow->bReadOnly = (eRWFlag == GF_Read);
        vmemshadow->nBufXSize = nBufXSize;
        vmemshadow->nBufYSize = nBufYSize;
        vmemshadow->nBandCount = band_list;
        return vmemshadow;
    }
%clear(int band_list, int *pband_list);

%feature( "kwargs" ) GetTiledVirtualMem;
%newobject GetTiledVirtualMem;
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
  CPLVirtualMemShadow* GetTiledVirtualMem( GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff,
                                      int nXSize, int nYSize,
                                      int nTileXSize, int nTileYSize,
                                      GDALDataType eBufType,
                                      int band_list, int *pband_list,
                                      GDALTileOrganization eTileOrganization,
                                      size_t nCacheSize,
                                      char** options = NULL )
    {
        if( band_list == 0 )
            return NULL;
        CPLVirtualMem* vmem = GDALDatasetGetTiledVirtualMem( self,
                                         eRWFlag,
                                         nXOff, nYOff,
                                         nXSize, nYSize,
                                         nTileXSize, nTileYSize,
                                         eBufType,
                                         band_list, pband_list,
                                         eTileOrganization,
                                         nCacheSize,
                                         FALSE,
                                         options );
        if( vmem == NULL )
            return NULL;
        CPLVirtualMemShadow* vmemshadow = (CPLVirtualMemShadow*)calloc(1, sizeof(CPLVirtualMemShadow));
        vmemshadow->vmem = vmem;
        vmemshadow->eBufType = eBufType;
        vmemshadow->bIsBandSequential = -1;
        vmemshadow->bReadOnly = (eRWFlag == GF_Read);
        vmemshadow->nBufXSize = nXSize;
        vmemshadow->nBufYSize = nYSize;
        vmemshadow->eTileOrganization = eTileOrganization;
        vmemshadow->nTileXSize = nTileXSize;
        vmemshadow->nTileYSize = nTileYSize;
        vmemshadow->nBandCount = band_list;
        return vmemshadow;
    }
%clear(int band_list, int *pband_list);

#endif /* PYTHON */

#if defined(SWIGPYTHON) || defined(SWIGJAVA) || defined(SWIGPERL)

  /* Note that datasources own their layers */
#ifndef SWIGJAVA
  %feature( "kwargs" ) CreateLayer;
#endif
  OGRLayerShadow *CreateLayer(const char* name,
              OSRSpatialReferenceShadow* srs=NULL,
              OGRwkbGeometryType geom_type=wkbUnknown,
              char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetCreateLayer( self,
                                  name,
                                  srs,
                                  geom_type,
                                  options);
    return layer;
  }

#ifndef SWIGJAVA
  %feature( "kwargs" ) CopyLayer;
#endif
%apply Pointer NONNULL {OGRLayerShadow *src_layer};
  OGRLayerShadow *CopyLayer(OGRLayerShadow *src_layer,
            const char* new_name,
            char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetCopyLayer( self,
                                                      src_layer,
                                                      new_name,
                                                      options);
    return layer;
  }

  OGRErr DeleteLayer(int index){
    return GDALDatasetDeleteLayer(self, index);
  }

  int GetLayerCount() {
    return GDALDatasetGetLayerCount(self);
  }

#ifdef SWIGJAVA
  OGRLayerShadow *GetLayerByIndex( int index ) {
#else
  OGRLayerShadow *GetLayerByIndex( int index=0) {
#endif
    OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetGetLayer(self, index);
    return layer;
  }

  OGRLayerShadow *GetLayerByName( const char* layer_name) {
    OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetGetLayerByName(self, layer_name);
    return layer;
  }

  void ResetReading()
  {
    GDALDatasetResetReading( self );
  }

#ifdef SWIGPYTHON
%newobject GetNextFeature;
%feature( "kwargs" ) GetNextFeature;
  OGRFeatureShadow* GetNextFeature( bool include_layer = true,
                                    bool include_pct = false,
                                    OGRLayerShadow** ppoBelongingLayer = NULL,
                                    double* pdfProgressPct = NULL,
                                    GDALProgressFunc callback = NULL,
                                    void* callback_data=NULL )
  {
    return GDALDatasetGetNextFeature( self, ppoBelongingLayer, pdfProgressPct,
                                      callback, callback_data );
  }
#else
    // FIXME: return layer
%newobject GetNextFeature;
  OGRFeatureShadow* GetNextFeature()
  {
    return GDALDatasetGetNextFeature( self, NULL, NULL, NULL, NULL );
  }
#endif

  bool TestCapability(const char * cap) {
    return (GDALDatasetTestCapability(self, cap) > 0);
  }

#ifndef SWIGJAVA
  %feature( "kwargs" ) ExecuteSQL;
#endif
  %apply Pointer NONNULL {const char * statement};
  OGRLayerShadow *ExecuteSQL(const char* statement,
                        OGRGeometryShadow* spatialFilter=NULL,
                        const char* dialect="") {
    OGRLayerShadow* layer = (OGRLayerShadow*) GDALDatasetExecuteSQL(self,
                                                      statement,
                                                      spatialFilter,
                                                      dialect);
    return layer;
  }

%apply SWIGTYPE *DISOWN {OGRLayerShadow *layer};
  void ReleaseResultSet(OGRLayerShadow *layer){
    GDALDatasetReleaseResultSet(self, layer);
  }
%clear OGRLayerShadow *layer;

  OGRStyleTableShadow *GetStyleTable() {
    return (OGRStyleTableShadow*) GDALDatasetGetStyleTable(self);
  }

  void SetStyleTable(OGRStyleTableShadow* table) {
    if( table != NULL )
        GDALDatasetSetStyleTable(self, (OGRStyleTableH) table);
  }

#endif /* defined(SWIGPYTHON) || defined(SWIGJAVA) || defined(SWIGPERL) */

#ifndef SWIGJAVA
  %feature( "kwargs" ) StartTransaction;
#endif
  OGRErr StartTransaction(int force = FALSE)
  {
    return GDALDatasetStartTransaction(self, force);
  }

  OGRErr CommitTransaction()
  {
    return GDALDatasetCommitTransaction(self);
  }

  OGRErr RollbackTransaction()
  {
    return GDALDatasetRollbackTransaction(self);
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

