/******************************************************************************
 * $Id$
 *
 * Name:     MultiDimensional.i
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

%rename (ExtendedDataTypeSubType) GDALExtendedDataTypeSubType;
typedef enum {
    GEDTST_NONE = 0,
    GEDTST_JSON = 1
} GDALExtendedDataTypeSubType;

%rename (Group) GDALGroupHS;

%apply Pointer NONNULL {const char* name};

//************************************************************************
//
// GDALGroup
//
//************************************************************************

class GDALGroupHS {
private:
  GDALGroupHS();
public:
%extend {

  ~GDALGroupHS() {
    GDALGroupRelease(self);
  }

  const char* GetName() {
    return GDALGroupGetName(self);
  }

  const char* GetFullName() {
    return GDALGroupGetFullName(self);
  }

%apply (char **CSL) {char **};
  char **GetMDArrayNames(char** options = 0) {
    return GDALGroupGetMDArrayNames( self, options );
  }
%clear char **;

%newobject OpenMDArray;
  GDALMDArrayHS* OpenMDArray( const char* name, char** options = 0) {
    return GDALGroupOpenMDArray(self, name, options);
  }

%newobject OpenMDArrayFromFullname;
  GDALMDArrayHS* OpenMDArrayFromFullname( const char* name, char** options = 0) {
    return GDALGroupOpenMDArrayFromFullname(self, name, options);
  }

%newobject ResolveMDArray;
  GDALMDArrayHS* ResolveMDArray( const char* name, const char* starting_point, char** options = 0) {
    return GDALGroupResolveMDArray(self, name, starting_point, options);
  }

%apply (char **CSL) {char **};
  char **GetGroupNames(char** options = 0) {
    return GDALGroupGetGroupNames( self, options );
  }
%clear char **;

%newobject OpenGroup;
  GDALGroupHS* OpenGroup( const char* name, char** options = 0) {
    return GDALGroupOpenGroup(self, name, options);
  }

%newobject OpenGroupFromFullname;
  GDALGroupHS* OpenGroupFromFullname( const char* name, char** options = 0) {
    return GDALGroupOpenGroupFromFullname(self, name, options);
  }

%apply (char **CSL) {char **};
  char **GetVectorLayerNames(char** options = 0) {
    return GDALGroupGetVectorLayerNames( self, options );
  }
%clear char **;

  OGRLayerShadow* OpenVectorLayer( const char* name, char** options = 0) {
    return (OGRLayerShadow*) GDALGroupOpenVectorLayer(self, name, options);
  }

#if defined(SWIGPYTHON)
  void GetDimensions( GDALDimensionHS*** pdims, size_t* pnCount, char** options = 0 ) {
    *pdims = GDALGroupGetDimensions(self, pnCount, options);
  }
#endif

%newobject GetAttribute;
  GDALAttributeHS* GetAttribute( const char* name) {
    return GDALGroupGetAttribute(self, name);
  }

#if defined(SWIGPYTHON)
  void GetAttributes( GDALAttributeHS*** pattrs, size_t* pnCount, char** options = 0 ) {
    *pattrs = GDALGroupGetAttributes(self, pnCount, options);
  }
#endif

%apply (char **dict) { char ** };
  char ** GetStructuralInfo () {
    return GDALGroupGetStructuralInfo( self );
  }
%clear char **;

%newobject CreateGroup;
  GDALGroupHS *CreateGroup( const char *name,
                            char **options = 0 ) {
    return GDALGroupCreateGroup(self, name, options);
  }

%newobject CreateDimension;
  GDALDimensionHS *CreateDimension( const char *name,
                                    const char* type,
                                    const char* direction,
                                    unsigned long long size,
                                    char **options = 0 ) {
    return GDALGroupCreateDimension(self, name, type, direction, size, options);
  }

#if defined(SWIGPYTHON)
%newobject CreateMDArray;
%apply (int object_list_count, GDALDimensionHS **poObjects) {(int nDimensions, GDALDimensionHS **dimensions)};
%apply Pointer NONNULL {GDALExtendedDataTypeHS* data_type};
  GDALMDArrayHS *CreateMDArray(const char* name,
                               int nDimensions,
                               GDALDimensionHS** dimensions,
                               GDALExtendedDataTypeHS* data_type,
                               char **options = 0)
  {
    return GDALGroupCreateMDArray(self, name, nDimensions, dimensions,
                                  data_type, options);
  }
%clear (int nDimensions, GDALDimensionHS **dimensions);
#endif

%newobject CreateAttribute;
%apply (int nList, GUIntBig* pList) {(int nDimensions, GUIntBig *dimensions)};
  GDALAttributeHS *CreateAttribute( const char *name,
                                    int nDimensions,
                                    GUIntBig *dimensions,
                                    GDALExtendedDataTypeHS* data_type,
                                    char **options = 0)
  {
    return GDALGroupCreateAttribute(self, name, nDimensions,
                                    (const GUInt64*)dimensions,
                                    data_type, options);
  }

} /* extend */
}; /* GDALGroupH */

//************************************************************************
//
// Statistics
//
//************************************************************************

#ifndef SWIGCSHARP
%{
typedef struct
{
  double min;
  double max;
  double mean;
  double std_dev;
  GIntBig valid_count;
} Statistics;
%}

