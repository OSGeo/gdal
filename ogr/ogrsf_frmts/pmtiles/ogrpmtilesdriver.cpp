/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_pmtiles.h"

#include "vsipmtiles.h"

#include "ogrpmtilesfrommbtiles.h"

#include "mbtiles.h"

#ifdef GDAL_ENABLE_ALGORITHMS
#include "gdalalg_raster_tile.h"
#include "ogrpmtilesfromtileset.h"
#endif

#ifdef HAVE_MVT_WRITE_SUPPORT
#include "mvtutils.h"
#endif

#include <algorithm>
#include <cmath>

/************************************************************************/
/*                      OGRPMTilesDriverIdentify()                      */
/************************************************************************/

static int OGRPMTilesDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->nHeaderBytes < PMTILES_HEADER_LENGTH || !poOpenInfo->fpL ||
        !poOpenInfo->IsExtensionEqualToCI("pmtiles"))
    {
        return FALSE;
    }
    return memcmp(poOpenInfo->pabyHeader, "PMTiles\x03", 8) == 0;
}

/************************************************************************/
/*                        OGRPMTilesDriverOpen()                        */
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
/*               OGRPMTilesDriverCanVectorTranslateFrom()               */
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
/*                OGRPMTilesDriverVectorTranslateFrom()                 */
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
    oOpenInfo.nOpenFlags = GDAL_OF_VECTOR;
    return OGRPMTilesDriverOpen(&oOpenInfo);
}

#ifdef HAVE_MVT_WRITE_SUPPORT
/************************************************************************/
/*                       OGRPMTilesDriverCreate()                       */
/************************************************************************/

static GDALDataset *OGRPMTilesDriverCreate(const char *pszFilename, int nXSize,
                                           int nYSize, int nBandsIn,
                                           GDALDataType eDT,
                                           CSLConstList papszOptions)
{
    if (nXSize == 0 && nYSize == 0 && nBandsIn == 0 && eDT == GDT_Unknown)
    {
        auto poDS = std::make_unique<OGRPMTilesWriterDataset>();
        if (!poDS->Create(pszFilename, papszOptions))
            return nullptr;
        return poDS.release();
    }
    CPLError(CE_Failure, CPLE_AppDefined, "Create() not supported for raster");
    return nullptr;
}
#endif

/************************************************************************/
/*                     OGRPMTilesDriverCreateCopy()                     */
/************************************************************************/

static GDALDataset *OGRPMTilesDriverCreateCopy(const char *pszFilename,
                                               GDALDataset *poSrcDS, int,
                                               CSLConstList papszOptions,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData)
{
    auto poSrcDriver = poSrcDS->GetDriver();
    if (poSrcDriver && EQUAL(poSrcDriver->GetDescription(), "MBTiles"))
    {
        if (papszOptions && papszOptions[0])
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No creation option is supported for conversion of "
                     "MBTiles to PMTiles");
            return nullptr;
        }
        if (!OGRPMTilesConvertFromMBTiles(pszFilename,
                                          poSrcDS->GetDescription()))
            return nullptr;
    }
    else
    {
        const int nBlockSize = std::clamp(
            atoi(CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "256")), 64,
            8192);
        const char *pszResampling =
            CSLFetchNameValueDef(papszOptions, "RESAMPLING", "BILINEAR");
        CPLStringList aosOptions(papszOptions);
        if (!aosOptions.FetchNameValue("NAME"))
            aosOptions.SetNameValue("NAME",
                                    CPLGetBasenameSafe(pszFilename).c_str());
        if (!aosOptions.FetchNameValue("DESCRIPTION"))
            aosOptions.SetNameValue("DESCRIPTION",
                                    CPLGetBasenameSafe(pszFilename).c_str());

        // There are two code paths possible:
        // - the preferred one, using "gdal raster tile" when possible, since
        //   this is multi-threaded
        // - the fallback one, using a temporary MBTiles dataset

