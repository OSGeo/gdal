/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Generate Translator
 * Purpose:  Implements OGRARCGENDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

extern "C" void RegisterOGRARCGEN();


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRARCGENDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == NULL )
    {
        return NULL;
    }

    /* Check that the first line is compatible with a generate file */
    /* and in particular contain >= 32 && <= 127 bytes */
    int i;
    int bFoundEOL = FALSE;
    char* szFirstLine = CPLStrdup((const char*) poOpenInfo->pabyHeader);
    for(i=0;szFirstLine[i] != '\0';i++)
    {
        if (szFirstLine[i] == '\n' || szFirstLine[i] == '\r')
        {
            bFoundEOL = TRUE;
            szFirstLine[i] = '\0';
            break;
        }
        if (szFirstLine[i] < 32)
        {
            CPLFree(szFirstLine);
            return NULL;
        }
    }

    if (!bFoundEOL)
    {
        CPLFree(szFirstLine);
        return NULL;
    }

    char** papszTokens = CSLTokenizeString2( szFirstLine, " ,", 0 );
    int nTokens = CSLCount(papszTokens);
    if (nTokens != 1 && nTokens != 3 && nTokens != 4)
    {
        CSLDestroy(papszTokens);
        CPLFree(szFirstLine);
        return NULL;
    }
    for(int i=0;i<nTokens;i++)
    {
        if( CPLGetValueType(papszTokens[i]) == CPL_VALUE_STRING )
        {
            CSLDestroy(papszTokens);
            CPLFree(szFirstLine);
            return NULL;
        }
    }
    CSLDestroy(papszTokens);
    CPLFree(szFirstLine);

    OGRARCGENDataSource   *poDS = new OGRARCGENDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRARCGEN()                           */
/************************************************************************/

void RegisterOGRARCGEN()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "ARCGEN" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "ARCGEN" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Arc/Info Generate" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_arcgen.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRARCGENDriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

