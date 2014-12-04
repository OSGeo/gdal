/******************************************************************************
 * $Id$
 *
 * Name:     gdal_array.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL / Numpy interface
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 
%feature("autodoc");

%module gdal_array

%include constraints.i

%import typemaps_python.i

%import MajorObject.i
%import Band.i
%import RasterAttributeTable.i

%include "cplvirtualmem.i"

%init %{
  import_array();
  GDALRegister_NUMPY();
%}

typedef int CPLErr;
typedef int GDALRIOResampleAlg;

%include "python_strings.i"


%{
#include "gdal_priv.h"
#ifdef _DEBUG
#undef _DEBUG
#include "Python.h"
#define _DEBUG
#else
#include "Python.h"
#endif
#include "numpy/arrayobject.h"

#ifdef DEBUG 
typedef struct GDALRasterBandHS GDALRasterBandShadow;
typedef struct RasterAttributeTableHS GDALRasterAttributeTableShadow;
#else
typedef void GDALRasterBandShadow;
typedef void GDALRasterAttributeTableShadow;
#endif

CPL_C_START

GDALRasterBandH CPL_DLL MEMCreateRasterBand( GDALDataset *, int, GByte *,
                                             GDALDataType, int, int, int );
CPL_C_END

typedef char retStringAndCPLFree;

class NUMPYDataset : public GDALDataset
{
    PyArrayObject *psArray;

    double	  adfGeoTransform[6];
    char	  *pszProjection;

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

  public:
                 NUMPYDataset();
                 ~NUMPYDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    static GDALDataset *Open( GDALOpenInfo * );
};



/************************************************************************/
/*                          GDALRegister_NUMPY()                        */
/************************************************************************/
   
static void GDALRegister_NUMPY(void)

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "NUMPY" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "NUMPY" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Numeric Python Array" );
        
        poDriver->pfnOpen = NUMPYDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );

    }
}

/************************************************************************/
/*                            NUMPYDataset()                            */
/************************************************************************/

NUMPYDataset::NUMPYDataset()

{
    pszProjection = CPLStrdup("");
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");
}

/************************************************************************/
/*                            ~NUMPYDataset()                            */
/************************************************************************/

NUMPYDataset::~NUMPYDataset()