struct Statistics
{
%immutable;
  double min;
  double max;
  double mean;
  double std_dev;
  GIntBig valid_count;
%mutable;

%extend {

  ~Statistics() {
    CPLFree(self);
  }
} /* extend */
} /* Statistics */ ;
#endif

//************************************************************************
//
// GDALMDArray
//
//************************************************************************

%{
#include <limits>

static bool CheckNumericDataType(GDALExtendedDataTypeHS* dt)
{
    GDALExtendedDataTypeClass klass = GDALExtendedDataTypeGetClass(dt);
    if( klass == GEDTC_NUMERIC )
        return true;
    if( klass == GEDTC_STRING )
        return false;
    CPLAssert( klass == GEDTC_COMPOUND );
    size_t nCount = 0;
    GDALEDTComponentH* comps = GDALExtendedDataTypeGetComponents(dt, &nCount);
    bool ret = true;
    for( size_t i = 0; i < nCount; i++ )
    {
        GDALExtendedDataTypeH tmpType = GDALEDTComponentGetType(comps[i]);
        ret = CheckNumericDataType(tmpType);
        GDALExtendedDataTypeRelease(tmpType);
        if( !ret )
            break;
    }
    GDALExtendedDataTypeFreeComponents(comps, nCount);
    return ret;
}

static CPLErr MDArrayReadWriteCheckArguments(GDALMDArrayHS* array,
                                             bool bCheckOnlyDims,
                                             int nDims1, GUIntBig* array_start_idx,
                                             int nDims2, GUIntBig* count,
                                             int nDims3, GIntBig* array_step,
                                             int nDims4, GIntBig* buffer_stride,
                                             GDALExtendedDataTypeHS* buffer_datatype,
                                             size_t* pnBufferSize)
{
    const int nExpectedDims = (int)GDALMDArrayGetDimensionCount(array);
    if( nDims1 != nExpectedDims )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in array_start_idx");
        return CE_Failure;
    }
    if( nDims2 != nExpectedDims )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in count");
        return CE_Failure;
    }
    if( nDims3 != nExpectedDims )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in array_step");
        return CE_Failure;
    }
    if( nDims4!= nExpectedDims )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in buffer_stride");
        return CE_Failure;
    }
    if( bCheckOnlyDims )
        return CE_None;
    if( !CheckNumericDataType(buffer_datatype) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "non-numeric buffer data type not supported in SWIG bindings");
        return CE_Failure;
    }
    GIntBig nBufferSize = 0;
    for( int i = 0; i < nExpectedDims; i++ )
    {
        if( count[i] == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "count[%d] = 0 is invalid", i);
            return CE_Failure;
        }
        if( buffer_stride[i] < 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                "Negative value in buffer_stride not supported in SWIG bindings");
            return CE_Failure;
        }
        if( count[i] > 1 && buffer_stride[i] != 0 )
        {
            if( (GUIntBig)buffer_stride[i] > std::numeric_limits<GIntBig>::max() / (count[i] - 1) )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
                return CE_Failure;
            }
            GIntBig nDelta = buffer_stride[i] * (count[i] - 1);
            if( nBufferSize > std::numeric_limits<GIntBig>::max() - nDelta )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
                return CE_Failure;
            }
            nBufferSize += nDelta;
        }
    }
    const size_t nDTSize = GDALExtendedDataTypeGetSize(buffer_datatype);
    if( nDTSize == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nDTSize == 0");
        return CE_Failure;
    }
    if( (GUIntBig)nBufferSize > (GUIntBig)std::numeric_limits<GIntBig>::max() / nDTSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
        return CE_Failure;
    }
    nBufferSize *= nDTSize;
    if( (GUIntBig)nBufferSize > (GUIntBig)std::numeric_limits<GIntBig>::max() - nDTSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
        return CE_Failure;
    }
    nBufferSize += nDTSize;

#if SIZEOF_VOIDP == 4
    if( nBufferSize > INT_MAX )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Integer overflow");
        return CE_Failure;
    }
#endif
    *pnBufferSize = (size_t)nBufferSize;
    return CE_None;
}
%}


%rename (MDArray) GDALMDArrayHS;

