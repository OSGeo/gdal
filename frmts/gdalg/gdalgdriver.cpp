/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDAL Algorithm driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_json.h"
#include "cpl_string.h"

#include "gdalalgorithm.h"
#include "gdal_proxy.h"
#include "gdal_priv.h"

/************************************************************************/
/*                            GDALGDataset                              */
/************************************************************************/

class GDALGDataset final : public GDALProxyDataset
{
  public:
    GDALGDataset(std::unique_ptr<GDALAlgorithm> poAlg, GDALDataset *poDS);

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);

  protected:
    GDALDataset *RefUnderlyingDataset() const override
    {
        return m_poUnderlyingDS;
    }

    void UnrefUnderlyingDataset(GDALDataset *) const override
    {
    }

  private:
    std::unique_ptr<GDALAlgorithm> m_poAlg{};
    GDALDataset *m_poUnderlyingDS = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALGDataset)

    GDALDriver *GetDriver() override
    {
        return poDriver;
    }

    int GetLayerCount() override
    {
        return m_poUnderlyingDS->GetLayerCount();
    }

    OGRLayer *GetLayer(int idx) override
    {
        return m_poUnderlyingDS->GetLayer(idx);
    }

    OGRLayer *GetLayerByName(const char *pszName) override
    {
        return m_poUnderlyingDS->GetLayerByName(pszName);
    }

    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override
    {
        return m_poUnderlyingDS->ExecuteSQL(pszStatement, poSpatialFilter,
                                            pszDialect);
    }

    void ResetReading() override
    {
        m_poUnderlyingDS->ResetReading();
    }

    OGRFeature *GetNextFeature(OGRLayer **ppoBelongingLayer,
                               double *pdfProgressPct,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData) override
    {
        return m_poUnderlyingDS->GetNextFeature(
            ppoBelongingLayer, pdfProgressPct, pfnProgress, pProgressData);
    }

    int TestCapability(const char *pszCap) override
    {
        return m_poUnderlyingDS->TestCapability(pszCap);
    }
};

/************************************************************************/
/*                          GDALGRasterBand                             */
/************************************************************************/

class GDALGRasterBand final : public GDALProxyRasterBand
{
  public:
    explicit GDALGRasterBand(GDALRasterBand *poUnderlyingBand);

  protected:
    GDALRasterBand *
    RefUnderlyingRasterBand(bool /* bForceOpen */) const override
    {
        return m_poUnderlyingBand;
    }

    void UnrefUnderlyingRasterBand(GDALRasterBand *) const override
    {
    }

  private:
    GDALRasterBand *m_poUnderlyingBand = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALGRasterBand)
};

/************************************************************************/
/*                      GDALGDataset::GDALGDataset()                    */
/************************************************************************/

GDALGDataset::GDALGDataset(std::unique_ptr<GDALAlgorithm> poAlg,
                           GDALDataset *poDS)
    : m_poAlg(std::move(poAlg)), m_poUnderlyingDS(poDS)
{
    nRasterXSize = m_poUnderlyingDS->GetRasterXSize();
    nRasterYSize = m_poUnderlyingDS->GetRasterYSize();
    for (int i = 0; i < m_poUnderlyingDS->GetRasterCount(); ++i)
    {
        SetBand(i + 1, std::make_unique<GDALGRasterBand>(
                           m_poUnderlyingDS->GetRasterBand(i + 1)));
    }
}

/************************************************************************/
/*                    GDALGRasterBand::GDALGRasterBand()                */
/************************************************************************/

GDALGRasterBand::GDALGRasterBand(GDALRasterBand *poUnderlyingBand)
    : m_poUnderlyingBand(poUnderlyingBand)
{
    nBand = poUnderlyingBand->GetBand();
    eDataType = poUnderlyingBand->GetRasterDataType();
    nRasterXSize = poUnderlyingBand->GetXSize();
    nRasterYSize = poUnderlyingBand->GetYSize();
    poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                         GDALGDataset::Identify()                     */
/************************************************************************/

/* static */ int GDALGDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    return (poOpenInfo->pabyHeader &&
            strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                   "\"gdal_streamed_alg\"")) ||
           (strstr(poOpenInfo->pszFilename, "\"gdal_streamed_alg\""));
}

