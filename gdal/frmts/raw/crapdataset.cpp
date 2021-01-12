/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  CRAP format converter
 * Author:   crap-designer
 *
 ******************************************************************************
 * Copyright (c) 2021, crap-designer
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

#include "gdal_priv.h"

extern "C" void GDALRegister_CRAP();

class CRAPDataset: public GDALDataset
{
public:
    CRAPDataset() = default;

    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
};

class CRAPRasterBand: public GDALRasterBand
{
public:
    CRAPRasterBand();

    CPLErr IReadBlock(int, int, void* ) override;
};

CRAPRasterBand::CRAPRasterBand()
{
    nRasterXSize = static_cast<int>(strlen("CRAP data"));
    nRasterYSize = 1;
    eDataType = GDT_Byte;
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
}

CPLErr CRAPRasterBand::IReadBlock(int, int, void* pData )
{
    memcpy(pData, "CRAP data", strlen("CRAP data"));
    return CE_None;
}

GDALDataset* CRAPDataset::Open(GDALOpenInfo* poOpenInfo)
{
    constexpr const char* magic = "This is a crappy format";
    if( poOpenInfo->nHeaderBytes < static_cast<int>(strlen(magic)) ||
        !STARTS_WITH(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                     magic) )
    {
        return nullptr;
    }

    auto poDS = new CRAPDataset();
    auto poBand = new CRAPRasterBand();
    poDS->nRasterXSize = poBand->GetXSize();
    poDS->nRasterYSize = poBand->GetYSize();
    poDS->SetBand(1, poBand);
    return poDS;
}

/************************************************************************/
/*                         GDALRegister_CRAP()                          */
/************************************************************************/

void GDALRegister_CRAP()

{
    if( GDALGetDriverByName( "CRAP" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "CRAP" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Some crappy format someone may perhaps invent someday" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = CRAPDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
