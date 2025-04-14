/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Class to abstract outputting to a vector layer
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_OUTPUT_ABSTRACT_INCLUDED
#define GDALALG_VECTOR_OUTPUT_ABSTRACT_INCLUDED

#include "gdalalgorithm.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                  GDALVectorOutputAbstractAlgorithm                   */
/************************************************************************/

class CPL_DLL
    GDALVectorOutputAbstractAlgorithm /* non-final*/ : public GDALAlgorithm
{
  protected:
    GDALVectorOutputAbstractAlgorithm(const std::string &name,
                                      const std::string &description,
                                      const std::string &helpURL)
        : GDALAlgorithm(name, description, helpURL)
    {
    }

    void AddAllOutputArgs();

    struct SetupOutputDatasetRet
    {
        std::unique_ptr<GDALDataset> newDS{};
        GDALDataset *outDS =
            nullptr;  // either newDS.get() or m_outputDataset.GetDatasetRef()
        OGRLayer *layer = nullptr;

        SetupOutputDatasetRet() = default;
        SetupOutputDatasetRet(SetupOutputDatasetRet &&) = default;
        SetupOutputDatasetRet &operator=(SetupOutputDatasetRet &&) = default;

        CPL_DISALLOW_COPY_ASSIGN(SetupOutputDatasetRet)
    };

    SetupOutputDatasetRet SetupOutputDataset();
    bool SetDefaultOutputLayerNameIfNeeded(GDALDataset *poOutDS);

    std::string m_outputFormat{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    std::vector<std::string> m_layerCreationOptions{};
    std::string m_outputLayerName{};
    bool m_overwrite = false;
    bool m_update = false;
    bool m_overwriteLayer = false;
    bool m_appendLayer = false;
};

//! @endcond

#endif