class GDALMDArrayHS {
private:
  GDALMDArrayHS();
public:
%extend {

  ~GDALMDArrayHS() {
    GDALMDArrayRelease(self);
  }

  const char* GetName() {
    return GDALMDArrayGetName(self);
  }

  const char* GetFullName() {
    return GDALMDArrayGetFullName(self);
  }

  unsigned long long GetTotalElementsCount() {
    return GDALMDArrayGetTotalElementsCount(self);
  }

  size_t GetDimensionCount() {
    return GDALMDArrayGetDimensionCount(self);
  }

#if defined(SWIGPYTHON)
  void GetDimensions( GDALDimensionHS*** pdims, size_t* pnCount ) {
    *pdims = GDALMDArrayGetDimensions(self, pnCount);
  }
#endif

#if defined(SWIGPYTHON)
  void GetCoordinateVariables( GDALMDArrayHS*** parrays, size_t* pnCount ) {
    *parrays = GDALMDArrayGetCoordinateVariables(self, pnCount);
  }
#endif

#if defined(SWIGPYTHON)
%apply ( GUIntBig** pvals, size_t* pnCount ) { (GUIntBig** psizes, size_t* pnCount ) };
  void GetBlockSize( GUIntBig** psizes, size_t* pnCount ) {
    *psizes = GDALMDArrayGetBlockSize(self, pnCount);
  }
#endif

#if defined(SWIGPYTHON)
%apply ( GUIntBig** pvals, size_t* pnCount ) { (GUIntBig** psizes, size_t* pnCount ) };
  void GetProcessingChunkSize( size_t nMaxChunkMemory, GUIntBig** psizes, size_t* pnCount ) {
     size_t* panTmp = GDALMDArrayGetProcessingChunkSize(self, pnCount, nMaxChunkMemory);
     *psizes = NULL;
     if( panTmp )
     {
        *psizes = (GUIntBig*) CPLMalloc(sizeof(GUIntBig) * (*pnCount));
        for( size_t i = 0; i < *pnCount; ++i )
        {
            (*psizes)[i] = panTmp[i];
        }
        CPLFree(panTmp);
     }
  }
#endif

%newobject GetDataType;
  GDALExtendedDataTypeHS* GetDataType() {
    return GDALMDArrayGetDataType(self);
  }

%apply (char **dict) { char ** };
  char ** GetStructuralInfo () {
    return GDALMDArrayGetStructuralInfo( self );
  }
%clear char **;

#if defined(SWIGPYTHON)
%apply Pointer NONNULL {GDALExtendedDataTypeHS* buffer_datatype};
%apply ( void **outPythonObject ) { (void **buf ) };
%apply (int nList, GUIntBig* pList) {(int nDims1, GUIntBig *array_start_idx)};
%apply (int nList, GUIntBig* pList) {(int nDims2, GUIntBig *count)};
%apply (int nList, GIntBig* pList) {(int nDims3, GIntBig *array_step)};
%apply (int nList, GIntBig* pList) {(int nDims4, GIntBig *buffer_stride)};
  CPLErr Read( int nDims1, GUIntBig* array_start_idx,
               int nDims2, GUIntBig* count,
               int nDims3, GIntBig* array_step,
               int nDims4, GIntBig* buffer_stride,
               GDALExtendedDataTypeHS* buffer_datatype,
               void **buf) {
    *buf = NULL;

    size_t buf_size = 0;
    if( MDArrayReadWriteCheckArguments(self, true,
                                        nDims1, array_start_idx,
                                        nDims2, count,
                                        nDims3, array_step,
                                        nDims4, buffer_stride,
                                        buffer_datatype,
                                        &buf_size) != CE_None )
    {
      return CE_Failure;
    }

    const int nExpectedDims = (int)GDALMDArrayGetDimensionCount(self);
    std::vector<size_t> count_internal(nExpectedDims + 1);
    std::vector<GPtrDiff_t> buffer_stride_internal(nExpectedDims + 1);
    size_t nProductCount = 1;
    for( int i = 0; i < nExpectedDims; i++ )
    {
        count_internal[i] = (size_t)count[i];
        if( count_internal[i] != count[i] )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
        nProductCount *= count_internal[i];
        buffer_stride_internal[i] = (GPtrDiff_t)buffer_stride[i];
        if( buffer_stride_internal[i] != buffer_stride[i] )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
    }

    GDALExtendedDataTypeHS* selfType = GDALMDArrayGetDataType(self);
    bool isSelfString = GDALExtendedDataTypeGetClass(selfType) == GEDTC_STRING;
    GDALExtendedDataTypeRelease(selfType);

    if( GDALExtendedDataTypeGetClass(buffer_datatype) == GEDTC_STRING &&
        isSelfString )
    {
        size_t nExpectedStride = 1;
        for( int i = nExpectedDims; i > 0; )
        {
            --i;
            if( (size_t)buffer_stride_internal[i] != nExpectedStride )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Unhandled stride");
                return CE_Failure;
            }
            nExpectedStride *= count_internal[i];
        }
        char** ppszBuffer = (char**)VSI_CALLOC_VERBOSE(nProductCount, sizeof(char*));
        if( !ppszBuffer )
            return CE_Failure;
        GByte* pabyBuffer = (GByte*)ppszBuffer;
        if( !(GDALMDArrayRead( self,
                            array_start_idx,
                            &count_internal[0],
                            array_step,
                            NULL,
                            buffer_datatype,
                            pabyBuffer,
                            pabyBuffer,
                            nProductCount * sizeof(char*) )) )
        {
            for( size_t i = 0; i < nProductCount; i++ )
                VSIFree(ppszBuffer[i]);
            VSIFree(pabyBuffer);
            return CE_Failure;
        }

        SWIG_PYTHON_THREAD_BEGIN_BLOCK;
        PyObject* obj = PyList_New( nProductCount );
        for( size_t i = 0; i < nProductCount; i++ )
        {
            if( !ppszBuffer[i] )
            {
                Py_INCREF(Py_None);
                PyList_SetItem(obj, i, Py_None);
            }
            else
            {
                PyList_SetItem(obj, i, GDALPythonObjectFromCStr( ppszBuffer[i] ) );
            }
            VSIFree(ppszBuffer[i]);
        }
        SWIG_PYTHON_THREAD_END_BLOCK;
        *buf = obj;
        VSIFree(pabyBuffer);
        return CE_None;
    }

    if( MDArrayReadWriteCheckArguments(self, false,
                                        nDims1, array_start_idx,
                                        nDims2, count,
                                        nDims3, array_step,
                                        nDims4, buffer_stride,
                                        buffer_datatype,
                                        &buf_size) != CE_None )
    {
      return CE_Failure;
    }
    if( buf_size == 0 )
    {
        return CE_None;
    }


    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    *buf = (void *)PyByteArray_FromStringAndSize( NULL, buf_size );
    if (*buf == NULL)
    {
        *buf = Py_None;
        if( !bUseExceptions )
        {
            PyErr_Clear();
        }
        SWIG_PYTHON_THREAD_END_BLOCK;
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyByteArray_AsString( (PyObject *)*buf );
    SWIG_PYTHON_THREAD_END_BLOCK;

    memset(data, 0, buf_size);

    CPLErr eErr = GDALMDArrayRead( self,
                                   array_start_idx,
                                   &count_internal[0],
                                   array_step,
                                   &buffer_stride_internal[0],
                                   buffer_datatype,
                                   data,
                                   data,
                                   buf_size ) ? CE_None : CE_Failure;
    if (eErr == CE_Failure)
    {
        SWIG_PYTHON_THREAD_BEGIN_BLOCK;
        Py_DECREF((PyObject*)*buf);
        SWIG_PYTHON_THREAD_END_BLOCK;
        *buf = NULL;
    }

    return eErr;
  }
%clear (void **buf );

  CPLErr WriteStringArray( int nDims1, GUIntBig* array_start_idx,
               int nDims2, GUIntBig* count,
               int nDims3, GIntBig* array_step,
               GDALExtendedDataTypeHS* buffer_datatype,
               char** options)
  {

    const int nExpectedDims = (int)GDALMDArrayGetDimensionCount(self);
    std::vector<size_t> count_internal(nExpectedDims);
    if( nExpectedDims != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Unsupported number of dimensions");
        return CE_Failure;
    }
    for( int i = 0; i < nExpectedDims; i++ )
    {
        count_internal[i] = (size_t)count[i];
        if( count_internal[i] != count[i] )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
    }
    if( nDims1 != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in array_start_idx");
        return CE_Failure;
    }
    if( nDims2 != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in count");
        return CE_Failure;
    }
    if( nDims3 != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in array_step");
        return CE_Failure;
    }

    CPLErr eErr = GDALMDArrayWrite(self,
                                   array_start_idx,
                                   &count_internal[0],
                                   array_step,
                                   NULL,
                                   buffer_datatype,
                                   options,
                                   options,
                                   CSLCount(options) * sizeof(char*) ) ? CE_None : CE_Failure;
    return eErr;
  }


%apply Pointer NONNULL {GDALExtendedDataTypeHS* buffer_datatype};
%apply (GIntBig nLen, char *pBuf) { (GIntBig buf_len, char *buf_string) };
%apply (int nList, GUIntBig* pList) {(int nDims1, GUIntBig *array_start_idx)};
%apply (int nList, GUIntBig* pList) {(int nDims2, GUIntBig *count)};
%apply (int nList, GIntBig* pList) {(int nDims3, GIntBig *array_step)};
%apply (int nList, GIntBig* pList) {(int nDims4, GIntBig *buffer_stride)};
  CPLErr Write( int nDims1, GUIntBig* array_start_idx,
               int nDims2, GUIntBig* count,
               int nDims3, GIntBig* array_step,
               int nDims4, GIntBig* buffer_stride,
               GDALExtendedDataTypeHS* buffer_datatype,
               GIntBig buf_len, char *buf_string) {

    size_t buf_size = 0;
    if( MDArrayReadWriteCheckArguments(self, false,
                                        nDims1, array_start_idx,
                                        nDims2, count,
                                        nDims3, array_step,
                                        nDims4, buffer_stride,
                                        buffer_datatype,
                                        &buf_size) != CE_None )
    {
      return CE_Failure;
    }

    if ( (GUIntBig)buf_len < buf_size )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
        return CE_Failure;
    }

    const int nExpectedDims = (int)GDALMDArrayGetDimensionCount(self);
    std::vector<size_t> count_internal(nExpectedDims+1);
    std::vector<GPtrDiff_t> buffer_stride_internal(nExpectedDims+1);
    for( int i = 0; i < nExpectedDims; i++ )
    {
        count_internal[i] = (size_t)count[i];
        if( count_internal[i] != count[i] )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
        buffer_stride_internal[i] = (GPtrDiff_t)buffer_stride[i];
        if( buffer_stride_internal[i] != buffer_stride[i] )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
    }

    CPLErr eErr = GDALMDArrayWrite( self,
                                   array_start_idx,
                                   &count_internal[0],
                                   array_step,
                                   &buffer_stride_internal[0],
                                   buffer_datatype,
                                   buf_string,
                                   buf_string,
                                   (size_t)buf_len ) ? CE_None : CE_Failure;
    return eErr;
  }
%clear (void **buf );


%apply (int nList, GUIntBig* pList) {(int nDims1, GUIntBig *array_start_idx)};
%apply (int nList, GUIntBig* pList) {(int nDims2, GUIntBig *count)};
  CPLErr AdviseRead( int nDims1, GUIntBig* array_start_idx,
                     int nDims2, GUIntBig* count ) {

    const int nExpectedDims = (int)GDALMDArrayGetDimensionCount(self);
    if( nDims1 != nExpectedDims )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in array_start_idx");
        return CE_Failure;
    }
    if( nDims2 != nExpectedDims )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Wrong number of values in count");
        return CE_Failure;
    }

    std::vector<size_t> count_internal(nExpectedDims+1);
    for( int i = 0; i < nExpectedDims; i++ )
    {
        count_internal[i] = (size_t)count[i];
        if( count_internal[i] != count[i] )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
            return CE_Failure;
        }
    }

    if( !(GDALMDArrayAdviseRead( self, array_start_idx, count_internal.data() )) )
    {
        return CE_Failure;
    }
    return CE_None;
  }
