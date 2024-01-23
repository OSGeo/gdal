/******************************************************************************
 *
 * Project:  GPX Translator
 * Purpose:  Implements OGRGPXDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2007-2008, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_gpx.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"

/************************************************************************/
/*                               Identify()                             */
/************************************************************************/

static int OGRGPXDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == nullptr)
        return false;

    return strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                  "<gpx") != nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGPXDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update || !OGRGPXDriverIdentify(poOpenInfo))
        return nullptr;

    OGRGPXDataSource *poDS = new OGRGPXDataSource();

    if (!poDS->Open(poOpenInfo))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *
OGRGPXDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                   CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                   CPL_UNUSED GDALDataType eDT, CPL_UNUSED char **papszOptions)
{
    OGRGPXDataSource *poDS = new OGRGPXDataSource();

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRGPXDriverDelete(const char *pszFilename)

{
    if (VSIUnlink(pszFilename) == 0)
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRGPX()                           */
/************************************************************************/

void RegisterOGRGPX()

{
    if (!GDAL_CHECK_VERSION("OGR/GPX driver"))
        return;

    if (GDALGetDriverByName("GPX") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("GPX");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GPX");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gpx");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/gpx.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='N_MAX_LINKS' type='integer' default='2' "
        "description='Maximum number of links attributes'/>"
        "  <Option name='ELE_AS_25D' type='boolean' default='NO' "
        "description='Whether to use the value of the ele element as the Z "
        "ordinate of geometries'/>"
        "  <Option name='SHORT_NAMES' type='boolean' default='NO' "
        "description='Whether to use short field names (typically for "
        "shapefile compatibility'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
#ifdef _WIN32
        "  <Option name='LINEFORMAT' type='string-select' "
        "description='end-of-line sequence' default='CRLF'>"
#else
        "  <Option name='LINEFORMAT' type='string-select' "
        "description='end-of-line sequence' default='LF'>"
#endif
        "    <Value>CRLF</Value>"
        "    <Value>LF</Value>"
        "  </Option>"
        "  <Option name='GPX_USE_EXTENSIONS' type='boolean' "
        "description='Whether to write non-GPX attributes in an "
        "&lt;extensions&gt; tag' default='NO'/>"
        "  <Option name='GPX_EXTENSIONS_NS' type='string' "
        "description='Namespace value used for extension tags' default='ogr'/>"
        "  <Option name='GPX_EXTENSIONS_NS_URL' type='string' "
        "description='Namespace URI' default='http://osgeo.org/gdal'/>"
        "  <Option name='METADATA_AUTHOR_EMAIL' type='string'/>"
        "  <Option name='METADATA_AUTHOR_NAME' type='string'/>"
        "  <Option name='METADATA_AUTHOR_LINK_HREF' type='string'/>"
        "  <Option name='METADATA_AUTHOR_LINK_TEXT' type='string'/>"
        "  <Option name='METADATA_AUTHOR_LINK_TYPE' type='string'/>"
        "  <Option name='METADATA_COPYRIGHT_AUTHOR' type='string'/>"
        "  <Option name='METADATA_COPYRIGHT_LICENSE' type='string'/>"
        "  <Option name='METADATA_COPYRIGHT_YEAR' type='string'/>"
        "  <Option name='METADATA_DESCRIPTION' type='string'/>"
        "  <Option name='METADATA_KEYWORDS' type='string'/>"
        "  <Option name='METADATA_LINK_*_HREF' type='string'/>"
        "  <Option name='METADATA_LINK_*_TEXT' type='string'/>"
        "  <Option name='METADATA_LINK_*_TYPE' type='string'/>"
        "  <Option name='METADATA_NAME' type='string'/>"
        "  <Option name='METADATA_TIME' type='string'/>"
        "  <Option name='CREATOR' type='string'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='FORCE_GPX_TRACK' type='boolean' description='Whether "
        "to force layers with geometries of type wkbLineString as tracks' "
        "default='NO'/>"
        "  <Option name='FORCE_GPX_ROUTE' type='boolean' description='Whether "
        "to force layers with geometries of type wkbMultiLineString (with "
        "single line string in them) as routes' default='NO'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");

    poDriver->pfnIdentify = OGRGPXDriverIdentify;
    poDriver->pfnOpen = OGRGPXDriverOpen;
    poDriver->pfnCreate = OGRGPXDriverCreate;
    poDriver->pfnDelete = OGRGPXDriverDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