#if GDAL_ENABLE_ALGORITHMS
        std::unique_ptr<GDALDataset> poTmpSrcDS;
        std::string osTmpSrcFilename;

        // Check that the dataset can be used in a thread-safe way to be able
        // to use gdal raster tile
        bool bCanCreateThreadSafeDataset;
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            bCanCreateThreadSafeDataset =
                std::unique_ptr<GDALDataset>(GDALGetThreadSafeDataset(
                    poSrcDS, GDAL_OF_RASTER)) != nullptr;

            // Case of anonymous VRT, e.g. when doing
            // gdal_translate in.tif out.pmtiles -outsize ...
            if (!bCanCreateThreadSafeDataset && poSrcDriver &&
                EQUAL(poSrcDriver->GetDescription(), "VRT") &&
                EQUAL(poSrcDS->GetDescription(), ""))
            {
                osTmpSrcFilename =
                    CPLGenerateTempFilenameSafe(CPLGetFilename(pszFilename));
                osTmpSrcFilename += ".vrt";
                poTmpSrcDS.reset(
                    poSrcDriver->CreateCopy(osTmpSrcFilename.c_str(), poSrcDS,
                                            false, nullptr, nullptr, nullptr));
                if (poTmpSrcDS)
                {
                    poTmpSrcDS->MarkSuppressOnClose();
                    bCanCreateThreadSafeDataset = true;
                    poSrcDS = poTmpSrcDS.get();
                }
                else
                {
                    VSIUnlink(osTmpSrcFilename.c_str());
                }
            }
        }

        const char *pszTileFormat =
            CSLFetchNameValueDef(papszOptions, "TILE_FORMAT", "PNG");
        // gdal raster tile doesn't support the PNG8 tile format
        // or the ZOOM_LEVEL_STRATEGY option
        if (bCanCreateThreadSafeDataset && !EQUAL(pszTileFormat, "PNG8") &&
            !CSLFetchNameValue(papszOptions, "ZOOM_LEVEL_STRATEGY"))
        {
            std::string osTmpTilesDirectory =
                std::string(pszFilename).append(".tmp");
            if (!VSIIsLocal(pszFilename))
            {
                osTmpTilesDirectory =
                    CPLGenerateTempFilenameSafe(CPLGetFilename(pszFilename));
            }

            GDALRasterTileAlgorithm alg;
            try
            {
                alg["input"] = poSrcDS;
                alg["output"] = osTmpTilesDirectory;
                alg["format"] = pszTileFormat;
                alg["tile-size"] = nBlockSize;
                alg["min-zoom-single-tile"] = true;
                alg["resampling"] = pszResampling;
                alg["overview-resampling"] = pszResampling;
                CPLStringList aosCO;
                if (const char *pszQuality =
                        CSLFetchNameValue(papszOptions, "QUALITY"))
                {
                    if (EQUAL(pszTileFormat, "JPEG") ||
                        EQUAL(pszTileFormat, "WEBP"))
                        aosCO.SetNameValue("QUALITY", pszQuality);
                }
                if (const char *pszZLevel =
                        CSLFetchNameValue(papszOptions, "ZLEVEL"))
                {
                    if (EQUAL(pszTileFormat, "PNG"))
                        aosCO.SetNameValue("ZLEVEL", pszZLevel);
                }
                alg["creation-option"] =
                    static_cast<std::vector<std::string>>(aosCO);
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
                return nullptr;
            }

            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                pScaledProgress(GDALCreateScaledProgress(0.0, 0.9, pfnProgress,
                                                         pProgressData),
                                GDALDestroyScaledProgress);
            const bool bRet =
                alg.Run(pScaledProgress ? GDALScaledProgress : nullptr,
                        pScaledProgress.get()) &&
                alg.Finalize() &&
                OGRPMTilesConvertFromTileset(pszFilename,
                                             osTmpTilesDirectory.c_str(),
                                             poSrcDS, aosOptions.List());
            VSIRmdirRecursive(osTmpTilesDirectory.c_str());
            if (!bRet)
                return nullptr;
            if (pfnProgress)
                pfnProgress(1.0, "", pProgressData);
        }
        else