#endif

%newobject GetAttribute;
  GDALAttributeHS* GetAttribute( const char* name) {
    return GDALMDArrayGetAttribute(self, name);
  }

#if defined(SWIGPYTHON)
  void GetAttributes( GDALAttributeHS*** pattrs, size_t* pnCount, char** options = 0 ) {
    *pattrs = GDALMDArrayGetAttributes(self, pnCount, options);
  }
#endif

%newobject CreateAttribute;
%apply (int nList, GUIntBig* pList) {(int nDimensions, GUIntBig *dimensions)};
  GDALAttributeHS *CreateAttribute( const char *name,
                                    int nDimensions,
                                    GUIntBig *dimensions,
                                    GDALExtendedDataTypeHS* data_type,
                                    char **options = 0)
  {
    return GDALMDArrayCreateAttribute(self, name, nDimensions,
                                    (const GUInt64*)dimensions,
                                    data_type, options);
  }

#if defined(SWIGPYTHON)
%apply ( void **outPythonObject ) { (void **buf ) };
  CPLErr GetNoDataValueAsRaw( void **buf) {
    *buf = NULL;
    const void* pabyBuf = GDALMDArrayGetRawNoDataValue(self);
    if( pabyBuf == NULL )
    {
      return CE_Failure;
    }
    GDALExtendedDataTypeHS* selfType = GDALMDArrayGetDataType(self);
    const size_t buf_size = GDALExtendedDataTypeGetSize(selfType);
    GDALExtendedDataTypeRelease(selfType);

    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    *buf = (void *)PyByteArray_FromStringAndSize( NULL, buf_size );
    if (*buf == NULL)
    {
        *buf = Py_None;
        if( !bUseExceptions )
        {
            PyErr_Clear();
        }
        SWIG_PYTHON_THREAD_END_BLOCK;
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyByteArray_AsString( (PyObject *)*buf );
    SWIG_PYTHON_THREAD_END_BLOCK;

    memcpy(data, pabyBuf, buf_size);

    return CE_None;
  }
%clear (void **buf );
#endif

  void GetNoDataValueAsDouble( double *val, int *hasval ) {
    *val = GDALMDArrayGetNoDataValueAsDouble( self, hasval );
  }

  retStringAndCPLFree* GetNoDataValueAsString() {
    GDALExtendedDataTypeHS* selfType = GDALMDArrayGetDataType(self);
    const size_t typeClass = GDALExtendedDataTypeGetClass(selfType);
    GDALExtendedDataTypeRelease(selfType);

    if( typeClass != GEDTC_STRING )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Data type is not string");
        return NULL;
    }
    const void* pabyBuf = GDALMDArrayGetRawNoDataValue(self);
    if( pabyBuf == NULL )
    {
      return NULL;
    }
    const char* ret = *reinterpret_cast<const char* const*>(pabyBuf);
    if( ret )
        return CPLStrdup(ret);
    return NULL;
  }

  CPLErr SetNoDataValueDouble( double d ) {
    return GDALMDArraySetNoDataValueAsDouble( self, d ) ? CE_None : CE_Failure;
  }

  CPLErr SetNoDataValueString( const char* nodata ) {
    GDALExtendedDataTypeHS* selfType = GDALMDArrayGetDataType(self);
    const size_t typeClass = GDALExtendedDataTypeGetClass(selfType);
    GDALExtendedDataTypeRelease(selfType);

    if( typeClass != GEDTC_STRING )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Data type is not string");
        return CE_Failure;
    }
    return GDALMDArraySetRawNoDataValue(self, &nodata) ? CE_None : CE_Failure;
  }