/************************************************************************/
/*                         GDALGDataset::Open()                         */
/************************************************************************/

/* static */ GDALDataset *GDALGDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;
    CPLJSONDocument oDoc;
    if (poOpenInfo->pabyHeader)
    {
        if (!oDoc.Load(poOpenInfo->pszFilename))
        {
            return nullptr;
        }
    }
    else
    {
        if (!oDoc.LoadMemory(
                reinterpret_cast<const char *>(poOpenInfo->pszFilename)))
        {
            return nullptr;
        }
    }
    if (oDoc.GetRoot().GetString("type") != "gdal_streamed_alg")
        return nullptr;
    const std::string osCommandLine = oDoc.GetRoot().GetString("command_line");
    if (osCommandLine.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "command_line missing");
        return nullptr;
    }

    const CPLStringList aosArgs(CSLTokenizeString(osCommandLine.c_str()));

    auto alg = GDALGlobalAlgorithmRegistry::GetSingleton().Instantiate(
        GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME);

    if (poOpenInfo->pabyHeader &&
        oDoc.GetRoot().GetBool("relative_paths_relative_to_this_file", true))
    {
        alg->SetReferencePathForRelativePaths(
            CPLGetPathSafe(poOpenInfo->pszFilename).c_str());
    }

    alg->SetExecutionForStreamedOutput();

    alg->SetCallPath(std::vector<std::string>{aosArgs[0]});
    std::vector<std::string> args;
    for (int i = 1; i < aosArgs.size(); ++i)
        args.push_back(aosArgs[i]);
    if (!alg->ParseCommandLineArguments(args))
    {
        return nullptr;
    }
    if (!alg->GetActualAlgorithm().SupportsStreamedOutput())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Algorithm %s does not support a streamed output",
                 alg->GetActualAlgorithm().GetName().c_str());
        return nullptr;
    }

    if (!alg->Run(nullptr, nullptr))
    {
        return nullptr;
    }

    const auto outputArg = alg->GetActualAlgorithm().GetArg("output");
    if (outputArg && outputArg->GetType() == GAAT_DATASET)
    {
        auto &val = outputArg->Get<GDALArgDatasetValue>();
        auto poUnderlyingDS = val.GetDatasetRef();
        if (poUnderlyingDS)
        {
            if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) &&
                !(poOpenInfo->nOpenFlags & GDAL_OF_VECTOR))
            {
                // Don't return if asked for a raster dataset but the
                // underlying one is not.
                if (poUnderlyingDS->GetRasterCount() == 0 &&
                    !poUnderlyingDS->GetMetadata("SUBDATASETS"))
                {
                    return nullptr;
                }
            }
            else if (!(poOpenInfo->nOpenFlags & GDAL_OF_RASTER) &&
                     (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR))
            {
                // Don't return if asked for a vector dataset but the
                // underlying one is not.
                if (poUnderlyingDS->GetLayerCount() == 0)
                {
                    return nullptr;
                }
            }
            return std::make_unique<GDALGDataset>(std::move(alg),
                                                  poUnderlyingDS)
                .release();
        }
    }

    return nullptr;
}

/************************************************************************/
/*                       GDALRegister_GDALG()                           */
/************************************************************************/

void GDALRegister_GDALG()
{
    if (GDALGetDriverByName("GDALG") != nullptr)
        return;

    auto poDriver = std::make_unique<GDALDriver>();

    poDriver->SetDescription("GDALG");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "GDAL Streamed Algorithm driver");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "gdalg.json");

    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = GDALGDataset::Identify;
    poDriver->pfnOpen = GDALGDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
