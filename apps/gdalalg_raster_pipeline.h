/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PIPELINE_INCLUDED
#define GDALALG_RASTER_PIPELINE_INCLUDED

#include "gdalalgorithm.h"
#include "gdalalg_abstract_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                GDALRasterPipelineStepAlgorithm                       */
/************************************************************************/

class GDALRasterPipelineStepAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    ~GDALRasterPipelineStepAlgorithm() override;

  protected:
    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool standaloneStep);

    GDALRasterPipelineStepAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    const ConstructorOptions &options);

    friend class GDALRasterPipelineAlgorithm;
    friend class GDALAbstractPipelineAlgorithm<GDALRasterPipelineStepAlgorithm>;
    friend class GDALRasterMosaicStackCommonAlgorithm;

    int GetInputType() const override
    {
        return GDAL_OF_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_RASTER;
    }

    void AddInputArgs(bool openForMixedRasterVector, bool hiddenForCLI);
    void AddOutputArgs(bool hiddenForCLI);
    void AddHiddenInputDatasetArg();

    void SetOutputVRTCompatible(bool b);

    // Output arguments
    GDALInConstructionAlgorithmArg *m_outputFormatArg = nullptr;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALRasterPipelineStepAlgorithm)
};

/************************************************************************/
/*           GDALRasterPipelineNonNativelyStreamingAlgorithm            */
/************************************************************************/

class GDALRasterPipelineNonNativelyStreamingAlgorithm /* non-final */
    : public GDALRasterPipelineStepAlgorithm
{
  protected:
    GDALRasterPipelineNonNativelyStreamingAlgorithm(
        const std::string &name, const std::string &description,
        const std::string &helpURL, bool standaloneStep);

    bool IsNativelyStreamingCompatible() const override;

    std::unique_ptr<GDALDataset>
    CreateTemporaryDataset(int nWidth, int nHeight, int nBands,
                           GDALDataType eDT, bool bTiledIfPossible,
                           GDALDataset *poSrcDSForMetadata,
                           bool bCopyMetadata = true);
    std::unique_ptr<GDALDataset>
    CreateTemporaryCopy(GDALDataset *poSrcDS, int nSingleBand,
                        bool bTiledIfPossible, GDALProgressFunc pfnProgress,
                        void *pProgressData);
};

/************************************************************************/
/*                     GDALRasterPipelineAlgorithm                      */
/************************************************************************/

// This is an easter egg to pay tribute to PROJ pipeline syntax
// We accept "gdal vector +gdal=pipeline +step +gdal=read +input=in.tif +step +gdal=reproject +dst-crs=EPSG:32632 +step +gdal=write +output=out.tif +overwrite"
// as an alternative to (recommended):
// "gdal vector pipeline ! read in.tif ! reproject--dst-crs=EPSG:32632 ! write out.tif --overwrite"
#ifndef GDAL_PIPELINE_PROJ_NOSTALGIA
#define GDAL_PIPELINE_PROJ_NOSTALGIA
#endif

class GDALRasterPipelineAlgorithm final
    : public GDALAbstractPipelineAlgorithm<GDALRasterPipelineStepAlgorithm>
{
  public:
    static constexpr const char *NAME = "pipeline";
    static constexpr const char *DESCRIPTION = "Process a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pipeline.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {
#ifdef GDAL_PIPELINE_PROJ_NOSTALGIA
            GDALAlgorithmRegistry::HIDDEN_ALIAS_SEPARATOR,
            "+pipeline",
            "+gdal=pipeline",
#endif
        };
    }

    explicit GDALRasterPipelineAlgorithm(bool openForMixedRasterVector = false);

    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

    GDALDataset *GetDatasetRef()
    {
        return m_inputDataset.size() == 1 ? m_inputDataset[0].GetDatasetRef()
                                          : nullptr;
    }

  protected:
    GDALArgDatasetValue &GetOutputDataset() override
    {
        return m_outputDataset;
    }

  private:
    std::string m_helpDocCategory{};
};

//! @endcond

#endif