#if defined(SWIGPYTHON)
  CPLErr SetNoDataValueRaw(GIntBig nLen, char *pBuf)
  {
    GDALExtendedDataTypeHS* selfType = GDALMDArrayGetDataType(self);
    const size_t selfTypeSize = GDALExtendedDataTypeGetSize(selfType);
    GDALExtendedDataTypeRelease(selfType);

    if( static_cast<size_t>(nLen) != selfTypeSize )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Argument of wrong size");
        return CE_Failure;
    }
    return GDALMDArraySetRawNoDataValue(self, pBuf) ? CE_None : CE_Failure;
  }
#endif

  CPLErr DeleteNoDataValue() {
    return GDALMDArraySetRawNoDataValue( self, NULL ) ? CE_None : CE_Failure;
  }

  void GetOffset( double *val, int *hasval ) {
    *val = GDALMDArrayGetOffset( self, hasval );
  }

  GDALDataType GetOffsetStorageType() {
    GDALDataType eDT = GDT_Unknown;
    int hasval = FALSE;
    GDALMDArrayGetOffsetEx( self, &hasval, &eDT );
    return hasval ? eDT : GDT_Unknown;
  }

  void GetScale( double *val, int *hasval ) {
    *val = GDALMDArrayGetScale( self, hasval );
  }

  GDALDataType GetScaleStorageType() {
    GDALDataType eDT = GDT_Unknown;
    int hasval = FALSE;
    GDALMDArrayGetScaleEx( self, &hasval, &eDT );
    return hasval ? eDT : GDT_Unknown;
  }

