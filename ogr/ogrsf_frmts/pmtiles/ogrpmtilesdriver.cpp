/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
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

#include "ogr_pmtiles.h"

#include "vsipmtiles.h"

#include "ogrpmtilesfrommbtiles.h"

#ifdef HAVE_MVT_WRITE_SUPPORT
#include "mvtutils.h"
#endif

/************************************************************************/
/*                     OGRPMTilesDriverIdentify()                       */
/************************************************************************/

static int OGRPMTilesDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->nHeaderBytes < 127 || !poOpenInfo->fpL)
        return FALSE;
    return memcmp(poOpenInfo->pabyHeader, "PMTiles\x03", 8) == 0;
}

/************************************************************************/
/*                       OGRPMTilesDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRPMTilesDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (!OGRPMTilesDriverIdentify(poOpenInfo))
        return nullptr;
    auto poDS = std::make_unique<OGRPMTilesDataset>();
    if (!poDS->Open(poOpenInfo))
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                   OGRPMTilesDriverCanVectorTranslateFrom()           */
/************************************************************************/

static bool OGRPMTilesDriverCanVectorTranslateFrom(
    const char * /*pszDestName*/, GDALDataset *poSourceDS,
    CSLConstList papszVectorTranslateArguments, char ***ppapszFailureReasons)
{
    auto poSrcDriver = poSourceDS->GetDriver();
    if (!(poSrcDriver && EQUAL(poSrcDriver->GetDescription(), "MBTiles")))
    {
        if (ppapszFailureReasons)
            *ppapszFailureReasons = CSLAddString(
                *ppapszFailureReasons, "Source driver is not MBTiles");
        return false;
    }

    if (papszVectorTranslateArguments)
    {
        const int nArgs = CSLCount(papszVectorTranslateArguments);
        for (int i = 0; i < nArgs; ++i)
        {
            if (i + 1 < nArgs &&
                (strcmp(papszVectorTranslateArguments[i], "-f") == 0 ||
                 strcmp(papszVectorTranslateArguments[i], "-of") == 0))
            {
                ++i;
            }
            else
            {
                if (ppapszFailureReasons)
                    *ppapszFailureReasons =
                        CSLAddString(*ppapszFailureReasons,
                                     "Direct copy from MBTiles does not "
                                     "support GDALVectorTranslate() options");
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                   OGRPMTilesDriverVectorTranslateFrom()              */
/************************************************************************/

static GDALDataset *OGRPMTilesDriverVectorTranslateFrom(
    const char *pszDestName, GDALDataset *poSourceDS,
    CSLConstList papszVectorTranslateArguments,
    GDALProgressFunc /* pfnProgress */, void * /* pProgressData */)
{
    if (!OGRPMTilesDriverCanVectorTranslateFrom(
            pszDestName, poSourceDS, papszVectorTranslateArguments, nullptr))
    {
        return nullptr;
    }

    if (!OGRPMTilesConvertFromMBTiles(pszDestName,
                                      poSourceDS->GetDescription()))
    {
        return nullptr;
    }

    GDALOpenInfo oOpenInfo(pszDestName, GA_ReadOnly);
    return OGRPMTilesDriverOpen(&oOpenInfo);
}

#ifdef HAVE_MVT_WRITE_SUPPORT
/************************************************************************/
/*                                Create()                              */
/************************************************************************/

static GDALDataset *OGRPMTilesDriverCreate(const char *pszFilename, int nXSize,
                                           int nYSize, int nBandsIn,
                                           GDALDataType eDT,
                                           char **papszOptions)
{
    if (nXSize == 0 && nYSize == 0 && nBandsIn == 0 && eDT == GDT_Unknown)
    {
        auto poDS = std::make_unique<OGRPMTilesWriterDataset>();
        if (!poDS->Create(pszFilename, papszOptions))
            return nullptr;
        return poDS.release();
    }
    return nullptr;
}
#endif

/************************************************************************/
/*                          RegisterOGRPMTiles()                        */
/************************************************************************/

void RegisterOGRPMTiles()
{
    if (GDALGetDriverByName("PMTiles") != nullptr)
        return;

    VSIPMTilesRegister();

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("PMTiles");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ProtoMap Tiles");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "pmtiles");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/pmtiles.html");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='ZOOM_LEVEL' type='integer' "
        "description='Zoom level of full resolution. If not specified, maximum "
        "non-empty zoom level'/>"
        "  <Option name='CLIP' type='boolean' "
        "description='Whether to clip geometries to tile extent' "
        "default='YES'/>"
        "  <Option name='ZOOM_LEVEL_AUTO' type='boolean' "
        "description='Whether to auto-select the zoom level for vector layers "
        "according to spatial filter extent. Only for display purpose' "
        "default='NO'/>"
        "  <Option name='JSON_FIELD' type='boolean' "
        "description='For vector layers, "
        "whether to put all attributes as a serialized JSon dictionary'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRPMTilesDriverOpen;
    poDriver->pfnIdentify = OGRPMTilesDriverIdentify;
    poDriver->pfnCanVectorTranslateFrom =
        OGRPMTilesDriverCanVectorTranslateFrom;
    poDriver->pfnVectorTranslateFrom = OGRPMTilesDriverVectorTranslateFrom;

#ifdef HAVE_MVT_WRITE_SUPPORT
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='NAME' scope='raster,vector' type='string' "
        "description='Tileset name'/>"
        "  <Option name='DESCRIPTION' scope='raster,vector' type='string' "
        "description='A description of the layer'/>"
        "  <Option name='TYPE' scope='raster,vector' type='string-select' "
        "description='Layer type' default='overlay'>"
        "    <Value>overlay</Value>"
        "    <Value>baselayer</Value>"
        "  </Option>" MVT_MBTILES_PMTILES_COMMON_DSCO "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Float32");

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST, MVT_LCO);

    poDriver->pfnCreate = OGRPMTilesDriverCreate;
#endif

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
