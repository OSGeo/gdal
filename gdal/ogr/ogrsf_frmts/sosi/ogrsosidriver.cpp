/******************************************************************************
 * $Id$
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSIDriver.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_sosi.h"

static int bFYBAInit = FALSE;

/************************************************************************/
/*                        OGRSOSIDriverUnload()                         */
/************************************************************************/

static void OGRSOSIDriverUnload(CPL_UNUSED GDALDriver* poDriver) {

    if ( bFYBAInit )
    {
        LC_Close(); /* Close FYBA */
        bFYBAInit = FALSE;
    }
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

static GDALDataset *OGRSOSIDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( poOpenInfo->fpL == NULL ||
        strstr((const char*)poOpenInfo->pabyHeader, ".HODE") == NULL )
        return NULL;

    if ( !bFYBAInit )
    {
        LC_Init();  /* Init FYBA */
        bFYBAInit = TRUE;
    }

    OGRSOSIDataSource   *poDS = new OGRSOSIDataSource();
    if ( !poDS->Open( poOpenInfo->pszFilename, 0 ) ) {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRSOSIDriverCreate( const char * pszName,
                                         CPL_UNUSED int nBands, CPL_UNUSED int nXSize,
                                         CPL_UNUSED int nYSize, CPL_UNUSED GDALDataType eDT,
                                         CPL_UNUSED char **papszOptions )
{
    
    if ( !bFYBAInit )
    {
        LC_Init();  /* Init FYBA */
        bFYBAInit = TRUE;
    }
    OGRSOSIDataSource   *poDS = new OGRSOSIDataSource();
    if ( !poDS->Create( pszName ) ) {
        delete poDS;
        return NULL;
    }
    return poDS;
}

/************************************************************************/
/*                         RegisterOGRSOSI()                            */
/************************************************************************/

void RegisterOGRSOSI() {
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "SOSI" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "SOSI" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Norwegian SOSI Standard" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_sosi.html" );

        poDriver->pfnOpen = OGRSOSIDriverOpen;
        poDriver->pfnCreate = OGRSOSIDriverCreate;
        poDriver->pfnUnloadDriver = OGRSOSIDriverUnload;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
