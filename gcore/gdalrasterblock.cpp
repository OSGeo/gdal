/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * gdalrasterblock.cpp
 *
 * The GDALRasterBlock class.
 *
 * 
 * $Log$
 * Revision 1.1  1998/12/31 18:52:58  warmerda
 * New
 *
 */

#include "gdal_priv.h"

static int nTileAgeTicker = 0;

/************************************************************************/
/*                          GDALRasterBlock()                           */
/************************************************************************/

GDALRasterBlock::GDALRasterBlock( int nXSizeIn, int nYSizeIn,
                                  GDALDataType eTypeIn, void * pDataIn )

{
    nXSize = nXSizeIn;
    nYSize = nYSizeIn;
    eType = eTypeIn;
    pData = pDataIn;
    bDirty = FALSE;
    
    Touch();
}

/************************************************************************/
/*                          ~GDALRasterBlock()                          */
/************************************************************************/

GDALRasterBlock::~GDALRasterBlock()

{
    VSIFree( pData );
}


/************************************************************************/
/*                               Touch()                                */
/************************************************************************/

void GDALRasterBlock::Touch()

{
    nAge = nTileAgeTicker++;
}

/************************************************************************/
/*                            Internalize()                             */
/************************************************************************/

CPLErr GDALRasterBlock::Internalize()

{
    void	*pNewData;
    int		nSizeInBytes;

    nSizeInBytes = (nXSize * nYSize * GDALGetDataTypeSize( eType ) + 7) / 8;

    pNewData = VSIMalloc( nSizeInBytes );
    if( pNewData == NULL )
        return( CE_Failure );

    if( pData != NULL )
        memcpy( pNewData, pData, nSizeInBytes );
    
    pData = pNewData;

    return( CE_None );
}

/************************************************************************/
/*                             MarkDirty()                              */
/************************************************************************/

void GDALRasterBlock::MarkDirty()

{
    bDirty = TRUE;
}


/************************************************************************/
/*                             MarkClean()                              */
/************************************************************************/

void GDALRasterBlock::MarkClean()

{
    bDirty = FALSE;
}


