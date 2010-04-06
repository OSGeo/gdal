/******************************************************************************
 * $Id: pcidskdataset.cpp 17097 2009-05-21 19:59:35Z warmerdam $
 *
 * Project:  PCIDSK Database File
 * Purpose:  External Database access interface implementation (EDBFile).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gdal_priv.h"
#include "pcidsk.h"

CPL_CVSID("$Id: pcidskdataset.cpp 17097 2009-05-21 19:59:35Z warmerdam $");

using namespace PCIDSK;

/************************************************************************/
/* ==================================================================== */
/*                            GDAL_EDBFile                              */
/* ==================================================================== */
/************************************************************************/

class GDAL_EDBFile : public EDBFile
{
    GDALDataset     *poDS;

public:

    GDAL_EDBFile( GDALDataset *poDSIn ) { poDS = poDSIn; }
    ~GDAL_EDBFile() { if( poDS ) Close(); }

    int Close() const;
    int GetWidth() const;
    int GetHeight() const;
    int GetChannels() const;
    int GetBlockWidth(int channel ) const;
    int GetBlockHeight(int channel ) const;
    eChanType GetType(int channel ) const;
    int ReadBlock(int channel,
                  int block_index, void *buffer,
                  int win_xoff, int win_yoff,
                  int win_xsize, int win_ysize );
    int WriteBlock( int channel, int block_index, void *buffer);
};

/************************************************************************/
/*                            GDAL_EDBOpen()                            */
/************************************************************************/

EDBFile *GDAL_EDBOpen( std::string osFilename, std::string osAccess )