%feature ("kwargs") SetOffset;
  CPLErr SetOffset( double val, GDALDataType storageType = GDT_Unknown ) {
    return GDALMDArraySetOffsetEx( self, val, storageType ) ? CE_None : CE_Failure;
  }

%feature ("kwargs") SetScale;
  CPLErr SetScale( double val, GDALDataType storageType = GDT_Unknown ) {
    return GDALMDArraySetScaleEx( self, val, storageType ) ? CE_None : CE_Failure;
  }

  CPLErr SetUnit(const char* unit) {
    return GDALMDArraySetUnit(self, unit) ? CE_None : CE_Failure;
  }

  const char* GetUnit() {
    return GDALMDArrayGetUnit(self);
  }

#ifndef SWIGCSHARP
  OGRErr SetSpatialRef(OSRSpatialReferenceShadow* srs)
  {
     return GDALMDArraySetSpatialRef( self, (OGRSpatialReferenceH)srs ) ? CE_None : CE_Failure;
  }

  %newobject GetSpatialRef;
  OSRSpatialReferenceShadow *GetSpatialRef() {
    return GDALMDArrayGetSpatialRef(self);
  }
#endif

%newobject GetView;
%apply Pointer NONNULL {const char* viewExpr};
  GDALMDArrayHS* GetView(const char* viewExpr)
  {
    return GDALMDArrayGetView(self, viewExpr);
  }

%newobject Transpose;
  GDALMDArrayHS* Transpose(int nList, int* pList)
  {
    return GDALMDArrayTranspose(self, nList, pList);
  }

%newobject GetUnscaled;
  GDALMDArrayHS* GetUnscaled()
  {
    return GDALMDArrayGetUnscaled(self);
  }

%newobject GetMask;
%apply (char **CSL) {char **};
  GDALMDArrayHS* GetMask(char** options = 0)
  {
    return GDALMDArrayGetMask(self, options);
  }
%clear char **;

%newobject AsClassicDataset;
  GDALDatasetShadow* AsClassicDataset(size_t iXDim, size_t iYDim)
  {
    return (GDALDatasetShadow*)GDALMDArrayAsClassicDataset(self, iXDim, iYDim);
  }

#ifndef SWIGCSHARP
%newobject Statistics;
%feature ("kwargs") GetStatistics;
  Statistics* GetStatistics( GDALDatasetShadow* ds = NULL,
                             bool approx_ok = FALSE,
                             bool force = TRUE,
                             GDALProgressFunc callback = NULL,
                             void* callback_data=NULL)
  {
        GUInt64 nValidCount = 0;
        Statistics* psStatisticsOut = (Statistics*)CPLMalloc(sizeof(Statistics));
        CPLErr eErr = GDALMDArrayGetStatistics(self, ds, approx_ok, force,
                                 &(psStatisticsOut->min),
                                 &(psStatisticsOut->max),
                                 &(psStatisticsOut->mean),
                                 &(psStatisticsOut->std_dev),
                                 &nValidCount,
                                 callback, callback_data);
        psStatisticsOut->valid_count = static_cast<GIntBig>(nValidCount);
        if( eErr == CE_None )
            return psStatisticsOut;
        CPLFree(psStatisticsOut);
        return NULL;
  }

%newobject Statistics;
%feature ("kwargs") ComputeStatistics;
  Statistics* ComputeStatistics( GDALDatasetShadow* ds = NULL,
                                 bool approx_ok = FALSE,
                                 GDALProgressFunc callback = NULL,
                                 void* callback_data=NULL)
  {
        GUInt64 nValidCount = 0;
        Statistics* psStatisticsOut = (Statistics*)CPLMalloc(sizeof(Statistics));
        int nSuccess = GDALMDArrayComputeStatistics(self, ds, approx_ok,
                                 &(psStatisticsOut->min),
                                 &(psStatisticsOut->max),
                                 &(psStatisticsOut->mean),
                                 &(psStatisticsOut->std_dev),
                                 &nValidCount,
                                 callback, callback_data);
        psStatisticsOut->valid_count = static_cast<GIntBig>(nValidCount);
        if( nSuccess )
            return psStatisticsOut;
        CPLFree(psStatisticsOut);
        return NULL;
  }
#endif

#if defined(SWIGPYTHON)
%newobject GetResampled;
%apply (int object_list_count, GDALDimensionHS **poObjectsItemMaybeNull) {(int nDimensions, GDALDimensionHS **dimensions)};
%apply (OSRSpatialReferenceShadow **optional_OSRSpatialReferenceShadow) { OSRSpatialReferenceShadow** };
  GDALMDArrayHS *GetResampled(int nDimensions,
                              GDALDimensionHS** dimensions,
                              GDALRIOResampleAlg resample_alg,
                              OSRSpatialReferenceShadow** srs,
                              char **options = 0)
  {
    return GDALMDArrayGetResampled(self, nDimensions, dimensions,
                                  resample_alg, srs ? *srs : NULL, options);
  }
