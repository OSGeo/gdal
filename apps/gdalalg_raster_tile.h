/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster tile" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_TILE_INCLUDED
#define GDALALG_RASTER_TILE_INCLUDED

#include "gdalalg_raster_pipeline.h"

#include "cpl_string.h"

#include <limits>

//! @cond Doxygen_Suppress

class GDALDataset;
class GDALDriver;

typedef struct _CPLSpawnedProcess CPLSpawnedProcess;

/************************************************************************/
/*                       GDALRasterTileAlgorithm                        */
/************************************************************************/

class GDALRasterTileAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "tile";
    static constexpr const char *DESCRIPTION =
        "Generate tiles in separate files from a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_tile.html";

    explicit GDALRasterTileAlgorithm(bool standaloneStep = false);

    bool CanBeLastStep() const override
    {
        return true;
    }

    bool IsNativelyStreamingCompatible() const override
    {
        return false;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALRasterTileAlgorithm)

    std::vector<std::string> m_metadata{};
    bool m_copySrcMetadata = false;
    std::string m_tilingScheme{};
    std::string m_convention = "xyz";
    std::string m_resampling{};
    std::string m_overviewResampling{};
    int m_minZoomLevel = -1;
    int m_maxZoomLevel = -1;
    bool m_noIntersectionIsOK = false;
    int m_minTileX = -1;
    int m_minTileY = -1;
    int m_maxTileX = -1;
    int m_maxTileY = -1;
    int m_ovrZoomLevel = -1;  // Used in spawn mode
    int m_minOvrTileX = -1;   // Used in spawn mode
    int m_minOvrTileY = -1;   // Used in spawn mode
    int m_maxOvrTileX = -1;   // Used in spawn mode
    int m_maxOvrTileY = -1;   // Used in spawn mode
    int m_tileSize = 0;
    bool m_addalpha = false;
    bool m_noalpha = false;
    double m_dstNoData = 0;
    bool m_skipBlank = false;
    bool m_auxXML = false;
    bool m_resume = false;
    bool m_kml = false;
    bool m_spawned = false;
    bool m_forked = false;
    bool m_dummy = false;
    int m_numThreads = 0;
    std::string m_parallelMethod{};

    std::string m_excludedValues{};
    double m_excludedValuesPctThreshold = 50;
    double m_nodataValuesPctThreshold = 100;

    std::vector<std::string> m_webviewers{};
    std::string m_url{};
    std::string m_title{};
    std::string m_copyright{};
    std::string m_mapmlTemplate{};

    // Work variables
    std::string m_numThreadsStr{"ALL_CPUS"};
    std::map<std::string, std::string> m_mapTileMatrixIdentifierToScheme{};
    GDALDataset *m_poSrcDS = nullptr;
    bool m_bIsNamedNonMemSrcDS = false;
    GDALDriver *m_poDstDriver = nullptr;
    std::string m_osGDALPath{};

    // Private methods
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    bool ValidateOutputFormat(GDALDataType eSrcDT) const;

    static void ComputeJobChunkSize(int nMaxJobCount, int nTilesPerCol,
                                    int nTilesPerRow, double &dfTilesYPerJob,
                                    int &nYOuterIterations,
                                    double &dfTilesXPerJob,
                                    int &nXOuterIterations);

    bool AddArgToArgv(const GDALAlgorithmArg *arg,
                      CPLStringList &aosArgv) const;

    bool IsCompatibleOfSpawn(const char *&pszErrorMsg);

    int GetMaxChildCount(int nMaxJobCount) const;

    void WaitForSpawnedProcesses(
        bool &bRet, const std::vector<std::string> &asCommandLines,
        std::vector<CPLSpawnedProcess *> &ahSpawnedProcesses) const;
    bool GenerateBaseTilesSpawnMethod(
        int nBaseTilesPerCol, int nBaseTilesPerRow, int nMinTileX,
        int nMinTileY, int nMaxTileX, int nMaxTileY, uint64_t nTotalTiles,
        uint64_t nBaseTiles, GDALProgressFunc pfnProgress, void *pProgressData);

    bool GenerateOverviewTilesSpawnMethod(
        int iZ, int nOvrMinTileX, int nOvrMinTileY, int nOvrMaxTileX,
        int nOvrMaxTileY, std::atomic<uint64_t> &nCurTile, uint64_t nTotalTiles,
        GDALProgressFunc pfnProgress, void *pProgressData);
};

/************************************************************************/
/*                 GDALRasterTileAlgorithmStandalone                    */
/************************************************************************/

class GDALRasterTileAlgorithmStandalone final : public GDALRasterTileAlgorithm
{
  public:
    GDALRasterTileAlgorithmStandalone()
        : GDALRasterTileAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterTileAlgorithmStandalone() override;
};

//! @endcond

#endif
