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
 * gdalrasterband.cpp
 *
 * The GDALRasterBand class.
 *
 * Note that the GDALRasterBand class is normally just used as a base class
 * for format specific band classes. 
 * 
 * $Log$
 * Revision 1.2  1998/12/06 02:52:08  warmerda
 * Added new methods, and C cover functions.
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/*                           GDALRasterBand()                           */
/************************************************************************/

GDALRasterBand::GDALRasterBand()

{
    poDS = NULL;
    nBand = 0;

    eAccess = GA_ReadOnly;
    nBlockXSize = nBlockYSize = 0;
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                          ~GDALRasterBand()                           */
/************************************************************************/

GDALRasterBand::~GDALRasterBand()

{
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr GDALRasterBand::RasterIO( GDALRWFlag /* eRWFlag */,
                                 int /* nXOff */, int /* nYOff */,
                                 int /* nXSize */, int /* nYSize */,
                                 void * /* pData */,
                                 int /* nBufXSize */, int /* nBufYSize */,
                                 GDALDataType /* eBufType */,
                                 int /* nPixelSpace */,
                                 int /* nLineSpace */ )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "RasterIO() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                           GDALReadBlock()                            */
/************************************************************************/

CPLErr GDALReadBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                      void * pData )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

    return( poBand->ReadBlock( nXOff, nYOff, pData ) );
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

CPLErr GDALRasterBand::WriteBlock( int /* nXBlockOff */, int /* nYBlockOff */,
                                   void * /* pImage */ )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "WriteBlock() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                           GDALWriteBlock()                           */
/************************************************************************/

CPLErr GDALWriteBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                       void * pData )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

    return( poBand->WriteBlock( nXOff, nYOff, pData ) );
}


/************************************************************************/
/*                         GetRasterDataType()                          */
/************************************************************************/

GDALDataType GDALRasterBand::GetRasterDataType()

{
    return eDataType;
}

/************************************************************************/
/*                       GDALGetRasterDataType()                        */
/************************************************************************/

GDALDataType GDALGetRasterDataType( GDALRasterBandH hBand )

{
    return( ((GDALRasterBand *) hBand)->GetRasterDataType() );
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

void GDALRasterBand::GetBlockSize( int * pnXSize, int *pnYSize )

{
    if( pnXSize != NULL )
        *pnXSize = nBlockXSize;
    if( pnYSize != NULL )
        *pnYSize = nBlockYSize;
}

/************************************************************************/
/*                          GDALGetBlockSize()                          */
/************************************************************************/

void GDALGetBlockSize( GDALRasterBandH hBand, int * pnXSize, int * pnYSize )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

    poBand->GetBlockSize( pnXSize, pnYSize );
}