{
    CPLFree( pszProjection );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    FlushCache();
    Py_DECREF( psArray );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NUMPYDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr NUMPYDataset::SetProjection( const char * pszNewProjection )

{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NUMPYDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr NUMPYDataset::SetGeoTransform( double * padfTransform )

{
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
    return( CE_None );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int NUMPYDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *NUMPYDataset::GetGCPProjection()

{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *NUMPYDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr NUMPYDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                              const char *pszGCPProjection )

{
    CPLFree( this->pszGCPProjection );
    if( this->nGCPCount > 0 )
    {
        GDALDeinitGCPs( this->nGCPCount, this->pasGCPList );
        CPLFree( this->pasGCPList );
    }

    this->pszGCPProjection = CPLStrdup(pszGCPProjection);

    this->nGCPCount = nGCPCount;

    this->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NUMPYDataset::Open( GDALOpenInfo * poOpenInfo )

{
    PyArrayObject *psArray;
    GDALDataType  eType;
    int     nBands;

/* -------------------------------------------------------------------- */
/*      Is this a numpy dataset name?                                   */
/* -------------------------------------------------------------------- */
    if( !EQUALN(poOpenInfo->pszFilename,"NUMPY:::",8) 
        || poOpenInfo->fpL != NULL )
        return NULL;

    psArray = NULL;
    sscanf( poOpenInfo->pszFilename+8, "%p", &(psArray) );
    if( psArray == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to parse meaningful pointer value from NUMPY name\n"
                  "string: %s\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we likely have corrupt definitions of the NUMPY stuff,       */
/*      then warn now.                                                  */
/* -------------------------------------------------------------------- */
#ifdef NUMPY_DEFS_WRONG
    CPLError( CE_Warning, CPLE_AppDefined, 
              "It would appear you have built GDAL without having it use\n"
              "the Numeric python include files.  Old definitions have\n"
              "been used instead at build time, and it is quite possible that\n"
              "the things will shortly fail or crash if they are wrong.\n"
              "Consider installing Numeric, and rebuilding with HAVE_NUMPY\n"
              "enabled in gdal\nmake.opt." );
#endif

/* -------------------------------------------------------------------- */
/*      Is this a directly mappable Python array?  Verify rank, and     */
/*      data type.                                                      */
/* -------------------------------------------------------------------- */

    if( psArray->nd < 2 || psArray->nd > 3 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Illegal numpy array rank %d.\n", 
                  psArray->nd );
        return NULL;
    }

    switch( psArray->descr->type_num )
    {
      case NPY_CDOUBLE:
        eType = GDT_CFloat64;
        break;

      case NPY_CFLOAT:
        eType = GDT_CFloat32;
        break;

      case NPY_DOUBLE:
        eType = GDT_Float64;
        break;

      case NPY_FLOAT:
        eType = GDT_Float32;
        break;

      case NPY_INT:
      case NPY_LONG:
        eType = GDT_Int32;
        break;

      case NPY_UINT:
      case NPY_ULONG:
        eType = GDT_UInt32;
        break;

      case NPY_SHORT:
        eType = GDT_Int16;
        break;

      case NPY_USHORT:
        eType = GDT_UInt16;
        break;

      case NPY_BYTE:
      case NPY_UBYTE:
        eType = GDT_Byte;
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to access numpy arrays of typecode `%c'.\n", 
                  psArray->descr->type );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the new NUMPYDataset object.                             */
/* -------------------------------------------------------------------- */
    NUMPYDataset *poDS;

    poDS = new NUMPYDataset();

    poDS->psArray = psArray;

    poDS->eAccess = GA_ReadOnly;

/* -------------------------------------------------------------------- */
/*      Add a reference to the array.                                   */
/* -------------------------------------------------------------------- */
    Py_INCREF( psArray );

/* -------------------------------------------------------------------- */
/*      Workout the data layout.                                        */
/* -------------------------------------------------------------------- */
    int    nBandOffset;
    int    nPixelOffset;
    int    nLineOffset;

    if( psArray->nd == 3 )
    {
        nBands = psArray->dimensions[0];
        nBandOffset = psArray->strides[0];
        poDS->nRasterXSize = psArray->dimensions[2];
        nPixelOffset = psArray->strides[2];
        poDS->nRasterYSize = psArray->dimensions[1];
        nLineOffset = psArray->strides[1];
    }
    else
    {
        nBands = 1;
        nBandOffset = 0;
        poDS->nRasterXSize = psArray->dimensions[1];
        nPixelOffset = psArray->strides[1];
        poDS->nRasterYSize = psArray->dimensions[0];
        nLineOffset = psArray->strides[0];
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, 
                       (GDALRasterBand *) 
                       MEMCreateRasterBand( poDS, iBand+1, 
                                (GByte *) psArray->data + nBandOffset*iBand,
                                          eType, nPixelOffset, nLineOffset,
                                          FALSE ) );
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    return poDS;
}

%}

// So that SWIGTYPE_p_f_double_p_q_const__char_p_void__int is declared...
/************************************************************************/
/*                            TermProgress()                            */
/************************************************************************/

%rename (TermProgress_nocb) GDALTermProgress_nocb;
%feature( "kwargs" ) GDALTermProgress_nocb;
%inline %{
int GDALTermProgress_nocb( double dfProgress, const char * pszMessage=NULL, void *pData=NULL ) {
  return GDALTermProgress( dfProgress, pszMessage, pData);
}
%}

%rename (TermProgress) GDALTermProgress;
%callback("%s");
int GDALTermProgress( double, const char *, void * );
%nocallback;

%include "callback.i"

%typemap(in,numinputs=1) (PyArrayObject *psArray)
{
  /* %typemap(in,numinputs=1) (PyArrayObject  *psArray) */
  if ($input != NULL && PyArray_Check($input))
  {
      $1 = (PyArrayObject*)($input);
  }
  else
  {
      PyErr_SetString(PyExc_TypeError, "not a numpy array");
      SWIG_fail;
  }
}

%inline %{
retStringAndCPLFree* GetArrayFilename(PyArrayObject *psArray)
{
    char      szString[128];
    
    GDALRegister_NUMPY();
    
    /* I wish I had a safe way of checking the type */        
    sprintf( szString, "NUMPY:::%p", psArray );
    return CPLStrdup(szString);
}
%}

%feature( "kwargs" ) BandRasterIONumPy;
%inline %{
  CPLErr BandRasterIONumPy( GDALRasterBandShadow* band, int bWrite, int xoff, int yoff, int xsize, int ysize,
                            PyArrayObject *psArray,
                            int buf_type,
                            GDALRIOResampleAlg resample_alg,
                            GDALProgressFunc callback = NULL,
                            void* callback_data = NULL) {

    GDALDataType ntype  = (GDALDataType)buf_type;
    if( psArray->nd < 2 || psArray->nd > 3 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Illegal numpy array rank %d.\n", 
                  psArray->nd );
        return CE_Failure;
    }

    int xdim = ( psArray->nd == 2) ? 1 : 2;
    int ydim = ( psArray->nd == 2) ? 0 : 1;

    int nxsize, nysize, pixel_space, line_space;
    nxsize = psArray->dimensions[xdim];
    nysize = psArray->dimensions[ydim];
    pixel_space = psArray->strides[xdim];
    line_space = psArray->strides[ydim];

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.eResampleAlg = resample_alg;
    sExtraArg.pfnProgress = callback;
    sExtraArg.pProgressData = callback_data;

    return  GDALRasterIOEx( band, (bWrite) ? GF_Write : GF_Read, xoff, yoff, xsize, ysize,
                          psArray->data, nxsize, nysize,
                          ntype, pixel_space, line_space, &sExtraArg );
  }
%}

%typemap(in,numinputs=0) (CPLVirtualMemShadow** pvirtualmem, int numpytypemap) (CPLVirtualMemShadow* virtualmem)
{
  $1 = &virtualmem;
  $2 = 0;
}

%typemap(argout) (CPLVirtualMemShadow** pvirtualmem, int numpytypemap)
{
    CPLVirtualMemShadow* virtualmem = *($1);
    void* ptr = CPLVirtualMemGetAddr( virtualmem->vmem );
    /*size_t nsize = CPLVirtualMemGetSize( virtualmem->vmem );*/
    GDALDataType datatype = virtualmem->eBufType;
    int readonly = virtualmem->bReadOnly;
    GIntBig nBufXSize = virtualmem->nBufXSize;
    GIntBig nBufYSize = virtualmem->nBufYSize;
    int nBandCount = virtualmem->nBandCount;
    int bIsBandSequential = virtualmem->bIsBandSequential;
    GDALTileOrganization eTileOrganization = virtualmem->eTileOrganization;
    int nTileXSize = virtualmem->nTileXSize;
    int nTileYSize = virtualmem->nTileYSize;
    int bAuto = virtualmem->bAuto;
    int            nPixelSpace = virtualmem->nPixelSpace; /* if bAuto == TRUE */
    GIntBig        nLineSpace = virtualmem->nLineSpace; /* if bAuto == TRUE */
    int numpytype;

    if( datatype == GDT_CInt16 || datatype == GDT_CInt32 )
    {
        PyErr_SetString( PyExc_RuntimeError, "GDT_CInt16 and GDT_CInt32 not supported for now" );
        SWIG_fail;
    }

    switch(datatype)
    {
        case GDT_Byte: numpytype = NPY_UBYTE; break;
        case GDT_Int16: numpytype = NPY_INT16; break;
        case GDT_UInt16: numpytype = NPY_UINT16; break;
        case GDT_Int32: numpytype = NPY_INT32; break;
        case GDT_UInt32: numpytype = NPY_UINT32; break;
        case GDT_Float32: numpytype = NPY_FLOAT32; break;
        case GDT_Float64: numpytype = NPY_FLOAT64; break;
        //case GDT_CInt16: numpytype = NPY_INT16; break;
        //case GDT_CInt32: numpytype = NPY_INT32; break;
        case GDT_CFloat32: numpytype = NPY_CFLOAT; break;
        case GDT_CFloat64: numpytype = NPY_CDOUBLE; break;
        default: numpytype = NPY_UBYTE; break;
    }
    PyArrayObject* ar;
    int flags = (readonly) ? 0x1 : 0x1 | 0x0400;
    int nDataTypeSize = GDALGetDataTypeSize(datatype) / 8;
    if( bAuto )
    {
        if( nBandCount == 1 )
        {
            npy_intp shape[2], stride[2];
            shape[0] = nBufYSize;
            shape[1] = nBufXSize;
            stride[1] = nPixelSpace;
            stride[0] = nLineSpace;
            ar = (PyArrayObject*) PyArray_New(&PyArray_Type, 2, shape,
                    numpytype, stride, ptr, 0, flags , NULL);
        }
        else
        {
            PyErr_SetString( PyExc_RuntimeError, "Code update needed for bAuto and nBandCount > 1 !" );
            SWIG_fail;
        }
    }
    else if( bIsBandSequential >= 0 )
    {
        if( nBandCount == 1 )
        {
            npy_intp shape[2], stride[2];
            shape[0] = nBufYSize;
            shape[1] = nBufXSize;
            stride[1] = nDataTypeSize;
            stride[0] = stride[1] * nBufXSize;
            ar = (PyArrayObject*) PyArray_New(&PyArray_Type, 2, shape,
                    numpytype, stride, ptr, 0, flags , NULL);
        }
        else
        {
            npy_intp shape[3], stride[3];
            if( bIsBandSequential )
            {
                shape[0] = nBandCount;
                shape[1] = nBufYSize;
                shape[2] = nBufXSize;
                stride[2] = nDataTypeSize;
                stride[1] = stride[2] * nBufXSize;
                stride[0] = stride[1] * nBufYSize;
            }
            else
            {
                shape[0] = nBufYSize;
                shape[1] = nBufXSize;
                shape[2] = nBandCount;
                stride[2] = nDataTypeSize;
                stride[1] = stride[2] * nBandCount;
                stride[0] = stride[1] * nBufXSize;
            }
            ar = (PyArrayObject*) PyArray_New(&PyArray_Type, 3, shape,
                    numpytype, stride, ptr, 0, flags , NULL);
        }
    }
    else
    {
        int nTilesPerRow = (nBufXSize + nTileXSize - 1) / nTileXSize;
        int nTilesPerCol = (nBufYSize + nTileYSize - 1) / nTileYSize;
        npy_intp shape[5], stride[5];
        if( nBandCount == 1 )
        {
            shape[0] = nTilesPerCol;
            shape[1] = nTilesPerRow;
            shape[2] = nTileYSize;
            shape[3] = nTileXSize;
            stride[3] = nDataTypeSize;
            stride[2] = stride[3] * nTileXSize;
            stride[1] = stride[2] * nTileYSize;
            stride[0] = stride[1] * nTilesPerRow;
            ar = (PyArrayObject*) PyArray_New(&PyArray_Type, 4, shape,
                    numpytype, stride, ptr, 0, flags , NULL);
        }
        else if( eTileOrganization == GTO_TIP )
        {
            shape[0] = nTilesPerCol;
            shape[1] = nTilesPerRow;
            shape[2] = nTileYSize;
            shape[3] = nTileXSize;
            shape[4] = nBandCount;
            stride[4] = nDataTypeSize;
            stride[3] = stride[4] * nBandCount;
            stride[2] = stride[3] * nTileXSize;
            stride[1] = stride[2] * nTileYSize;
            stride[0] = stride[1] * nTilesPerRow;
            ar = (PyArrayObject*) PyArray_New(&PyArray_Type, 5, shape,
                    numpytype, stride, ptr, 0, flags , NULL);
        }
        else if( eTileOrganization == GTO_BIT )
        {
            shape[0] = nTilesPerCol;
            shape[1] = nTilesPerRow;
            shape[2] = nBandCount;
            shape[3] = nTileYSize;
            shape[4] = nTileXSize;
            stride[4] = nDataTypeSize;
            stride[3] = stride[4] * nTileXSize;
            stride[2] = stride[3] * nTileYSize;
            stride[1] = stride[2] * nBandCount;
            stride[0] = stride[1] * nTilesPerRow;
            ar = (PyArrayObject*) PyArray_New(&PyArray_Type, 5, shape,
                    numpytype, stride, ptr, 0, flags , NULL);
        }
        else /* GTO_BSQ */
        {
            shape[0] = nBandCount;
            shape[1] = nTilesPerCol;
            shape[2] = nTilesPerRow;
            shape[3] = nTileYSize;
            shape[4] = nTileXSize;
            stride[4] = nDataTypeSize;
            stride[3] = stride[4] * nTileXSize;
            stride[2] = stride[3] * nTileYSize;
            stride[1] = stride[2] * nTilesPerRow;
            stride[0] = stride[1] * nTilesPerCol;
            ar = (PyArrayObject*) PyArray_New(&PyArray_Type, 5, shape,
                    numpytype, stride, ptr, 0, flags , NULL);
        }
    }

    /* Keep a reference to the VirtualMem object */
    ar->base = obj0;
    Py_INCREF(obj0);
    $result = (PyObject*) ar;
}

%apply Pointer NONNULL {CPLVirtualMemShadow* virtualmem};
%inline %{
    void VirtualMemGetArray(CPLVirtualMemShadow* virtualmem, CPLVirtualMemShadow** pvirtualmem, int numpytypemap)
    {
        *pvirtualmem = virtualmem;
    }
%}
%clear CPLVirtualMemShadow* virtualmem;

%feature( "kwargs" ) RATValuesIONumPyWrite;
%inline %{
  // need different functions for read and write
  // since reading strings requires us to know the 
  // length of the longest string before creating array
  CPLErr RATValuesIONumPyWrite( GDALRasterAttributeTableShadow* poRAT, int nField, int nStart, 
                       PyArrayObject *psArray) {

    if( PyArray_NDIM(psArray) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Illegal numpy array rank %d.\n", 
                  PyArray_NDIM(psArray) );
        return CE_Failure;
    }

    int nLength = PyArray_DIM(psArray, 0);
    int nType = PyArray_TYPE(psArray);
    CPLErr retval = CE_None;

    if( nType == NPY_INT32 )
    {
        retval = GDALRATValuesIOAsInteger(poRAT, GF_Write, nField, nStart, nLength, 
                        (int*)PyArray_DATA(psArray) );
    }
    else if( nType == NPY_DOUBLE )
    {
        retval = GDALRATValuesIOAsDouble(poRAT, GF_Write, nField, nStart, nLength,
                        (double*)PyArray_DATA(psArray) );
    }
    else if( nType == NPY_STRING )
    {
        // have to convert array of strings to a char **
        char **papszStringData = (char**)CPLCalloc(sizeof(char*), nLength);

        // max size of string
        int nMaxLen = PyArray_ITEMSIZE(psArray);
        char *pszBuffer = (char*)CPLMalloc((nMaxLen+1) * sizeof(char));
        // make sure there is a null char on the end
        // as there won't be if this string is the maximum size
        pszBuffer[nMaxLen] = '\0';

        // we can't just use the memory location in the array 
        // since long strings won't be null terminated
        for( int i = 0; i < nLength; i++ )
        {
            strncpy(pszBuffer, (char*)PyArray_GETPTR1(psArray, i), nMaxLen);
            papszStringData[i] = CPLStrdup(pszBuffer);
        }
        CPLFree(pszBuffer);

        retval = GDALRATValuesIOAsString(poRAT, GF_Write, nField, nStart, nLength,
                                            papszStringData);

        for( int i = 0; i < nLength; i++ )
        {
            CPLFree(papszStringData[i]);
        }
        CPLFree(papszStringData);
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Illegal numpy array type %d.\n", 
                  nType );
        return CE_Failure;
    }
    return retval;
  }
%}

%feature( "kwargs" ) RATValuesIONumPyRead;
%inline %{
  // need different functions for read and write
  // since reading strings requires us to know the 
  // length of the longest string before creating array
  PyObject *RATValuesIONumPyRead( GDALRasterAttributeTableShadow* poRAT, int nField, int nStart, 
                       int nLength) {

    GDALRATFieldType colType = GDALRATGetTypeOfCol(poRAT, nField);
    npy_intp dims = nLength;
    PyObject *pOutArray = NULL;
    if( colType == GFT_Integer )
    {
        pOutArray = PyArray_SimpleNew(1, &dims, NPY_INT32);
        if( GDALRATValuesIOAsInteger(poRAT, GF_Read, nField, nStart, nLength, 
                        (int*)PyArray_DATA(pOutArray)) != CE_None)
        {
            Py_DECREF(pOutArray);
            Py_RETURN_NONE;
        }
    }
    else if( colType == GFT_Real )
    {
        pOutArray = PyArray_SimpleNew(1, &dims, NPY_DOUBLE);
        if( GDALRATValuesIOAsDouble(poRAT, GF_Read, nField, nStart, nLength,
                        (double*)PyArray_DATA(pOutArray)) != CE_None)
        {
            Py_DECREF(pOutArray);
            Py_RETURN_NONE;
        }
    }
    else if( colType == GFT_String )
    {
        // must read the data first to work out max size
        // of strings to create array
        int n;
        char **papszStringList = (char**)CPLCalloc(sizeof(char*), nLength);
        if( GDALRATValuesIOAsString(poRAT, GF_Read, nField, nStart, nLength, papszStringList) != CE_None )
        {
            CPLFree(papszStringList);
            Py_RETURN_NONE;
        }
        int nMaxLen = 0, nLen;
        for( n = 0; n < nLength; n++ )
        {
            // note strlen doesn't include null char
            // but that is what numpy expects so all good
            nLen = strlen(papszStringList[n]);
            if( nLen > nMaxLen )
                nMaxLen = nLen;
        }
        int bZeroLength = FALSE;
        // numpy can't deal with zero length strings
        if( nMaxLen == 0 )
        {
            nMaxLen = 1;
            bZeroLength = TRUE;
        }

        // create the dtype string
#if PY_VERSION_HEX >= 0x03000000
        PyObject *pDTypeString = PyUnicode_FromFormat("S%d", nMaxLen);
#else
        PyObject *pDTypeString = PyString_FromFormat("S%d", nMaxLen);
#endif
        // out type description object
        PyArray_Descr *pDescr;
        PyArray_DescrConverter(pDTypeString, &pDescr);
        Py_DECREF(pDTypeString);

        // create array
        pOutArray = PyArray_SimpleNewFromDescr(1, &dims, pDescr);

        // copy data in
        if( !bZeroLength )
        {
            for( n = 0; n < nLength; n++ )
            {
                // we use strncpy so that we don't go over nMaxLen
                // which we would if the null char is copied
                // (which we don't want as numpy 'knows' to interpret the string as nMaxLen long)
                strncpy((char*)PyArray_GETPTR1(pOutArray, n), papszStringList[n], nMaxLen);
            }
        }
        else
        {
            // so there isn't rubbush in the 1 char strings
            PyArray_FILLWBYTE(pOutArray, 0);
        }

        // free strings
        for( n = 0; n < nLength; n++ )
        {
            CPLFree(papszStringList[n]);
        }
        CPLFree(papszStringList);
    }
    return pOutArray;
  }
%}

%pythoncode %{
import numpy
import _gdal_array

import gdalconst
import gdal
gdal.AllRegister()

codes = {   gdalconst.GDT_Byte      :   numpy.uint8,
            gdalconst.GDT_UInt16    :   numpy.uint16,
            gdalconst.GDT_Int16     :   numpy.int16,
            gdalconst.GDT_UInt32    :   numpy.uint32,
            gdalconst.GDT_Int32     :   numpy.int32,
            gdalconst.GDT_Float32   :   numpy.float32,
            gdalconst.GDT_Float64   :   numpy.float64,
            gdalconst.GDT_CInt16    :   numpy.complex64,
            gdalconst.GDT_CInt32    :   numpy.complex64,
            gdalconst.GDT_CFloat32  :   numpy.complex64,
            gdalconst.GDT_CFloat64  :   numpy.complex128
        }

def OpenArray( array, prototype_ds = None ):

    ds = gdal.Open( GetArrayFilename(array) )

    if ds is not None and prototype_ds is not None:
        if type(prototype_ds).__name__ == 'str':
            prototype_ds = gdal.Open( prototype_ds )
        if prototype_ds is not None:
            CopyDatasetInfo( prototype_ds, ds )
            
    return ds
    
    
def flip_code(code):
    if isinstance(code, (numpy.dtype,type)):
        # since several things map to complex64 we must carefully select
        # the opposite that is an exact match (ticket 1518)
        if code == numpy.int8:
            return gdalconst.GDT_Byte
        if code == numpy.complex64:
            return gdalconst.GDT_CFloat32
        
        for key, value in codes.items():
            if value == code:
                return key
        return None
    else:
        try:
            return codes[code]
        except KeyError:
            return None

def NumericTypeCodeToGDALTypeCode(numeric_type):
    if not isinstance(numeric_type, (numpy.dtype,type)):
        raise TypeError("Input must be a type")
    return flip_code(numeric_type)

def GDALTypeCodeToNumericTypeCode(gdal_code):
    return flip_code(gdal_code)
    
def LoadFile( filename, xoff=0, yoff=0, xsize=None, ysize=None,
              callback=None, callback_data=None ):
    ds = gdal.Open( filename )
    if ds is None:
        raise ValueError("Can't open "+filename+"\n\n"+gdal.GetLastErrorMsg())

    return DatasetReadAsArray( ds, xoff, yoff, xsize, ysize,
                               callback = callback, callback_data = callback_data )

def SaveArray( src_array, filename, format = "GTiff", prototype = None ):
    driver = gdal.GetDriverByName( format )
    if driver is None:
        raise ValueError("Can't find driver "+format)

    return driver.CreateCopy( filename, OpenArray(src_array,prototype) )


class ScaledCallbackData:
    def __init__(self, min,max,callback,callback_data):
        self.min = min
        self.max = max
        self.callback = callback
        self.callback_data = callback_data

def ScaledCallback(pct, msg, scaled_callbackdata):
    if scaled_callbackdata.callback == None:
        return 1
    scaled_pct = scaled_callbackdata.min + pct * (scaled_callbackdata.max - scaled_callbackdata.min)
    return scaled_callbackdata.callback(scaled_pct, msg, scaled_callbackdata.callback_data)

def DatasetReadAsArray( ds, xoff=0, yoff=0, xsize=None, ysize=None, buf_obj=None,
                        resample_alg = gdal.GRIORA_NearestNeighbour,
                        callback=None, callback_data=None ):

    if xsize is None:
        xsize = ds.RasterXSize
    if ysize is None:
        ysize = ds.RasterYSize

    if ds.RasterCount == 1:
        return BandReadAsArray( ds.GetRasterBand(1), xoff, yoff, xsize, ysize, buf_obj = buf_obj,
                                resample_alg = resample_alg,
                                callback = callback,
                                callback_data = callback_data )

    datatype = ds.GetRasterBand(1).DataType
    for band_index in range(2,ds.RasterCount+1):
        if datatype != ds.GetRasterBand(band_index).DataType:
            datatype = gdalconst.GDT_Float32
    
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = gdalconst.GDT_Float32
        typecode = numpy.float32

    if buf_obj is not None:
        for band_index in range(1,ds.RasterCount+1):
            if callback:
                scaled_callback = ScaledCallback
                scaled_callbackdata = ScaledCallbackData(1.0 * (band_index-1) / ds.RasterCount,
                                                        1.0 * band_index / ds.RasterCount,
                                                        callback, callback_data)
            else:
                scaled_callback = None
                scaled_callbackdata = None

            BandReadAsArray( ds.GetRasterBand(band_index),
                             xoff, yoff, xsize, ysize, buf_obj = buf_obj[band_index-1],
                             resample_alg = resample_alg,
                             callback = scaled_callback,
                             callback_data = scaled_callbackdata)
        return buf_obj
    
    array_list = []
    for band_index in range(1,ds.RasterCount+1):
        if callback:
            scaled_callback = ScaledCallback
            scaled_callbackdata = ScaledCallbackData(1.0 * (band_index-1) / ds.RasterCount,
                                                    1.0 * band_index / ds.RasterCount,
                                                    callback, callback_data)
        else:
            scaled_callback = None
            scaled_callbackdata = None
        band_array = BandReadAsArray( ds.GetRasterBand(band_index),
                                      xoff, yoff, xsize, ysize,
                                      resample_alg = resample_alg,
                                      callback = scaled_callback,
                                      callback_data = scaled_callbackdata)
        array_list.append( numpy.reshape( band_array, [1,ysize,xsize] ) )

    return numpy.concatenate( array_list )
            
def BandReadAsArray( band, xoff = 0, yoff = 0, win_xsize = None, win_ysize = None,
                     buf_xsize=None, buf_ysize=None, buf_obj=None,
                     resample_alg = gdal.GRIORA_NearestNeighbour,
                     callback=None, callback_data=None):
    """Pure python implementation of reading a chunk of a GDAL file
    into a numpy array.  Used by the gdal.Band.ReadAsArray method."""

    if win_xsize is None:
        win_xsize = band.XSize
    if win_ysize is None:
        win_ysize = band.YSize

    if buf_obj is None:
        if buf_xsize is None:
            buf_xsize = win_xsize
        if buf_ysize is None:
            buf_ysize = win_ysize
    else:
        if len(buf_obj.shape) == 2:
            shape_buf_xsize = buf_obj.shape[1]
            shape_buf_ysize = buf_obj.shape[0]
        else:
            shape_buf_xsize = buf_obj.shape[2]
            shape_buf_ysize = buf_obj.shape[1]
        if buf_xsize is not None and buf_xsize != shape_buf_xsize:
            raise ValueError('Specified buf_xsize not consistant with array shape')
        if buf_ysize is not None and buf_ysize != shape_buf_ysize:
            raise ValueError('Specified buf_ysize not consistant with array shape')
        buf_xsize = shape_buf_xsize
        buf_ysize = shape_buf_ysize

    if buf_obj is None:
        datatype = band.DataType
        typecode = GDALTypeCodeToNumericTypeCode( datatype )
        if typecode == None:
            datatype = gdalconst.GDT_Float32
            typecode = numpy.float32
        else:
            datatype = NumericTypeCodeToGDALTypeCode( typecode )

        if datatype == gdalconst.GDT_Byte and band.GetMetadataItem('PIXELTYPE', 'IMAGE_STRUCTURE') == 'SIGNEDBYTE':
            typecode = numpy.int8
        ar = numpy.empty([buf_ysize,buf_xsize], dtype = typecode)
        if BandRasterIONumPy( band, 0, xoff, yoff, win_xsize, win_ysize,
                                ar, datatype, resample_alg, callback, callback_data ) != 0:
            return None

        return ar
    else:
        datatype = NumericTypeCodeToGDALTypeCode( buf_obj.dtype.type )
        if not datatype:
            raise ValueError("array does not have corresponding GDAL data type")

        if BandRasterIONumPy( band, 0, xoff, yoff, win_xsize, win_ysize,
                                buf_obj, datatype, resample_alg, callback, callback_data ) != 0:
            return None

        return buf_obj

def BandWriteArray( band, array, xoff=0, yoff=0,
                    resample_alg = gdal.GRIORA_NearestNeighbour,
                    callback=None, callback_data=None ):
    """Pure python implementation of writing a chunk of a GDAL file
    from a numpy array.  Used by the gdal.Band.WriteArray method."""

    if array is None or len(array.shape) != 2:
        raise ValueError("expected array of dim 2")

    xsize = array.shape[1]
    ysize = array.shape[0]

    if xsize + xoff > band.XSize or ysize + yoff > band.YSize:
        raise ValueError("array larger than output file, or offset off edge")

    datatype = NumericTypeCodeToGDALTypeCode( array.dtype.type )

    # if we receive some odd type, like int64, try casting to a very
    # generic type we do support (#2285)
    if not datatype:
        gdal.Debug( 'gdal_array', 'force array to float64' )
        array = array.astype( numpy.float64 )
        datatype = NumericTypeCodeToGDALTypeCode( array.dtype.type )
        
    if not datatype:
        raise ValueError("array does not have corresponding GDAL data type")

    return BandRasterIONumPy( band, 1, xoff, yoff, xsize, ysize,
                                array, datatype, resample_alg, callback, callback_data )

def RATWriteArray(rat, array, field, start=0):
    """
    Pure Python implementation of writing a chunk of the RAT
    from a numpy array. Type of array is coerced to one of the types
    (int, double, string) supported. Called from RasterAttributeTable.WriteArray
    """
    if array is None:
        raise ValueError("Expected array of dim 1")

    # if not the array type convert it to handle lists etc
    if not isinstance(array, numpy.ndarray):
        array = numpy.array(array)

    if array.ndim != 1:
        raise ValueError("Expected array of dim 1")

    if (start + array.size) > rat.GetRowCount():
        raise ValueError("Array too big to fit into RAT from start position")

    if numpy.issubdtype(array.dtype, numpy.integer):
        # is some type of integer - coerce to standard int
        # TODO: must check this is fine on all platforms
        # confusingly numpy.int 64 bit even if native type 32 bit
        array = array.astype(numpy.int32)
    elif numpy.issubdtype(array.dtype, numpy.floating):
        # is some type of floating point - coerce to double
        array = array.astype(numpy.double)
    elif numpy.issubdtype(array.dtype, numpy.character):
        # cast away any kind of Unicode etc
        array = array.astype(numpy.character)
    else:
        raise ValueError("Array not of a supported type (integer, double or string)")

    return RATValuesIONumPyWrite(rat, field, start, array)

def RATReadArray(rat, field, start=0, length=None):
    """
    Pure Python implementation of reading a chunk of the RAT
    into a numpy array. Called from RasterAttributeTable.ReadAsArray
    """
    if length is None:
        length = rat.GetRowCount() - start

    return RATValuesIONumPyRead(rat, field, start, length)
    
def CopyDatasetInfo( src, dst, xoff=0, yoff=0 ):
    """
    Copy georeferencing information and metadata from one dataset to another.
    src: input dataset
    dst: output dataset - It can be a ROI - 
    xoff, yoff:  dst's offset with respect to src in pixel/line.  
    
    Notes: Destination dataset must have update access.  Certain formats
           do not support creation of geotransforms and/or gcps.

    """

    dst.SetMetadata( src.GetMetadata() )
                    


    #Check for geo transform
    gt = src.GetGeoTransform()
    if gt != (0,1,0,0,0,1):
        dst.SetProjection( src.GetProjectionRef() )
        
        if (xoff == 0) and (yoff == 0):
            dst.SetGeoTransform( gt  )
        else:
            ngt = [gt[0],gt[1],gt[2],gt[3],gt[4],gt[5]]
            ngt[0] = gt[0] + xoff*gt[1] + yoff*gt[2];
            ngt[3] = gt[3] + xoff*gt[4] + yoff*gt[5];
            dst.SetGeoTransform( ( ngt[0], ngt[1], ngt[2], ngt[3], ngt[4], ngt[5] ) )
            
    #Check for GCPs
    elif src.GetGCPCount() > 0:
        
        if (xoff == 0) and (yoff == 0):
            dst.SetGCPs( src.GetGCPs(), src.GetGCPProjection() )
        else:
            gcps = src.GetGCPs()
            #Shift gcps
            new_gcps = []
            for gcp in gcps:
                ngcp = gdal.GCP()
                ngcp.GCPX = gcp.GCPX 
                ngcp.GCPY = gcp.GCPY
                ngcp.GCPZ = gcp.GCPZ
                ngcp.GCPPixel = gcp.GCPPixel - xoff
                ngcp.GCPLine = gcp.GCPLine - yoff
                ngcp.Info = gcp.Info
                ngcp.Id = gcp.Id
                new_gcps.append(ngcp)

            try:
                dst.SetGCPs( new_gcps , src.GetGCPProjection() )
            except:
                print ("Failed to set GCPs")
                return

    return
%}
