/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset identify" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "gdalalg_dataset_identify.h"

#include "cpl_string.h"
#include "gdal_dataset.h"
#include "gdal_driver.h"
#include "gdal_drivermanager.h"
#include "gdal_rasterband.h"
#include "ogrsf_frmts.h"

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALDatasetIdentifyAlgorithm()                    */
/************************************************************************/

GDALDatasetIdentifyAlgorithm::GDALDatasetIdentifyAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL), m_oWriter(JSONPrint, this)
{
    AddProgressArg();

    auto &arg = AddArg("filename", 0, _("File or directory name"), &m_filename)
                    .AddAlias(GDAL_ARG_NAME_INPUT)
                    .SetPositional()
                    .SetRequired();
    SetAutoCompleteFunctionForFilename(arg, 0);

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR,
                        /* positionalAndRequired = */ false)
        .SetDatasetInputFlags(GADV_NAME);

    AddOutputFormatArg(&m_format)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE})
        .AddMetadataItem(GAAMDI_EXTRA_FORMATS, {"json", "text"});

    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);
    AddArg(GDAL_ARG_NAME_OUTPUT_LAYER, 'l', _("Output layer name"),
           &m_outputLayerName);
    AddOverwriteArg(&m_overwrite);

    AddArg("recursive", 'r', _("Recursively scan files/folders for datasets"),
           &m_recursive);

    AddArg("force-recursive", 0,
           _("Recursively scan folders for datasets, forcing "
             "recursion in folders recognized as valid formats"),
           &m_forceRecursive);

    AddArg("detailed", 0,
           _("Most detailed output. Reports the presence of georeferencing, "
             "if a GeoTIFF file is cloud optimized, etc."),
           &m_detailed);

    AddArg("report-failures", 0,
           _("Report failures if file type is unidentified"),
           &m_reportFailures);

    AddOutputStringArg(&m_output);
    AddStdoutArg(&m_stdout);
}

/************************************************************************/
/*                   ~GDALDatasetIdentifyAlgorithm()                    */
/************************************************************************/

GDALDatasetIdentifyAlgorithm::~GDALDatasetIdentifyAlgorithm() = default;

/************************************************************************/
/*                GDALDatasetIdentifyAlgorithm::Print()                 */
/************************************************************************/

void GDALDatasetIdentifyAlgorithm::Print(const char *str)
{
    if (m_fpOut)
        m_fpOut->Write(str, 1, strlen(str));
    else if (m_stdout)
        fwrite(str, 1, strlen(str), stdout);
    else
        m_output += str;
}

/************************************************************************/
/*              GDALDatasetIdentifyAlgorithm::JSONPrint()               */
/************************************************************************/

/* static */ void GDALDatasetIdentifyAlgorithm::JSONPrint(const char *pszTxt,
                                                          void *pUserData)
{
    static_cast<GDALDatasetIdentifyAlgorithm *>(pUserData)->Print(pszTxt);
}

/************************************************************************/
/*                              Process()                               */
/************************************************************************/

bool GDALDatasetIdentifyAlgorithm::Process(const char *pszTarget,
                                           CSLConstList papszSiblingList,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData)