{
    GDALDataset *poDS;

    if( osAccess == "r" )
        poDS = (GDALDataset *) GDALOpen( osFilename.c_str(), GA_ReadOnly );
    else
        poDS = (GDALDataset *) GDALOpen( osFilename.c_str(), GA_Update );

    if( poDS == NULL )
        ThrowPCIDSKException( "%s", CPLGetLastErrorMsg() );

    return new GDAL_EDBFile( poDS );
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

int GDAL_EDBFile::Close() const

{
    if( poDS != NULL )
    {
        delete poDS;
        ((GDAL_EDBFile*)this)->poDS = NULL;
    }

    return 1;
}

/************************************************************************/
/*                              GetWidth()                              */
/************************************************************************/

int GDAL_EDBFile::GetWidth() const

{
    return poDS->GetRasterXSize();
}

/************************************************************************/
/*                             GetHeight()                              */
/************************************************************************/

int GDAL_EDBFile::GetHeight() const

{
    return poDS->GetRasterYSize();
}

/************************************************************************/
/*                            GetChannels()                             */
/************************************************************************/

int GDAL_EDBFile::GetChannels() const

{
    return poDS->GetRasterCount();
}

/************************************************************************/
/*                           GetBlockWidth()                            */
/************************************************************************/

int GDAL_EDBFile::GetBlockWidth( int nChannel ) const

{
    int nWidth, nHeight;

    poDS->GetRasterBand(nChannel)->GetBlockSize( &nWidth, &nHeight );

    return nWidth;
}

/************************************************************************/
/*                           GetBlockHeight()                           */
/************************************************************************/

int GDAL_EDBFile::GetBlockHeight( int nChannel ) const

{
    int nWidth, nHeight;

    poDS->GetRasterBand(nChannel)->GetBlockSize( &nWidth, &nHeight );

    return nHeight;
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

eChanType GDAL_EDBFile::GetType( int nChannel ) const
{
    switch( poDS->GetRasterBand(nChannel)->GetRasterDataType() )
    {
      case GDT_Byte:
        return CHN_8U;

      case GDT_Int16:
        return CHN_16S;

      case GDT_UInt16:
        return CHN_16U;

      case GDT_Float32:
        return CHN_32R;

      case GDT_CInt16:
        return CHN_C16S;

      default:
        return CHN_UNKNOWN;
    }
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

int GDAL_EDBFile::ReadBlock( int channel,
                             int block_index, void *buffer,
                             int win_xoff, int win_yoff,
                             int win_xsize, int win_ysize )

{
    GDALRasterBand *poBand = poDS->GetRasterBand(channel);
    int nBlockXSize, nBlockYSize;
    int nBlockX, nBlockY;
    int nWidthInBlocks;
    int nPixelOffset;
    int nLineOffset;

    if( GetType(channel) == CHN_UNKNOWN )
    {
        ThrowPCIDSKException("%s channel type not supported for PCIDSK access.",
                             GDALGetDataTypeName(poBand->GetRasterDataType()) );
    }

    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    nWidthInBlocks = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
    
    nBlockX = block_index % nWidthInBlocks;
    nBlockY = block_index / nWidthInBlocks;

    nPixelOffset = GDALGetDataTypeSize(poBand->GetRasterDataType()) / 8;
    nLineOffset = win_xsize * nPixelOffset;

/* -------------------------------------------------------------------- */
/*      Are we reading a partial block at the edge of the database?     */
/*      If so, ensure we don't read off the database.                   */
/* -------------------------------------------------------------------- */
    if( nBlockX * nBlockXSize + win_xoff + win_xsize > poBand->GetXSize() )
        win_xsize = poBand->GetXSize() - nBlockX * nBlockXSize - win_xoff;

    if( nBlockY * nBlockYSize + win_yoff + win_ysize > poBand->GetYSize() )
        win_ysize = poBand->GetYSize() - nBlockY * nBlockYSize - win_yoff;

    CPLErr eErr = poBand->RasterIO( GF_Read, 
                                    nBlockX * nBlockXSize + win_xoff, 
                                    nBlockY * nBlockYSize + win_yoff,
                                    win_xsize, win_ysize, 
                                    buffer, win_xsize, win_ysize, 
                                    poBand->GetRasterDataType(),
                                    nPixelOffset, nLineOffset );

    if( eErr != CE_None )
    {
        ThrowPCIDSKException( "%s", CPLGetLastErrorMsg() );
    }

    return 1;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int GDAL_EDBFile::WriteBlock( int channel, int block_index, void *buffer)

{
    GDALRasterBand *poBand = poDS->GetRasterBand(channel);
    int nBlockXSize, nBlockYSize;
    int nBlockX, nBlockY;
    int nWinXSize, nWinYSize;
    int nWidthInBlocks;

    if( GetType(channel) == CHN_UNKNOWN )
    {
        ThrowPCIDSKException("%s channel type not supported for PCIDSK access.",
                             GDALGetDataTypeName(poBand->GetRasterDataType()) );
    }

    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    nWidthInBlocks = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
    
    nBlockX = block_index % nWidthInBlocks;
    nBlockY = block_index / nWidthInBlocks;

/* -------------------------------------------------------------------- */
/*      Are we reading a partial block at the edge of the database?     */
/*      If so, ensure we don't read off the database.                   */
/* -------------------------------------------------------------------- */
    if( nBlockX * nBlockXSize + nBlockXSize > poBand->GetXSize() )
        nWinXSize = poBand->GetXSize() - nBlockX * nBlockXSize;
    else
        nWinXSize = nBlockXSize;

    if( nBlockY * nBlockYSize + nBlockYSize > poBand->GetYSize() )
        nWinYSize = poBand->GetYSize() - nBlockY * nBlockYSize;
    else
        nWinYSize = nBlockYSize;

    CPLErr eErr = poBand->RasterIO( GF_Write, 
                                    nBlockX * nBlockXSize, 
                                    nBlockY * nBlockYSize,
                                    nWinXSize, nWinYSize,
                                    buffer, nWinXSize, nWinYSize,
                                    poBand->GetRasterDataType(), 0, 0 );

    if( eErr != CE_None )
    {
        ThrowPCIDSKException( "%s", CPLGetLastErrorMsg() );
    }

    return 1;
}
