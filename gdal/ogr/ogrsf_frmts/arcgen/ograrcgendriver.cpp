/******************************************************************************
 *
 * Project:  Arc/Info Generate Translator
 * Purpose:  Implements OGRARCGENDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMARCGENS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_arcgen.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

extern "C" void RegisterOGRARCGEN();

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRARCGENDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == nullptr )
    {
        return nullptr;
    }

    /* Check that the first line is compatible with a generate file */
    /* and in particular contain >= 32 && <= 127 bytes */
    bool bFoundEOL = false;
    char* szFirstLine
        = CPLStrdup(reinterpret_cast<char *>( poOpenInfo->pabyHeader ) );
    for( int i = 0; szFirstLine[i] != '\0'; i++ )
    {
        if (szFirstLine[i] == '\n' || szFirstLine[i] == '\r')
        {
            bFoundEOL = true;
            szFirstLine[i] = '\0';
            break;
        }
        if (szFirstLine[i] < 32)
        {
            CPLFree(szFirstLine);
            return nullptr;
        }
    }

    if (!bFoundEOL)
    {
        CPLFree(szFirstLine);
        return nullptr;
    }

    char** papszTokens = CSLTokenizeString2( szFirstLine, " ,", 0 );
    const int nTokens = CSLCount(papszTokens);
    if (nTokens != 1 && nTokens != 3 && nTokens != 4)
    {
        CSLDestroy(papszTokens);
        CPLFree(szFirstLine);
        return nullptr;
    }
    for(int i=0;i<nTokens;i++)
    {
        if( CPLGetValueType(papszTokens[i]) == CPL_VALUE_STRING )
        {
            CSLDestroy(papszTokens);
            CPLFree(szFirstLine);
            return nullptr;
        }
    }
    CSLDestroy(papszTokens);
    CPLFree(szFirstLine);

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("ARCGEN") )
        return nullptr;

    OGRARCGENDataSource *poDS = new OGRARCGENDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRARCGEN()                           */
/************************************************************************/

void RegisterOGRARCGEN()

{
    if( GDALGetDriverByName( "ARCGEN" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "ARCGEN" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Arc/Info Generate" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/arcgen.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->pfnOpen = OGRARCGENDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