%clear (int nDimensions, GDALDimensionHS **dimensions);
%clear OSRSpatialReferenceShadow**;
#endif

  bool Cache( char** options = NULL )
  {
      return GDALMDArrayCache(self, options);
  }

} /* extend */
}; /* GDALMDArrayH */


//************************************************************************
//
// GDALAttribute
//
//************************************************************************

%rename (Attribute) GDALAttributeHS;

class GDALAttributeHS {
private:
  GDALAttributeHS();
public:
%extend {

  ~GDALAttributeHS() {
    GDALAttributeRelease(self);
  }

  const char* GetName() {
    return GDALAttributeGetName(self);
  }

  const char* GetFullName() {
    return GDALAttributeGetFullName(self);
  }

  unsigned long long GetTotalElementsCount() {
    return GDALAttributeGetTotalElementsCount(self);
  }

  size_t GetDimensionCount() {
    return GDALAttributeGetDimensionCount(self);
  }

#if defined(SWIGPYTHON)
%apply ( GUIntBig** pvals, size_t* pnCount ) { (GUIntBig** pdims, size_t* pnCount ) };
  void GetDimensionsSize( GUIntBig** pdims, size_t* pnCount ) {
    *pdims = GDALAttributeGetDimensionsSize(self, pnCount);
  }
#endif

%newobject GetDataType;
  GDALExtendedDataTypeHS* GetDataType() {
    return GDALAttributeGetDataType(self);
  }

#if defined(SWIGPYTHON)
%apply ( void **outPythonObject ) { (void **buf ) };
  CPLErr ReadAsRaw( void **buf) {
    *buf = NULL;
    GDALExtendedDataTypeHS* dt = GDALAttributeGetDataType(self);
    bool bIsNumeric = CheckNumericDataType(dt);
    GDALExtendedDataTypeRelease(dt);
    if( !bIsNumeric )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "non-numeric buffer data type not supported in SWIG bindings");
        return CE_Failure;
    }
    size_t buf_size = 0;
    GByte* pabyBuf = GDALAttributeReadAsRaw(self, &buf_size);
    if( pabyBuf == NULL )
    {
      return CE_Failure;
    }

    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    *buf = (void *)PyBytes_FromStringAndSize( NULL, buf_size );
    if (*buf == NULL)
    {
        *buf = Py_None;
        if( !bUseExceptions )
        {
            PyErr_Clear();
        }
        SWIG_PYTHON_THREAD_END_BLOCK;
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        GDALAttributeFreeRawResult(self, pabyBuf, buf_size);
        return CE_Failure;
    }
    char *data = PyBytes_AsString( (PyObject *)*buf );
    SWIG_PYTHON_THREAD_END_BLOCK;

    memcpy(data, pabyBuf, buf_size);
    GDALAttributeFreeRawResult(self, pabyBuf, buf_size);

    return CE_None;
  }
%clear (void **buf );
#endif

  const char* ReadAsString() {
    return GDALAttributeReadAsString(self);
  }

  int ReadAsInt() {
    return GDALAttributeReadAsInt(self);
  }

  double ReadAsDouble() {
    return GDALAttributeReadAsDouble(self);
  }

%apply (char **CSL) {char **};
  char** ReadAsStringArray() {
    return GDALAttributeReadAsStringArray(self);
  }
%clear char **;

#if defined(SWIGPYTHON)
  void ReadAsIntArray( int** pvals, size_t* pnCount ) {
    *pvals = GDALAttributeReadAsIntArray(self, pnCount);
  }
#endif

#if defined(SWIGPYTHON)
  void ReadAsDoubleArray( double** pvals, size_t* pnCount ) {
    *pvals = GDALAttributeReadAsDoubleArray(self, pnCount);
  }
#endif

#if defined(SWIGPYTHON)
  CPLErr WriteRaw(GIntBig nLen, char *pBuf)
  {
    GDALExtendedDataTypeHS* dt = GDALAttributeGetDataType(self);
    bool bIsNumeric = CheckNumericDataType(dt);
    GDALExtendedDataTypeRelease(dt);
    if( !bIsNumeric )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "non-numeric buffer data type not supported in SWIG bindings");
        return CE_Failure;
    }
    return GDALAttributeWriteRaw(self, pBuf, nLen) ? CE_None : CE_Failure;
  }
#endif

  CPLErr WriteString(const char* val)
  {
    return GDALAttributeWriteString(self, val) ? CE_None : CE_Failure;
  }

%apply (char **options ) { (char **vals) };
  CPLErr WriteStringArray(char** vals)
  {
    return GDALAttributeWriteStringArray(self, vals) ? CE_None : CE_Failure;
  }
%clear (char **vals);

  CPLErr WriteInt(int val)
  {
    return GDALAttributeWriteInt(self, val) ? CE_None : CE_Failure;
  }

  CPLErr WriteDouble(double val)
  {
    return GDALAttributeWriteDouble(self, val) ? CE_None : CE_Failure;
  }

#if defined(SWIGPYTHON)
  CPLErr WriteDoubleArray(int nList, double* pList)
  {
    return GDALAttributeWriteDoubleArray(self, pList, nList) ? CE_None : CE_Failure;
  }
#endif

} /* extend */
}; /* GDALAttributeH */


