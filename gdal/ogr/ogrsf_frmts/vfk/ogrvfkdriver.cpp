/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVFKDriver class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "ogr_vfk.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static int OGRVFKDriverIdentify(GDALOpenInfo* poOpenInfo)
{
    return ( poOpenInfo->fpL != NULL &&
             poOpenInfo->nHeaderBytes >= 2 &&
             strncmp((const char*)poOpenInfo->pabyHeader, "&H", 2) == 0 );
}

/*
  \brief Open existing data source
  \return NULL on failure
*/
static GDALDataset *OGRVFKDriverOpen(GDALOpenInfo* poOpenInfo)
{
    OGRVFKDataSource *poDS;

    if( poOpenInfo->eAccess == GA_Update ||
        !OGRVFKDriverIdentify(poOpenInfo) )
        return NULL;

    poDS = new OGRVFKDataSource();

    if(!poDS->Open(poOpenInfo->pszFilename, TRUE) || poDS->GetLayerCount() == 0) {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}


/*!
  \brief Register VFK driver
*/
void RegisterOGRVFK()
{
    if (!GDAL_CHECK_VERSION("OGR/VFK driver"))
        return;
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "VFK" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "VFK" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Czech Cadastral Exchange Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "vfk" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_vfk.html" );

        poDriver->pfnOpen = OGRVFKDriverOpen;
        poDriver->pfnIdentify = OGRVFKDriverIdentify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
