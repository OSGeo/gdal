/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  OGRNASDriver implementation
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_nas.h"
#include "cpl_conv.h"
#include "nasreaderp.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");


/************************************************************************/
/*                       OGRNASDriverUnload()                           */
/************************************************************************/

static void OGRNASDriverUnload(CPL_UNUSED GDALDriver* poDriver)
{
    if( NASReader::hMutex != NULL )
        CPLDestroyMutex( NASReader::hMutex );
    NASReader::hMutex = NULL;
}

/************************************************************************/
/*                     OGRNASDriverIdentify()                           */
/************************************************************************/

static int OGRNASDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->fpL == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Check for a UTF-8 BOM and skip if found                         */
/*                                                                      */
/*      TODO: BOM is variable-length parameter and depends on encoding. */
/*            Add BOM detection for other encodings.                    */
/* -------------------------------------------------------------------- */

    // Used to skip to actual beginning of XML data
    const char* szPtr = (const char*)poOpenInfo->pabyHeader;

    if( ( (unsigned char)szPtr[0] == 0xEF )
        && ( (unsigned char)szPtr[1] == 0xBB )
        && ( (unsigned char)szPtr[2] == 0xBF) )
    {
        szPtr += 3;
    }

/* -------------------------------------------------------------------- */
/*      Here, we expect the opening chevrons of NAS tree root element   */
/* -------------------------------------------------------------------- */
    if( szPtr[0] != '<' )
        return FALSE;

    if( !poOpenInfo->TryToIngest(8192) )
        return FALSE;
    szPtr = (const char*)poOpenInfo->pabyHeader;

    if( strstr(szPtr,"opengis.net/gml") == NULL
        || (strstr(szPtr,"NAS-Operationen.xsd") == NULL &&
            strstr(szPtr,"NAS-Operationen_optional.xsd") == NULL &&
            strstr(szPtr,"AAA-Fachschema.xsd") == NULL ) )
    {
        return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRNASDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRNASDataSource    *poDS;

    if( poOpenInfo->eAccess == GA_Update ||
        !OGRNASDriverIdentify(poOpenInfo) )
        return NULL;

    VSIFCloseL(poOpenInfo->fpL);
    poOpenInfo->fpL = NULL;

    poDS = new OGRNASDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRNAS()                           */
/************************************************************************/

void RegisterOGRNAS()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "NAS" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "NAS" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "NAS - ALKIS" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xml" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_nas.html" );

        poDriver->pfnOpen = OGRNASDriverOpen;
        poDriver->pfnIdentify = OGRNASDriverIdentify;
        poDriver->pfnUnloadDriver = OGRNASDriverUnload;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
