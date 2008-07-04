/******************************************************************************
 * $Id: gdalctexpander.cpp $
 *
 * Project:  GDAL Core
 * Purpose:  Expand a dataset with a paletted band into several bands for each
 *           color component
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "gdalctexpander.h"

/* ******************************************************************** */
/*                    GDALCTExpandedBand                               */
/* ******************************************************************** */

/************************************************************************/
/*                     GDALCTExpandedBand()                            */
/************************************************************************/

GDALCTExpandedBand::GDALCTExpandedBand(GDALRasterBand* poPalettedRasterBand, int nComponent)
{
    this->poPalettedRasterBand = poPalettedRasterBand;

    this->poDS         = poPalettedRasterBand->GetDataset();
    this->nBand        = nComponent;
    this->eDataType    = GDT_Byte;
    this->nRasterXSize = poPalettedRasterBand->GetXSize();
    this->nRasterYSize = poPalettedRasterBand->GetYSize();
    poPalettedRasterBand->GetBlockSize(&nBlockXSize, &nBlockYSize);

    GDALColorTable* poColorTable = poPalettedRasterBand->GetColorTable();
    nColors = poColorTable->GetColorEntryCount();

    pabyLUT = (GByte*) VSIMalloc(nColors);
    for(int i=0;i<nColors;i++)
    {
        const GDALColorEntry* poEntry = poColorTable->GetColorEntry(i);
        if (nComponent == 1)
            pabyLUT[i] = poEntry->c1;
        else if (nComponent == 2)
            pabyLUT[i] = poEntry->c2;
        else if (nComponent == 3)
            pabyLUT[i] = poEntry->c3;
        else
            pabyLUT[i] = poEntry->c4;
    }
}
/************************************************************************/
/*                    ~GDALCTExpandedBand()                            */
/************************************************************************/

