/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALPamMDArray class
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_PAM_MULTIDIM_H_INCLUDED
#define GDAL_PAM_MULTIDIM_H_INCLUDED

#include "cpl_port.h"

#include "gdal_multidim.h"

#include <memory>
#include <string>

//! @cond Doxygen_Suppress

/* ******************************************************************** */
/*                          GDALPamMultiDim                             */
/* ******************************************************************** */

/** Class that serializes/deserializes metadata on multidimensional objects.
 * Currently SRS on GDALMDArray.
 */
class CPL_DLL GDALPamMultiDim final
{
    struct Private;
    std::unique_ptr<Private> d;

    void Load();
    void Save();

  public:
    explicit GDALPamMultiDim(const std::string &osFilename);
    ~GDALPamMultiDim();

    std::shared_ptr<OGRSpatialReference>
    GetSpatialRef(const std::string &osArrayFullName,
                  const std::string &osContext);

    void SetSpatialRef(const std::string &osArrayFullName,
                       const std::string &osContext,
                       const OGRSpatialReference *poSRS);

    CPLErr GetStatistics(const std::string &osArrayFullName,
                         const std::string &osContext, bool bApproxOK,
                         double *pdfMin, double *pdfMax, double *pdfMean,
                         double *pdfStdDev, GUInt64 *pnValidCount);

    void SetStatistics(const std::string &osArrayFullName,
                       const std::string &osContext, bool bApproxStats,
                       double dfMin, double dfMax, double dfMean,
                       double dfStdDev, GUInt64 nValidCount);

    void ClearStatistics();

    void ClearStatistics(const std::string &osArrayFullName,
                         const std::string &osContext);

    static std::shared_ptr<GDALPamMultiDim>
    GetPAM(const std::shared_ptr<GDALMDArray> &poParent);
};

/* ******************************************************************** */
/*                          GDALPamMDArray                              */
/* ******************************************************************** */

/** Class that relies on GDALPamMultiDim to serializes/deserializes metadata. */
class CPL_DLL GDALPamMDArray : public GDALMDArray
{
    std::shared_ptr<GDALPamMultiDim> m_poPam;

  protected:
    GDALPamMDArray(const std::string &osParentName, const std::string &osName,
                   const std::shared_ptr<GDALPamMultiDim> &poPam,
                   const std::string &osContext = std::string());

    bool SetStatistics(bool bApproxStats, double dfMin, double dfMax,
                       double dfMean, double dfStdDev, GUInt64 nValidCount,
                       CSLConstList papszOptions) override;

  public:
    const std::shared_ptr<GDALPamMultiDim> &GetPAM() const
    {
        return m_poPam;
    }

    CPLErr GetStatistics(bool bApproxOK, bool bForce, double *pdfMin,
                         double *pdfMax, double *pdfMean, double *padfStdDev,
                         GUInt64 *pnValidCount, GDALProgressFunc pfnProgress,
                         void *pProgressData) override;

    void ClearStatistics() override;

    bool SetSpatialRef(const OGRSpatialReference *poSRS) override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;
};

//! @endcond

#endif