#endif
        {
            auto poMBTilesDrv =
                GetGDALDriverManager()->GetDriverByName("MBTiles");
            if (!poMBTilesDrv)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Conversion to PMTiles requires prior conversion to "
                         "MBTiles but driver is not available");
                return nullptr;
            }
            std::string osTmpMBTilesFilename =
                CPLResetExtensionSafe(pszFilename, "mbtiles");
            if (!VSIIsLocal(pszFilename))
            {
                osTmpMBTilesFilename =
                    CPLGenerateTempFilenameSafe(CPLGetFilename(pszFilename));
            }

            std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                pScaledProgress(GDALCreateScaledProgress(0.0, 2.0 / 3 * 0.9,
                                                         pfnProgress,
                                                         pProgressData),
                                GDALDestroyScaledProgress);
            std::unique_ptr<GDALDataset> poMBTilesDS(poMBTilesDrv->CreateCopy(
                osTmpMBTilesFilename.c_str(), poSrcDS, false, aosOptions.List(),
                pScaledProgress ? GDALScaledProgress : nullptr,
                pScaledProgress.get()));
            if (!poMBTilesDS)
            {
                // Error message emitted by CreateCopy()
                VSIStatBufL sStat;
                if (VSIStatL(osTmpMBTilesFilename.c_str(), &sStat) == 0 &&
                    VSIUnlink(osTmpMBTilesFilename.c_str()) != 0)
                {
                    CPLError(CE_Warning, CPLE_FileIO, "Cannot delete %s",
                             osTmpMBTilesFilename.c_str());
                }
                return nullptr;
            }
            const int nMaxDim = std::max(poMBTilesDS->GetRasterXSize(),
                                         poMBTilesDS->GetRasterYSize());
            const int nOvrCount = static_cast<int>(std::ceil(
                std::log2(static_cast<double>(nMaxDim) / nBlockSize)));
            if (nOvrCount > 0)
            {
                std::vector<int> anLevels;
                for (int i = 0; i < nOvrCount; ++i)
                    anLevels.push_back(1 << (i + 1));
                pScaledProgress.reset(GDALCreateScaledProgress(
                    2.0 / 3 * 0.9, 0.9, pfnProgress, pProgressData));
                const bool bOK =
                    (poMBTilesDS->BuildOverviews(
                         pszResampling, nOvrCount, anLevels.data(), 0, nullptr,
                         pScaledProgress ? GDALScaledProgress : nullptr,
                         pScaledProgress.get(), nullptr) == CE_None);
                if (!bOK)
                {
                    poMBTilesDS.reset();
                    // Error message emitted by BuildOverviewss()
                    if (VSIUnlink(osTmpMBTilesFilename.c_str()) != 0)
                    {
                        CPLError(CE_Warning, CPLE_FileIO, "Cannot delete %s",
                                 osTmpMBTilesFilename.c_str());
                    }
                    return nullptr;
                }
            }
            poMBTilesDS.reset();
            if (!OGRPMTilesConvertFromMBTiles(pszFilename,
                                              osTmpMBTilesFilename.c_str()))
            {
                if (VSIUnlink(osTmpMBTilesFilename.c_str()) != 0)
                {
                    CPLError(CE_Warning, CPLE_FileIO, "Cannot delete %s",
                             osTmpMBTilesFilename.c_str());
                }
                return nullptr;
            }
            if (pfnProgress)
                pfnProgress(1.0, "", pProgressData);
            VSIUnlink(osTmpMBTilesFilename.c_str());
        }
    }
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    oOpenInfo.nOpenFlags = GDAL_OF_RASTER | GDAL_OF_VECTOR;
    return OGRPMTilesDriverOpen(&oOpenInfo);
}

/************************************************************************/
/*                         RegisterOGRPMTiles()                         */
/************************************************************************/

void RegisterOGRPMTiles()
{
    if (GDALGetDriverByName("PMTiles") != nullptr)
        return;

    VSIPMTilesRegister();

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("PMTiles");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
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

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>" MBTILES_RASTER_CREATION_OPTIONS
#ifdef HAVE_MVT_WRITE_SUPPORT
        MVT_MBTILES_COMMON_DSCO
#endif
        "</CreationOptionList>");

#ifdef HAVE_MVT_WRITE_SUPPORT
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Float32");

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST, MVT_LCO);

    poDriver->pfnCreate = OGRPMTilesDriverCreate;
#endif
    poDriver->pfnCreateCopy = OGRPMTilesDriverCreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
