/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  ESRIJSON driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault, <even.rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_geojson.h"

#include <stdlib.h>
#include <string.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogrgeojsonutils.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                       OGRESRIJSONDriverIdentify()                    */
/************************************************************************/

static int OGRESRIJSONDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType = ESRIJSONDriverGetSourceType(poOpenInfo);
    if( nSrcType == eGeoJSONSourceUnknown )
        return FALSE;
    if( nSrcType == eGeoJSONSourceService &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:") )
    {
        return -1;
    }
    return TRUE;
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset* OGRESRIJSONDriverOpen( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType = ESRIJSONDriverGetSourceType(poOpenInfo);
    if( nSrcType == eGeoJSONSourceUnknown )
        return nullptr;
    return OGRGeoJSONDriverOpenInternal(poOpenInfo, nSrcType, "ESRIJSON");
}

/************************************************************************/
/*                          RegisterOGRESRIJSON()                       */
/************************************************************************/

void RegisterOGRESRIJSON()
{
    if( !GDAL_CHECK_VERSION("OGR/ESRIJSON driver") )
        return;

    if( GDALGetDriverByName( "ESRIJSON" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ESRIJSON" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ESRIJSON" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "json" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/esrijson.html" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='FEATURE_SERVER_PAGING' type='boolean' description='Whether to automatically scroll through results with a ArcGIS Feature Service endpoint'/>"
"</OpenOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                               "<CreationOptionList/>");

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRESRIJSONDriverOpen;
    poDriver->pfnIdentify = OGRESRIJSONDriverIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
