/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements OGRTigerDriver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRTigerDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !poOpenInfo->bStatOK )
        return NULL;
    char** papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if( papszSiblingFiles != NULL )
    {
        bool bFoundCompatibleFile = false;
        for( int i = 0; papszSiblingFiles[i] != NULL; i++ )
        {
            int nLen = (int)strlen(papszSiblingFiles[i]);
            if( nLen > 4 &&
                papszSiblingFiles[i][nLen-4] == '.' &&
                papszSiblingFiles[i][nLen-1] == '1' )
            {
                bFoundCompatibleFile = true;
                break;
            }
        }
        if( !bFoundCompatibleFile )
            return NULL;
    }

    OGRTigerDataSource  *poDS = new OGRTigerDataSource;

    if( !poDS->Open( poOpenInfo->pszFilename, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    if( poDS != NULL && poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Tiger Driver doesn't support update." );
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRTigerDriverCreate( const char * pszName,
                                          CPL_UNUSED int nBands,
                                          CPL_UNUSED int nXSize,
                                          CPL_UNUSED int nYSize,
                                          CPL_UNUSED GDALDataType eDT,
                                          char **papszOptions )
{
    OGRTigerDataSource *poDS = new OGRTigerDataSource();

    if( poDS->Create( pszName, papszOptions ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}

/************************************************************************/
/*                           RegisterOGRTiger()                         */
/************************************************************************/

void RegisterOGRTiger()

{
    if( GDALGetDriverByName( "TIGER" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "TIGER" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "U.S. Census TIGER/Line" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_tiger.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRTigerDriverOpen;
    poDriver->pfnCreate = OGRTigerDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
