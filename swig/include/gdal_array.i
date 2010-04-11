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

%import typemaps_python.i

%import MajorObject.i
%import Band.i

%init %{
  import_array();
  GDALRegister_NUMPY();
%}

typedef int CPLErr;

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
#else
typedef void GDALRasterBandShadow;
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
        || poOpenInfo->fp != NULL )
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

%apply ( int *optional_int ) {(int*)};
%feature( "kwargs" ) BandRasterIONumPy;
%inline %{
  CPLErr BandRasterIONumPy( GDALRasterBandShadow* band, int bWrite, int xoff, int yoff, int xsize, int ysize,
                     PyArrayObject *psArray,
                     int *buf_xsize = 0,
                     int *buf_ysize = 0,
                     int *buf_type = 0,
                     int *buf_pixel_space = 0,
                     int *buf_line_space = 0) {

    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(band)
                                        : (GDALDataType)*buf_type;
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
    if (buf_xsize == 0)
        nxsize = psArray->dimensions[xdim];
    else
        nxsize = *buf_xsize;
        
    if (buf_ysize == 0)
        nysize = psArray->dimensions[ydim];
    else
        nysize = *buf_ysize;
        
    if (buf_pixel_space == 0)
        pixel_space = psArray->strides[xdim];
    else
        pixel_space = *buf_pixel_space;
        
    if (buf_line_space == 0)
        line_space = psArray->strides[ydim];
    else
        line_space = *buf_line_space;
    
    int min_buf_size = ComputeBandRasterIOSize (nxsize, nysize, GDALGetDataTypeSize( ntype ) / 8,
                                                pixel_space, line_space, FALSE );
    if (min_buf_size == 0)
        return CE_Failure;
        
    if (PyArray_NBYTES(psArray) < min_buf_size)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Buffer too small");
        return CE_Failure;
    }

    return  GDALRasterIO( band, (bWrite) ? GF_Write : GF_Read, xoff, yoff, xsize, ysize,
                          psArray->data, nxsize, nysize,
                          ntype, pixel_space, line_space );
  }
%}
%clear (int*);


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
    if isinstance(code, type):
        # since several things map to complex64 we must carefully select
        # the opposite that is an exact match (ticket 1518)
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
    if not isinstance(numeric_type, type):
        raise TypeError("Input must be a type")
    return flip_code(numeric_type)

def GDALTypeCodeToNumericTypeCode(gdal_code):
    return flip_code(gdal_code)
    
def LoadFile( filename, xoff=0, yoff=0, xsize=None, ysize=None ):
    ds = gdal.Open( filename )
    if ds is None:
        raise ValueError("Can't open "+filename+"\n\n"+gdal.GetLastErrorMsg())

    return DatasetReadAsArray( ds, xoff, yoff, xsize, ysize )

def SaveArray( src_array, filename, format = "GTiff", prototype = None ):
    driver = gdal.GetDriverByName( format )
    if driver is None:
        raise ValueError("Can't find driver "+format)

    return driver.CreateCopy( filename, OpenArray(src_array,prototype) )

def DatasetReadAsArray( ds, xoff=0, yoff=0, xsize=None, ysize=None, buf_obj=None ):

    if xsize is None:
        xsize = ds.RasterXSize
    if ysize is None:
        ysize = ds.RasterYSize

    if ds.RasterCount == 1:
        return BandReadAsArray( ds.GetRasterBand(1), xoff, yoff, xsize, ysize, buf_obj = buf_obj)

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
            BandReadAsArray( ds.GetRasterBand(band_index),
                             xoff, yoff, xsize, ysize, buf_obj = buf_obj[band_index-1])
        return buf_obj
    
    array_list = []
    for band_index in range(1,ds.RasterCount+1):
        band_array = BandReadAsArray( ds.GetRasterBand(band_index),
                                      xoff, yoff, xsize, ysize)
        array_list.append( numpy.reshape( band_array, [1,ysize,xsize] ) )

    return numpy.concatenate( array_list )
            
def BandReadAsArray( band, xoff = 0, yoff = 0, win_xsize = None, win_ysize = None,
                     buf_xsize=None, buf_ysize=None, buf_obj=None ):
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
            if buf_xsize is None:
                buf_xsize = buf_obj.shape[1]
            if buf_ysize is None:
                buf_ysize = buf_obj.shape[0]
        else:
            if buf_xsize is None:
                buf_xsize = buf_obj.shape[2]
            if buf_ysize is None:
                buf_ysize = buf_obj.shape[1]
                
    datatype = band.DataType
    typecode = GDALTypeCodeToNumericTypeCode( datatype )
    if typecode == None:
        datatype = gdalconst.GDT_Float32
        typecode = numpy.float32
    else:
        datatype = NumericTypeCodeToGDALTypeCode( typecode )

    if buf_obj is None:
        ar = numpy.empty([buf_ysize,buf_xsize], dtype = typecode)
        if BandRasterIONumPy( band, 0, xoff, yoff, win_xsize, win_ysize,
                                ar, buf_xsize, buf_ysize, datatype ) != 0:
            return None

        return ar
    else:
            
        datatype = NumericTypeCodeToGDALTypeCode( buf_obj.dtype.type )
            
        if BandRasterIONumPy( band, 0, xoff, yoff, win_xsize, win_ysize,
                                buf_obj, buf_xsize, buf_ysize, datatype ) != 0:
            return None

        return buf_obj

def BandWriteArray( band, array, xoff=0, yoff=0 ):
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
                                array, xsize, ysize, datatype )

    
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
