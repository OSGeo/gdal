/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_mysql.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

static void* hMutex = NULL;
static int   bInitialized = FALSE;

/************************************************************************/
/*                        OGRMySQLDriverUnload()                        */
/************************************************************************/

static void OGRMySQLDriverUnload( GDALDriver* poDriver )
{
    if( bInitialized )
    {
        mysql_library_end();
        bInitialized = FALSE;
    }
    if( hMutex != NULL )
    {
        CPLDestroyMutex(hMutex);
        hMutex = NULL;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRMySQLDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRMySQLDataSource     *poDS;

    if( !EQUALN(poOpenInfo->pszFilename,"MYSQL:",6) )
        return NULL;
 
    {
        CPLMutexHolderD(&hMutex);
        if( !bInitialized )
        {
            if ( mysql_library_init( 0, NULL, NULL ) )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Could not initialize MySQL library" );
                return NULL;
            }
            bInitialized = TRUE;
        }
    }

    poDS = new OGRMySQLDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}


/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRMySQLDriverCreate( const char * pszName,
                                    int nBands, int nXSize, int nYSize, GDALDataType eDT,
                                    char **papszOptions )

{
    OGRMySQLDataSource     *poDS;

    poDS = new OGRMySQLDataSource();


    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, 
         "MySQL driver doesn't currently support database creation.\n"
                  "Please create database before using." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          RegisterOGRMySQL()                          */
/************************************************************************/

void RegisterOGRMySQL()

{
    if (! GDAL_CHECK_VERSION("MySQL driver"))
        return;
  
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "MySQL" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "MySQL" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "MySQL" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_mysqls.html" );

        poDriver->pfnOpen = OGRMySQLDriverOpen;
        poDriver->pfnCreate = OGRMySQLDriverCreate;
        poDriver->pfnUnloadDriver = OGRMySQLDriverUnload;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

