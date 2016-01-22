/*****************************************************************************
* $Id$
*
* Project:  Intergraph Raster Format support
* Purpose:  Read selected types of Intergraph Raster Format
* Author:   Ivan Lucena, [lucena_ivan at hotmail.com]
*
******************************************************************************
* Copyright (c) 2007, Ivan Lucena
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files ( the "Software" ),
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

#include "IngrTypes.h"

//  ----------------------------------------------------------------------------
//     Intergraph IntergraphRasterBand
//  ----------------------------------------------------------------------------

class IntergraphRasterBand : public GDALPamRasterBand
{
    friend class IntergraphDataset;

protected:
    GDALColorTable *poColorTable;
    uint32          nDataOffset;
    uint32          nBlockBufSize;
    uint32          nBandStart;
    uint8           nRGBIndex;

    INGR_Format     eFormat;
    bool            bTiled;
    int             nFullBlocksX;
    int             nFullBlocksY;

    GByte	       *pabyBlockBuf;
    uint32          nTiles;

    INGR_TileItem  *pahTiles;

    INGR_HeaderOne  hHeaderOne;
    INGR_HeaderTwoA hHeaderTwo;
    INGR_TileHeader hTileDir;
    
    int             nRLEOffset;

public:
    IntergraphRasterBand( IntergraphDataset *poDS, 
        int nBand,
        int nBandOffset,
        GDALDataType eType = GDT_Unknown);
    ~IntergraphRasterBand();

    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum( int *pbSuccess = NULL );    
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );
    virtual CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void *pImage );
    virtual CPLErr SetColorTable( GDALColorTable *poColorTable ); 
    virtual CPLErr SetStatistics( double dfMin, double dfMax, double dfMean, double dfStdDev );

protected:
    int  HandleUninstantiatedTile( int nBlockXOff, int nBlockYOff, void* pImage);
    int  LoadBlockBuf( int nBlockXOff, int nBlockYOff, int nBlockBytes, GByte *pabyBlock );
    void ReshapeBlock( int nBlockXOff, int nBlockYOff, int nBlockBytes, GByte *pabyBlock );
    void FlushBandHeader( void );
    void BlackWhiteCT( bool bReverse = false );
};

//  ----------------------------------------------------------------------------
//     Intergraph IntergraphRGBBand
//  ----------------------------------------------------------------------------

class IntergraphRGBBand : public IntergraphRasterBand
{
public:
    IntergraphRGBBand( IntergraphDataset *poDS, 
        int nBand,
        int nBandOffset,
        int nRGorB );

    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );
};

//  ----------------------------------------------------------------------------
//     Intergraph IntergraphBitmapBand
//  ----------------------------------------------------------------------------

class IntergraphBitmapBand : public IntergraphRasterBand
{
    friend class IntergraphDataset;

private:
    GByte	       *pabyBMPBlock;
    uint32          nBMPSize;
    int             nQuality;
    int             nRGBBand;

public:
    IntergraphBitmapBand( IntergraphDataset *poDS, 
        int nBand,
        int nBandOffset,
        int nRGorB = 1 );
    ~IntergraphBitmapBand();

    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );
    virtual GDALColorInterp GetColorInterpretation();
};

//  ----------------------------------------------------------------------------
//     Intergraph IntergraphRLEBand
//  ----------------------------------------------------------------------------

class IntergraphRLEBand : public IntergraphRasterBand
{
    friend class IntergraphDataset;

private:
    GByte	       *pabyRLEBlock;
    uint32          nRLESize;
    int             bRLEBlockLoaded;
    uint32         *panRLELineOffset;

public:
    IntergraphRLEBand( IntergraphDataset *poDS, 
        int nBand,
        int nBandOffset,
        int nRGorB = 0);
    ~IntergraphRLEBand();

    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );
};
