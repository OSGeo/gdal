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
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GTiffBitmapBand::GetColorTable()

{
    if (m_poGDS->m_bPromoteTo8Bits)
        return nullptr;

    return m_poColorTable;
}