GDALCTExpandedBand::~GDALCTExpandedBand()
{
    VSIFree(pabyLUT);
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr GDALCTExpandedBand::IReadBlock( int nXBlockOff, int nYBlockOff, void* pImage )
{
    CPLErr eErr;
    if (poPalettedRasterBand->GetRasterDataType() == GDT_Byte)
    {
        eErr = poPalettedRasterBand->ReadBlock(nXBlockOff, nYBlockOff, pImage);
    }
    else
    {
        eErr = poPalettedRasterBand->RasterIO(GF_Read,
                                             nXBlockOff * nBlockXSize,
                                             nYBlockOff * nBlockYSize,
                                             nBlockXSize,
                                             nBlockYSize,
                                             pImage,
                                             nBlockXSize,
                                             nBlockYSize,
                                             GDT_Byte,
                                             0, 0);
    }
    if (eErr != CE_None)
        return eErr;

    GByte* pabyImage = (GByte*)pImage;
    for(int i = nBlockXSize * nBlockYSize - 1; i>=0; i--)
    {
        if (*pabyImage < nColors)
            *pabyImage = pabyLUT[*pabyImage];
        else
        {
            return CE_Failure;
        }
        pabyImage ++;
    }

    return CE_None;
}

/************************************************************************/
/*                           IWriteBlock()                              */
/************************************************************************/

CPLErr GDALCTExpandedBand::IWriteBlock( int nXBlockOff, int nYBlockOff, void* pImage )
{
    CPLError( CE_Failure, CPLE_NotSupported,
                "WriteBlock() not supported for expanded bands." );

    return( CE_Failure );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALCTExpandedBand::GetColorInterpretation()
{
    GDALColorTable* poColorTable = poPalettedRasterBand->GetColorTable();
    if (poColorTable->GetPaletteInterpretation() == GPI_Gray)
    {
        return GCI_GrayIndex;
    }
    else if (poColorTable->GetPaletteInterpretation() == GPI_RGB)
    {
        if (nBand == 1)
            return GCI_RedBand;
        else if (nBand == 2)
            return GCI_GreenBand;
        else if (nBand == 3)
            return GCI_BlueBand;
        else
            return GCI_AlphaBand;
    }
    else if (poColorTable->GetPaletteInterpretation() == GPI_CMYK)
    {
        if (nBand == 1)
            return GCI_CyanBand;
        else if (nBand == 2)
            return GCI_MagentaBand;
        else if (nBand == 3)
            return GCI_YellowBand;
        else
            return GCI_BlackBand;
    }
    else
    {
        if (nBand == 1)
            return GCI_HueBand;
        else if (nBand == 2)
            return GCI_SaturationBand;
        else
            return GCI_LightnessBand;
    }
}

/* ******************************************************************** */
/*                    GDALCTExpandedDataset                            */
/* ******************************************************************** */

/************************************************************************/
/*                   GDALCTExpandedDataset()                           */
/************************************************************************/

GDALCTExpandedDataset::GDALCTExpandedDataset(GDALDataset* poPalettedDataset, int nBands, int bShared)
{
    CPLString osDescription(poPalettedDataset->GetDescription());

    this->poPalettedDataset = poPalettedDataset;

    osDescription += "_expanded";

    SetDescription(osDescription);

    nRasterXSize = poPalettedDataset->GetRasterXSize();
    nRasterYSize = poPalettedDataset->GetRasterYSize();

    if (bShared)
        MarkAsShared();

    GDALRasterBand* poSrcBand = poPalettedDataset->GetRasterBand(1);
    int i;
    for(i=0;i<nBands;i++)
    {
        SetBand(i+1, new GDALCTExpandedBand(poSrcBand, i+1));
    }
}

/************************************************************************/
/*                  ~GDALCTExpandedDataset()                           */
/************************************************************************/

GDALCTExpandedDataset::~GDALCTExpandedDataset()
{
}

/************************************************************************/
/*                    GetUnderlyingDataset()                            */
/************************************************************************/

GDALDataset* GDALCTExpandedDataset::GetUnderlyingDataset()
{
    return poPalettedDataset;
}

/************************************************************************/
/*                        IBuildOverviews()                             */
/************************************************************************/

CPLErr GDALCTExpandedDataset::IBuildOverviews( const char *pszResampling, 
                                                int nOverviews, int *panOverviewList, 
                                                int nListBands, int *panBandList,
                                                GDALProgressFunc pfnProgress, 
                                                void * pProgressData )
{
    /* Don't proxy */
    return GDALDataset::IBuildOverviews( pszResampling, nOverviews, panOverviewList, 
                                         nListBands, panBandList, pfnProgress, pProgressData );
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr GDALCTExpandedDataset::IRasterIO ( GDALRWFlag eRWFlag,
                                           int nXOff, int nYOff, int nXSize, int nYSize,
                                           void * pData, int nBufXSize, int nBufYSize,
                                           GDALDataType eBufType, 
                                           int nBandCount, int *panBandMap,
                                           int nPixelSpace, int nLineSpace, int nBandSpace)
{
    /* Don't proxy */
    return GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                   pData, nBufXSize, nBufYSize,
                                   eBufType, nBandCount, panBandMap,
                                   nPixelSpace, nLineSpace, nBandSpace );
}

/************************************************************************/
/*                           AdviseRead()                               */
/************************************************************************/


CPLErr GDALCTExpandedDataset::AdviseRead ( int nXOff, int nYOff, int nXSize, int nYSize,
                                            int nBufXSize, int nBufYSize, 
                                            GDALDataType eDT, 
                                            int nBandCount, int *panBandList,
                                            char **papszOptions )
{
    /* Don't proxy */
    return GDALDataset::AdviseRead ( nXOff, nYOff, nXSize, nYSize,
                                     nBufXSize, nBufYSize, eDT,
                                     nBandCount, panBandList, papszOptions );
}


static int GDALCTExpanderCheckBand(GDALRasterBand* poBand, int *pnMax)
{
    GDALColorTable* poColorTable = poBand->GetColorTable();

    if (poColorTable == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Band has no color table");
        return FALSE;
    }

    if (poColorTable->GetColorEntryCount() > 256)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Only palettes with a maximum of 256 entries are supported");
        return FALSE;
    }

    if (pnMax)
    {
        if (poColorTable->GetPaletteInterpretation() == GPI_Gray)
        {
            *pnMax = 1;
        }
        else if (poColorTable->GetPaletteInterpretation() == GPI_HLS)
        {
            *pnMax = 3;
        }
        else
        {
            *pnMax = 4;
        }
    }

    return TRUE;
}

/************************************************************************/
/*               GDALCTExpandedDatasetCreate()                         */
/************************************************************************/


/** Creates a dataset that will expose each components of the color table of
 *  the paletted band as a separate band
 *
 *  This dataset will act mainly as a proxy for the source dataset.
 *
 *  @param hDS : the source dataset, which must have one band with a color table
 *  @param nComponents : the number of bands in the output dataset. The possible values
 *                       are 1 for grey color tables, 1,2 or 3 for HLS, or 1,2,3 or 4 for RGB.
 *  @param bShared : whether the created dataset must have the bShared flag. Usefull when
 *                   embedding the bands of the returned dataset as a source for a VRT dataset
 *
 *  @return the newly created dataset. It must be deleted by GDALClose.
 */

GDALDatasetH GDALCTExpandedDatasetCreate( GDALDatasetH hPalettedDS, int nComponents, int bShared)
{
    VALIDATE_POINTER1( hPalettedDS, "GDALCTExpandedBandCreate", NULL );

    if (GDALGetRasterCount(hPalettedDS) != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Only datasets with 1 band are supported");
        return NULL;
    }

    GDALRasterBand* poBand = (GDALRasterBand*)GDALGetRasterBand(hPalettedDS, 1);
    int nMax;
    if (GDALCTExpanderCheckBand(poBand, &nMax) == FALSE)
    {
        return NULL;
    }

    if (nComponents < 1 || nComponents > nMax)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid nComponents value : %d. Must be between 1 and %d", nComponents, nMax);
        return NULL;
    }


    GDALCTExpandedDataset* poDS = new GDALCTExpandedDataset((GDALDataset*)hPalettedDS, nComponents, bShared);
    return (GDALDatasetH)poDS;
}


