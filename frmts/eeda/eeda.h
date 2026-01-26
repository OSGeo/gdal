/******************************************************************************
 *
 * Project:  Earth Engine Data API Images driver
 * Purpose:  Earth Engine Data API Images driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef EEDA_H_INCLUDED
#define EEDA_H_INCLUDED

#include "cpl_string.h"
#include "gdal.h"
#include "cpl_json_header.h"
#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0
#include "gdal_priv.h"
#include "cpl_http.h"
#include <vector>
#include <map>

CPLHTTPResult *EEDAHTTPFetch(const char *pszURL, CSLConstList papszOptions);

/************************************************************************/
/*                            EEDAIBandDesc                             */
/************************************************************************/

class EEDAIBandDesc
{
  public:
    CPLString osName{};
    CPLString osWKT{};
    GDALDataType eDT{GDT_Unknown};
    GDALGeoTransform gt{};
    int nWidth{0};
    int nHeight{0};

    /* Check if it similar enough for being considered as a compatible */
    /* GDAL band in the same dataset */
    bool IsSimilar(const EEDAIBandDesc &oOther) const
    {
        return osWKT == oOther.osWKT && gt == oOther.gt &&
               nWidth == oOther.nWidth && nHeight == oOther.nHeight;
    }
};

std::vector<EEDAIBandDesc>
BuildBandDescArray(json_object *poBands,
                   std::map<CPLString, CPLString> &oMapCodeToWKT);

/************************************************************************/
/*                         GDALEEDABaseDataset                          */
/************************************************************************/

class GDALEEDABaseDataset CPL_NON_FINAL : public GDALDataset
{
  protected:
    bool m_bMustCleanPersistent;
    CPLString m_osBaseURL{};
    CPLString m_osBearer{};
    GIntBig m_nExpirationTime;

    char **GetBaseHTTPOptions();
    static CPLString ConvertPathToName(const CPLString &path);

  public:
    GDALEEDABaseDataset();
    ~GDALEEDABaseDataset() override;

    const CPLString &GetBaseURL() const
    {
        return m_osBaseURL;
    }
};

#endif  //  EEDA_H_INCLUDED
