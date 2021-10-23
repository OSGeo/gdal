/*****************************************************************************
* $Id$
*
* Project:  Intergraph Raster Format support
* Purpose:  Read selected types of Intergraph Raster Format
* Author:   Ivan Lucena, [lucena_ivan at hotmail.com]
*
******************************************************************************
* Copyright (c) 2007, Ivan Lucena
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

class IntergraphRasterBand CPL_NON_FINAL: public GDALPamRasterBand
{
    friend class IntergraphDataset;

protected:
    GDALColorTable *poColorTable;
    uint32_t          nDataOffset;
    uint32_t          nBlockBufSize;
    uint32_t          nBandStart;
    uint8_t           nRGBIndex;

    INGR_Format     eFormat;
    bool            bTiled;
    int             nFullBlocksX;
    int             nFullBlocksY;

    GByte          *pabyBlockBuf;
    uint32_t          nTiles;

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
    virtual ~IntergraphRasterBand();

    virtual double GetMinimum( int *pbSuccess = nullptr ) override;
    virtual double GetMaximum( int *pbSuccess = nullptr ) override;
    virtual GDALColorTable *GetColorTable() override;
    virtual GDALColorInterp GetColorInterpretation() override;
    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
    virtual CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
    virtual CPLErr SetColorTable( GDALColorTable *poColorTable ) override;
    virtual CPLErr SetStatistics( double dfMin, double dfMax, double dfMean, double dfStdDev ) override;

protected:
    int  HandleUninstantiatedTile( int nBlockXOff, int nBlockYOff, void* pImage);
    int  LoadBlockBuf( int nBlockXOff, int nBlockYOff, int nBlockBytes, GByte *pabyBlock ) const;
    bool ReshapeBlock( int nBlockXOff, int nBlockYOff, int nBlockBytes, GByte *pabyBlock );
    void FlushBandHeader();
    void BlackWhiteCT( bool bReverse = false );
};

//  ----------------------------------------------------------------------------
//     Intergraph IntergraphRGBBand
//  ----------------------------------------------------------------------------

class IntergraphRGBBand final: public IntergraphRasterBand
{
public:
    IntergraphRGBBand( IntergraphDataset *poDS,
                       int nBand,
                       int nBandOffset,
                       int nRGorB );

    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
};

//  ----------------------------------------------------------------------------
//     Intergraph IntergraphBitmapBand
//  ----------------------------------------------------------------------------

class IntergraphBitmapBand final: public IntergraphRasterBand
{
    friend class IntergraphDataset;

private:
    GByte          *pabyBMPBlock;
    uint32_t          nBMPSize;
    int             nQuality;
    int             nRGBBand;

public:
    IntergraphBitmapBand( IntergraphDataset *poDS,
                          int nBand,
                          int nBandOffset,
                          int nRGorB = 1 );
    virtual ~IntergraphBitmapBand();

    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
    virtual GDALColorInterp GetColorInterpretation() override;
};

//  ----------------------------------------------------------------------------
//     Intergraph IntergraphRLEBand
//  ----------------------------------------------------------------------------

class IntergraphRLEBand final: public IntergraphRasterBand
{
    friend class IntergraphDataset;

private:
    GByte          *pabyRLEBlock;
    uint32_t          nRLESize;
    int             bRLEBlockLoaded;
    uint32_t         *panRLELineOffset;

public:
    IntergraphRLEBand( IntergraphDataset *poDS,
                       int nBand,
                       int nBandOffset,
                       int nRGorB = 0 );
    virtual ~IntergraphRLEBand();

    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage ) override;
};
