/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Python Bindings
 * Purpose:  Implementation of NumPy arrays as a GDALDataset.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2001/02/06 16:16:28  warmerda
 * fixed numpydataset.cpp to use sscanf to parse pointer
 *
 * Revision 1.1  2000/07/20 00:33:37  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "../frmts/mem/memdataset.h"
#include "gdal_py.h"

static GDALDriver	*poNUMPYDriver = NULL;

/************************************************************************/
/*				MEMDataset				*/
/************************************************************************/

class NUMPYDataset : public GDALDataset
{
    PyArrayObject *psArray;

  public:
                 NUMPYDataset();
                 ~NUMPYDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            NUMPYDataset()                            */
/************************************************************************/

NUMPYDataset::NUMPYDataset()

{
}

/************************************************************************/
/*                            ~NUMPYDataset()                            */
/************************************************************************/

NUMPYDataset::~NUMPYDataset()

{
    FlushCache();
    Py_DECREF( psArray );
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
    sscanf( poOpenInfo->pszFilename+8, "%p", &psArray );
    if( psArray == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to parse meaningful pointer value from NUMPY name\n"
                  "string: %s\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

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

    switch( psArray->descr->type )
    {
      case 'D':
        eType = GDT_CFloat64;
        break;

      case 'F':
        eType = GDT_CFloat32;
        break;

      case 'd':
        eType = GDT_Float64;
        break;

      case 'f':
        eType = GDT_Float32;
        break;

      case 'l':
      case 'i':
        eType = GDT_Int32;
        break;

      case 's':
        eType = GDT_Int16;
        break;

      case 'b':
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
    poDS->poDriver = poNUMPYDriver;

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
                       new MEMRasterBand( poDS, iBand+1, 
                                (GByte *) psArray->data + nBandOffset*iBand,
                                          eType, nPixelOffset, nLineOffset,
                                          FALSE ) );
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    return poDS;
}

/************************************************************************/
/*                          GDALRegister_NUMPY()                        */
/************************************************************************/

void GDALRegister_NUMPY()

{
    GDALDriver	*poDriver;

    if( poNUMPYDriver == NULL )
    {
        poNUMPYDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "NUMPY";
        poDriver->pszLongName = "NumPy Array";
        
        poDriver->pfnOpen = NUMPYDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