{
    if (IsCalledFromCommandLine())
        pfnProgress = nullptr;

    if (m_format.empty())
        m_format = IsCalledFromCommandLine() ? "text" : "json";

    GDALDriverH hDriver = nullptr;
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        hDriver = GDALIdentifyDriver(pszTarget, papszSiblingList);
    }

    const char *pszDriverName = hDriver ? GDALGetDriverShortName(hDriver) : "";

    CPLStringList aosFileList;
    std::string osLayout;
    bool bHasCRS = false;
    bool bHasGeoTransform = false;
    bool bHasOverview = false;
    if (hDriver && m_detailed)
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        const char *const apszDriver[] = {pszDriverName, nullptr};
        auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            pszTarget, 0, apszDriver, nullptr, papszSiblingList));
        if (poDS)
        {
            if (EQUAL(pszDriverName, "GTiff"))
            {
                if (const char *pszLayout =
                        poDS->GetMetadataItem("LAYOUT", "IMAGE_STRUCTURE"))
                {
                    osLayout = pszLayout;
                }
            }

            aosFileList.Assign(poDS->GetFileList(),
                               /* bTakeOwnership = */ true);
            bHasCRS = poDS->GetSpatialRef() != nullptr;
            GDALGeoTransform gt;
            bHasGeoTransform = poDS->GetGeoTransform(gt) == CE_None;
            bHasOverview = (poDS->GetRasterCount() &&
                            poDS->GetRasterBand(1)->GetOverviewCount() > 0);
        }
    }

    if (m_poLayer)
    {
        OGRFeature oFeature(m_poLayer->GetLayerDefn());
        oFeature.SetField("filename", pszTarget);
        if (hDriver)
        {
            oFeature.SetField("driver", pszDriverName);

            if (m_detailed)
            {
                if (!osLayout.empty())
                    oFeature.SetField("layout", osLayout.c_str());

                if (!aosFileList.empty())
                {
                    oFeature.SetField("file_list", aosFileList.List());
                }

                oFeature.SetField("has_crs", bHasCRS);
                oFeature.SetField("has_geotransform", bHasGeoTransform);
                oFeature.SetField("has_overview", bHasOverview);
            }

            if (m_poLayer->CreateFeature(&oFeature) != OGRERR_NONE)
                return false;
        }
        else if (m_reportFailures)
        {
            if (m_poLayer->CreateFeature(&oFeature) != OGRERR_NONE)
                return false;
        }
    }
    else if (m_format == "json")
    {
        if (hDriver)
        {
            m_oWriter.StartObj();
            m_oWriter.AddObjKey("name");
            m_oWriter.Add(pszTarget);
            m_oWriter.AddObjKey("driver");
            m_oWriter.Add(GDALGetDriverShortName(hDriver));
            if (m_detailed)
            {
                if (!osLayout.empty())
                {
                    m_oWriter.AddObjKey("layout");
                    m_oWriter.Add(osLayout);
                }

                if (!aosFileList.empty())
                {
                    m_oWriter.AddObjKey("file_list");
                    m_oWriter.StartArray();
                    for (const char *pszFilename : aosFileList)
                    {
                        m_oWriter.Add(pszFilename);
                    }
                    m_oWriter.EndArray();
                }

                if (bHasCRS)
                {
                    m_oWriter.AddObjKey("has_crs");
                    m_oWriter.Add(true);
                }

                if (bHasGeoTransform)
                {
                    m_oWriter.AddObjKey("has_geotransform");
                    m_oWriter.Add(true);
                }

                if (bHasOverview)
                {
                    m_oWriter.AddObjKey("has_overview");
                    m_oWriter.Add(true);
                }
            }
            m_oWriter.EndObj();
        }
        else if (m_reportFailures)
        {
            m_oWriter.StartObj();
            m_oWriter.AddObjKey("name");
            m_oWriter.Add(pszTarget);
            m_oWriter.AddObjKey("driver");
            m_oWriter.AddNull();
            m_oWriter.EndObj();
        }
    }
    else
    {
        if (hDriver)
        {
            Print(pszTarget);
            Print(": ");
            Print(pszDriverName);
            if (m_detailed)
            {
                if (!osLayout.empty())
                {
                    Print(", layout=");
                    Print(osLayout.c_str());
                }
                if (aosFileList.size() > 1)
                {
                    Print(", has side-car files");
                }
                if (bHasCRS)
                {
                    Print(", has CRS");
                }
                if (bHasGeoTransform)
                {
                    Print(", has geotransform");
                }
                if (bHasOverview)
                {
                    Print(", has overview(s)");
                }
            }
            Print("\n");
        }
        else if (m_reportFailures)
        {
            Print(pszTarget);
            Print(": unrecognized\n");
        }
    }

    bool ret = true;
    VSIStatBufL sStatBuf;
    if ((m_forceRecursive || (m_recursive && hDriver == nullptr)) &&
        VSIStatL(pszTarget, &sStatBuf) == 0 && VSI_ISDIR(sStatBuf.st_mode))
    {
        const CPLStringList aosSiblingList(VSIReadDir(pszTarget));
        const int nListSize = aosSiblingList.size();
        for (int i = 0; i < nListSize; ++i)
        {
            const char *pszSubTarget = aosSiblingList[i];
            if (!(EQUAL(pszSubTarget, "..") || EQUAL(pszSubTarget, ".")))
            {
                const std::string osSubTarget =
                    CPLFormFilenameSafe(pszTarget, pszSubTarget, nullptr);

                std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
                    pScaledProgress(GDALCreateScaledProgress(
                                        static_cast<double>(i) / nListSize,
                                        static_cast<double>(i + 1) / nListSize,
                                        pfnProgress, pProgressData),
                                    GDALDestroyScaledProgress);
                ret = ret &&
                      Process(osSubTarget.c_str(), aosSiblingList.List(),
                              pScaledProgress ? GDALScaledProgress : nullptr,
                              pScaledProgress.get());
            }
        }
    }

    return ret && (!pfnProgress || pfnProgress(1.0, "", pProgressData));
}

