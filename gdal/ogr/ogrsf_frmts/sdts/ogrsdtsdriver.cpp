/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Implements OGRSDTSDriver
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

#include "ogr_sdts.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSDTSDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "DDF") )
        return NULL;
    if( poOpenInfo->nHeaderBytes < 10 )
        return NULL;
    const char* pachLeader = (const char* )poOpenInfo->pabyHeader;
    if( (pachLeader[5] != '1' && pachLeader[5] != '2'
                && pachLeader[5] != '3' )
            || pachLeader[6] != 'L'
            || (pachLeader[8] != '1' && pachLeader[8] != ' ') )
    {
        return NULL;
    }

    OGRSDTSDataSource   *poDS = new OGRSDTSDataSource();
    if( !poDS->Open( poOpenInfo->pszFilename, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    if( poDS != NULL && poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "SDTS Driver doesn't support update." );
        delete poDS;
        poDS = NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                           RegisterOGRSDTS()                          */
/************************************************************************/

void RegisterOGRSDTS()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "OGR_SDTS" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "OGR_SDTS" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "SDTS" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_sdts.html" );

        poDriver->pfnOpen = OGRSDTSDriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
