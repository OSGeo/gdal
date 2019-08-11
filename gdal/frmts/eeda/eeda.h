/******************************************************************************
 *
 * Project:  Earth Engine Data API Images driver
 * Purpose:  Earth Engine Data API Images driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018, Planet Labs
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

CPLHTTPResult* EEDAHTTPFetch(const char* pszURL, char** papszOptions);

/************************************************************************/
/*                             EEDAIBandDesc                            */
/************************************************************************/

class EEDAIBandDesc
{
public:
    CPLString osName{};
    CPLString osWKT{};
    GDALDataType eDT{GDT_Unknown};
    bool      bSignedByte{false};
    std::vector<double> adfGeoTransform{0.0,1.0,0.0,0.0,0.0,1.0};
    int       nWidth{0};
    int       nHeight{0};

    /* Check if it similar enough for being considered as a compatible */
    /* GDAL band in the same dataset */
    bool IsSimilar(const EEDAIBandDesc& oOther) const
    {
        return osWKT == oOther.osWKT &&
               adfGeoTransform == oOther.adfGeoTransform &&
               nWidth == oOther.nWidth &&
               nHeight == oOther.nHeight;
    }
};

std::vector<EEDAIBandDesc> BuildBandDescArray(json_object* poBands,
                                std::map<CPLString, CPLString>& oMapCodeToWKT);

/************************************************************************/
/*                      GDALEEDABaseDataset                             */
/************************************************************************/

class GDALEEDABaseDataset CPL_NON_FINAL: public GDALDataset
{
    protected:
            bool        m_bMustCleanPersistent;
            CPLString   m_osBaseURL{};
            CPLString   m_osBearer{};
            GIntBig     m_nExpirationTime;

            char      **GetBaseHTTPOptions();
            static CPLString ConvertPathToName(const CPLString& path);

    public:
                GDALEEDABaseDataset();
                virtual ~GDALEEDABaseDataset();

                const CPLString& GetBaseURL() const { return m_osBaseURL; }
};

#endif //  EEDA_H_INCLUDED