/************************************************************************/
/*               GDALDatasetIdentifyAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALDatasetIdentifyAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    if (m_format.empty() && m_outputDataset.GetName().empty())
        m_format = IsCalledFromCommandLine() ? "text" : "json";

    if (m_format == "text" || m_format == "json")
    {
        if (!m_outputDataset.GetName().empty())
        {
            m_fpOut = VSIFilesystemHandler::OpenStatic(
                m_outputDataset.GetName().c_str(), "wb");
            if (!m_fpOut)
            {
                ReportError(CE_Failure, CPLE_FileIO, "Cannot create '%s'",
                            m_outputDataset.GetName().c_str());
                return false;
            }
        }
    }
    else
    {
        if (m_outputDataset.GetName().empty() && m_format != "MEM")
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "'output' argument must be specified for non-text or "
                        "non-json output");
            return false;
        }

        if (m_format.empty())
        {
            const CPLStringList aosFormats(GDALGetOutputDriversForDatasetName(
                m_outputDataset.GetName().c_str(), GDAL_OF_VECTOR,
                /* bSingleMatch = */ true,
                /* bEmitWarning = */ true));
            if (aosFormats.size() != 1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot guess driver for %s",
                            m_outputDataset.GetName().c_str());
                return false;
            }
            m_format = aosFormats[0];
        }

        auto poOutDrv =
            GetGDALDriverManager()->GetDriverByName(m_format.c_str());
        if (!poOutDrv)
        {
            // shouldn't happen given checks done in GDALAlgorithm unless
            // someone deregister the driver between ParseCommandLineArgs() and
            // Run()
            ReportError(CE_Failure, CPLE_AppDefined, "Driver %s does not exist",
                        m_format.c_str());
            return false;
        }

        m_poOutDS.reset(poOutDrv->Create(
            m_outputDataset.GetName().c_str(), 0, 0, 0, GDT_Unknown,
            CPLStringList(m_creationOptions).List()));
        if (!m_poOutDS)
            return false;

        if (m_outputLayerName.empty())
        {
            if (EQUAL(poOutDrv->GetDescription(), "ESRI Shapefile"))
                m_outputLayerName =
                    CPLGetBasenameSafe(m_outputDataset.GetName().c_str());
            else
                m_outputLayerName = "output";
        }

        m_poLayer = m_poOutDS->CreateLayer(
            m_outputLayerName.c_str(), nullptr,
            CPLStringList(m_layerCreationOptions).List());
        if (!m_poLayer)
            return false;

        bool ret = true;
        {
            OGRFieldDefn oFieldDefn("filename", OFTString);
            ret = m_poLayer->CreateField(&oFieldDefn) == OGRERR_NONE;
        }

        {
            OGRFieldDefn oFieldDefn("driver", OFTString);
            ret = ret && m_poLayer->CreateField(&oFieldDefn) == OGRERR_NONE;
        }

        if (m_detailed)
        {
            {
                OGRFieldDefn oFieldDefn("layout", OFTString);
                ret = ret && m_poLayer->CreateField(&oFieldDefn) == OGRERR_NONE;
            }
            {
                const char *pszSupportedFieldTypes =
                    poOutDrv->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES);
                OGRFieldDefn oFieldDefn(
                    "file_list", (pszSupportedFieldTypes &&
                                  strstr(pszSupportedFieldTypes, "StringList"))
                                     ? OFTStringList
                                     : OFTString);
                ret = ret && m_poLayer->CreateField(&oFieldDefn) == OGRERR_NONE;
            }
            {
                OGRFieldDefn oFieldDefn("has_crs", OFTInteger);
                oFieldDefn.SetSubType(OFSTBoolean);
                ret = ret && m_poLayer->CreateField(&oFieldDefn) == OGRERR_NONE;
            }
            {
                OGRFieldDefn oFieldDefn("has_geotransform", OFTInteger);
                oFieldDefn.SetSubType(OFSTBoolean);
                ret = ret && m_poLayer->CreateField(&oFieldDefn) == OGRERR_NONE;
            }
            {
                OGRFieldDefn oFieldDefn("has_overview", OFTInteger);
                oFieldDefn.SetSubType(OFSTBoolean);
                ret = ret && m_poLayer->CreateField(&oFieldDefn) == OGRERR_NONE;
            }
        }

        if (!ret)
            return false;
    }

    if (m_format == "json")
        m_oWriter.StartArray();
    int i = 0;
    bool ret = true;
    for (const std::string &osPath : m_filename)
    {
        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgress(GDALCreateScaledProgress(
                                static_cast<double>(i) /
                                    static_cast<int>(m_filename.size()),
                                static_cast<double>(i + 1) /
                                    static_cast<int>(m_filename.size()),
                                pfnProgress, pProgressData),
                            GDALDestroyScaledProgress);
        ret = ret && Process(osPath.c_str(), nullptr,
                             pScaledProgress ? GDALScaledProgress : nullptr,
                             pScaledProgress.get());
        ++i;
    }
    if (m_format == "json")
        m_oWriter.EndArray();

    if (!m_output.empty())
    {
        GetArg(GDAL_ARG_NAME_OUTPUT_STRING)->Set(m_output);
    }
    else
    {
        m_outputDataset.Set(std::move(m_poOutDS));
    }

    return ret;
}

//! @endcond
