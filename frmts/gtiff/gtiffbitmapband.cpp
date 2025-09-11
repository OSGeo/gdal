/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gtiffbitmapband.h"
#include "gtiffdataset.h"

/************************************************************************/
/*                           GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::GTiffBitmapBand(GTiffDataset *poDSIn, int nBandIn)
    : GTiffOddBitsBand(poDSIn, nBandIn)

{
    eDataType = GDT_Byte;

    if (poDSIn->m_poColorTable != nullptr)
    {
        m_poColorTable = poDSIn->m_poColorTable->Clone();
    }
    else
    {
#ifdef ESRI_BUILD
        m_poColorTable = nullptr;
#else
        const GDALColorEntry oWhite = {255, 255, 255, 255};
        const GDALColorEntry oBlack = {0, 0, 0, 255};

        m_poColorTable = new GDALColorTable();

        if (poDSIn->m_nPhotometric == PHOTOMETRIC_MINISWHITE)
        {
            m_poColorTable->SetColorEntry(0, &oWhite);
            m_poColorTable->SetColorEntry(1, &oBlack);
        }
        else
        {
            m_poColorTable->SetColorEntry(0, &oBlack);
            m_poColorTable->SetColorEntry(1, &oWhite);
        }
#endif  // not defined ESRI_BUILD.
    }
}

/************************************************************************/
/*                          ~GTiffBitmapBand()                          */
/************************************************************************/

GTiffBitmapBand::~GTiffBitmapBand()

{
    delete m_poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GTiffBitmapBand::GetColorInterpretation()

{
    if (m_poGDS->m_bPromoteTo8Bits)
        return GCI_Undefined;

    return GCI_PaletteIndex;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr GTiffBitmapBand::SetColorInterpretation(GDALColorInterp eInterp)
{
    if (eInterp != GetColorInterpretation())
    {
        CPLDebug(
            "GTiff",
            "Setting color interpration on GTiffBitmap band is not supported");
    }
    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffBitmapBand::GetColorTable()

{
    if (m_poGDS->m_bPromoteTo8Bits)
        return nullptr;

    return m_poColorTable;
}
