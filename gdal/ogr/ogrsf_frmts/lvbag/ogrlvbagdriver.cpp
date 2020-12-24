/******************************************************************************
 *
 * Project:  LV BAG Translator
 * Purpose:  Implements OGRLVBAGDriver.
 * Author:   Laixer B.V., info at laixer dot com
 *
 ******************************************************************************
 * Copyright (c) 2020, Laixer B.V. <info at laixer dot com>
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

#include "ogr_lvbag.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGRLVBAGDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    if( !poOpenInfo->bStatOK )
        return FALSE;
    if( poOpenInfo->bIsDirectory )
        return -1;  // Unsure.
    if( poOpenInfo->fpL == nullptr )
        return FALSE;
 
    CPLString osExt(CPLGetExtension(poOpenInfo->pszFilename));
    if( !EQUAL(osExt, "xml") )
        return FALSE;

    auto pszPtr = reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if( poOpenInfo->nHeaderBytes == 0 || pszPtr[0] != '<' )
        return FALSE;

    // Can't handle mutations just yet
    if( strstr(pszPtr, "http://www.kadaster.nl/schemas/mutatielevering-generiek/1.0") != nullptr )
        return FALSE;

    if( strstr(pszPtr, "http://www.kadaster.nl/schemas/standlevering-generiek/1.0") == nullptr )
        return FALSE;

    // Pin the driver to XSD version 'v20200601'
    if( strstr(pszPtr, "http://www.kadaster.nl/schemas/lvbag/extract-deelbestand-lvc/v20200601") == nullptr )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGRLVBAGDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( !OGRLVBAGDriverIdentify(poOpenInfo) ||
        poOpenInfo->eAccess == GA_Update)
        return nullptr;

    auto poDS = std::unique_ptr<OGRLVBAGDataSource>{
        new OGRLVBAGDataSource{} };
    poDS->SetDescription(poOpenInfo->pszFilename);

    if( !poOpenInfo->bIsDirectory && poOpenInfo->fpL != nullptr )
    {
        if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions ) )
            poDS.reset();
    }
    else if( poOpenInfo->bIsDirectory && poOpenInfo->fpL == nullptr )
    {
        char **papszNames = VSIReadDir(poOpenInfo->pszFilename);
        for( int i = 0; papszNames != nullptr && papszNames[i] != nullptr; ++i )
        {
            const CPLString oSubFilename =
                CPLFormFilename(poOpenInfo->pszFilename, papszNames[i], nullptr);

            if( EQUAL(papszNames[i], ".") || EQUAL(papszNames[i], "..") )
                continue;

            CPLString osExt(CPLGetExtension(oSubFilename));
            if( !EQUAL(osExt, "xml") )
                continue;

            GDALOpenInfo oOpenInfo{ oSubFilename, GA_ReadOnly };
            if( !OGRLVBAGDriverIdentify(&oOpenInfo) )
                continue;

            if( !poDS->Open( oSubFilename, poOpenInfo->papszOpenOptions ) )
                continue;
        }

        CSLDestroy(papszNames);
        if( !poDS->GetLayerCount() )
        {
            poDS.reset();
            return nullptr;
        }
    }
    else
    {
        poDS.reset();
        return nullptr;
    }

    return poDS.release();
}

/************************************************************************/
/*                         RegisterOGRLVBAG()                           */
/************************************************************************/

void RegisterOGRLVBAG()
{
    if( GDALGetDriverByName( "LVBAG" ) != nullptr )
        return;

    std::unique_ptr<GDALDriver> poDriver = std::unique_ptr<GDALDriver>{ new GDALDriver };

    poDriver->SetDescription( "LVBAG" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Kadaster LV BAG Extract 2.0" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xml" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/lvbag.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='AUTOCORRECT_INVALID_DATA' type='boolean' description='whether driver should try to fix invalid data' default='NO'/>"
"  <Option name='LEGACY_ID' type='boolean' description='whether driver should use the BAG 1.0 identifiers' default='NO'/>"
"</OpenOptionList>" );

    poDriver->pfnOpen = OGRLVBAGDriverOpen;
    poDriver->pfnIdentify = OGRLVBAGDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
