/******************************************************************************
 * $Id$
 *
 * Project:  GTM Driver
 * Purpose:  Implementation of OGRGTMDriver class.
 * Author:   Leonardo de Paula Rosa Piga; http://lampiao.lsc.ic.unicamp.br/~piga
 *
 ******************************************************************************
 * Copyright (c) 2009, Leonardo de Paula Rosa Piga
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
#include "ogr_gtm.h"
#include "cpl_conv.h"
#include "cpl_error.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGTMDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == NULL ||
        poOpenInfo->nHeaderBytes < 13)
        return NULL;

/* -------------------------------------------------------------------- */
/*      If it looks like a GZip header, this may be a .gtz file, so     */
/*      try opening with the /vsigzip/ prefix                           */
/* -------------------------------------------------------------------- */
    if (poOpenInfo->pabyHeader[0] == 0x1f && ((unsigned char*)poOpenInfo->pabyHeader)[1] == 0x8b &&
        strncmp(poOpenInfo->pszFilename, "/vsigzip/", strlen("/vsigzip/")) != 0)
    {
        /* ok */
    }
    else
    {
        short version = CPL_LSBINT16PTR(poOpenInfo->pabyHeader);
        if (version != 211 ||
            strncmp((const char*)poOpenInfo->pabyHeader + 2, "TrackMaker", strlen("TrackMaker")) != 0 )
        {
            return NULL;
        }
    }

    OGRGTMDataSource *poDS = new OGRGTMDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, FALSE ) )
    {
        delete poDS;
        poDS = NULL;
    }
    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGTMDriverCreate( const char * pszName,
                                    int nBands, int nXSize, int nYSize, GDALDataType eDT,
                                    char **papszOptions )
{
    CPLAssert( NULL != pszName );
    CPLDebug( "GTM", "Attempt to create: %s", pszName );
    
    OGRGTMDataSource *poDS = new OGRGTMDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRGTM()                           */
/************************************************************************/

void RegisterOGRGTM()
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GPSTrackMaker" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GPSTrackMaker" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "GPSTrackMaker" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "gtm gtz" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_gtm.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRGTMDriverOpen;
        poDriver->pfnCreate = OGRGTMDriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}