//************************************************************************
//
// GDALDimension
//
//************************************************************************

%rename (Dimension) GDALDimensionHS;

class GDALDimensionHS {
private:
  GDALDimensionHS();
public:
%extend {

  ~GDALDimensionHS() {
    GDALDimensionRelease(self);
  }

  const char* GetName() {
    return GDALDimensionGetName(self);
  }

  const char* GetFullName() {
    return GDALDimensionGetFullName(self);
  }

#if defined(SWIGCSHARP)
  const char* GetType_()
#else
  const char* GetType()
#endif
  {
    return GDALDimensionGetType(self);
  }

  const char* GetDirection() {
    return GDALDimensionGetDirection(self);
  }

  unsigned long long GetSize() {
    return GDALDimensionGetSize(self);
  }

%newobject GetIndexingVariable;
  GDALMDArrayHS* GetIndexingVariable() {
    return GDALDimensionGetIndexingVariable(self);
  }

  bool SetIndexingVariable(GDALMDArrayHS* array) {
    return GDALDimensionSetIndexingVariable(self, array);
  }

} /* extend */
}; /* GDALDimensionH */


//************************************************************************
//
// GDALExtendedDataTypeClass
//
//************************************************************************

%rename (ExtendedDataTypeClass) GDALExtendedDataTypeClass;

typedef enum {
    GEDTC_NUMERIC,
    GEDTC_STRING,
    GEDTC_COMPOUND
} GDALExtendedDataTypeClass;

//************************************************************************
//
// GDALExtendedDataType
//
//************************************************************************

%rename (ExtendedDataType) GDALExtendedDataTypeHS;

class GDALExtendedDataTypeHS {
private:
  GDALExtendedDataTypeHS();
public:
%extend {

  ~GDALExtendedDataTypeHS() {
    GDALExtendedDataTypeRelease(self);
  }

%newobject Create;
  static GDALExtendedDataTypeHS* Create(GDALDataType dt)
  {
    return GDALExtendedDataTypeCreate(dt);
  }

%newobject CreateString;
  static GDALExtendedDataTypeHS* CreateString(size_t nMaxStringLength = 0)
  {
    return GDALExtendedDataTypeCreateString(nMaxStringLength);
  }

#if defined(SWIGPYTHON)
%newobject CreateCompound;
%apply (int object_list_count, GDALEDTComponentHS **poObjects) {(int nComps, GDALEDTComponentHS **comps)};
  static GDALExtendedDataTypeHS* CreateCompound(const char* name,
                                                size_t nTotalSize,
                                                int nComps,
                                                GDALEDTComponentHS** comps)
  {
    return GDALExtendedDataTypeCreateCompound(name, nTotalSize, nComps, comps);
  }
%clear (int nComps, GDALEDTComponentHS **comps);
#endif

  const char* GetName()
  {
    return GDALExtendedDataTypeGetName(self);
  }

  GDALExtendedDataTypeClass GetClass()
  {
    return GDALExtendedDataTypeGetClass(self);
  }

  GDALDataType GetNumericDataType()
  {
    return GDALExtendedDataTypeGetNumericDataType(self);
  }

  size_t GetSize()
  {
    return GDALExtendedDataTypeGetSize(self);
  }

  size_t GetMaxStringLength()
  {
    return GDALExtendedDataTypeGetMaxStringLength(self);
  }

  GDALExtendedDataTypeSubType GetSubType() {
    return GDALExtendedDataTypeGetSubType(self);
  }

#if defined(SWIGPYTHON)
  void GetComponents( GDALEDTComponentHS*** pcomps, size_t* pnCount ) {
    *pcomps = GDALExtendedDataTypeGetComponents(self, pnCount);
  }
#endif

%apply Pointer NONNULL {GDALExtendedDataTypeHS* other};
  bool CanConvertTo(GDALExtendedDataTypeHS* other)
  {
    return GDALExtendedDataTypeCanConvertTo(self, other);
  }

  bool Equals(GDALExtendedDataTypeHS* other)
  {
    return GDALExtendedDataTypeEquals(self, other);
  }

} /* extend */
}; /* GDALExtendedDataTypeH */


//************************************************************************
//
// GDALExtendedDataType
//
//************************************************************************

%rename (EDTComponent) GDALEDTComponentHS;

class GDALEDTComponentHS {
private:
  GDALEDTComponentHS();
public:
%extend {

  ~GDALEDTComponentHS() {
    GDALEDTComponentRelease(self);
  }

%newobject Create;
%apply Pointer NONNULL {const char* name};
%apply Pointer NONNULL {GDALExtendedDataTypeHS* type};
  static GDALEDTComponentHS* Create(const char* name, size_t offset, GDALExtendedDataTypeHS* type)
  {
    return GDALEDTComponentCreate(name, offset, type);
  }

  const char* GetName()
  {
    return GDALEDTComponentGetName(self);
  }

  size_t GetOffset()
  {
    return GDALEDTComponentGetOffset(self);
  }

#if defined(SWIGCSHARP)
%newobject GetType_;
  GDALExtendedDataTypeHS* GetType_()
#else
%newobject GetType;
  GDALExtendedDataTypeHS* GetType()
#endif
  {
    return GDALEDTComponentGetType(self);
  }


} /* extend */
}; /* GDALEDTComponentHS */