/************************************************************************/
/*                  GDALCTExpandedBandCreate()                         */
/************************************************************************/

/** Creates a raster band that will the R, G, B or A component of a paletted rasterband.
 *
 *  @param hPalettedRasterBand : the source band which must have a color table
 *  @param nComponent : the index of the component to return. 1 = R, 2 = G, 3 = B, 4 = A
 *
 *  @return the newly created rasterband. It must be deleted by GDALCTExpandedBandDelete.
 */

GDALRasterBandH GDALCTExpandedBandCreate( GDALRasterBandH hPalettedRasterBand, int nComponent )
{
    VALIDATE_POINTER1( hPalettedRasterBand, "GDALCTExpandedBandCreate", NULL );

    int nMax;
    if (GDALCTExpanderCheckBand((GDALRasterBand*)hPalettedRasterBand, &nMax) == FALSE)
    {
        return FALSE;
    }

    if (nComponent < 1 || nComponent > nMax)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid nComponent value : %d. Must be between 1 and %d", nComponent, nMax);
        return NULL;
    }

    GDALCTExpandedBand* poBand = new GDALCTExpandedBand((GDALRasterBand*)hPalettedRasterBand, nComponent);
    return (GDALRasterBandH)poBand;
}

/************************************************************************/
/*                  GDALCTExpandedBandDelete()                         */
/************************************************************************/

/** Destroys a rasterband created by GDALCTExpandedBandCreate
 *
 *  @param hCTExpandedBand : the band to destroy
 */

void GDALCTExpandedBandDelete( GDALRasterBandH hCTExpandedBand )
{
    VALIDATE_POINTER0( hCTExpandedBand, "GDALCTExpandedBandDelete" );

    delete (GDALCTExpandedBand*) hCTExpandedBand;
}
