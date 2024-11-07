/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGC Features and Geometries JSON (JSON-FG)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_jsonfg.h"

/************************************************************************/
/*             OGRJSONFGStreamingParserGetMaxObjectSize()               */
/************************************************************************/

static size_t OGRJSONFGStreamingParserGetMaxObjectSize()
{
    const double dfTmp =
        CPLAtof(CPLGetConfigOption("OGR_JSONFG_MAX_OBJ_SIZE", "200"));
    return dfTmp > 0 ? static_cast<size_t>(dfTmp * 1024 * 1024) : 0;
}

/************************************************************************/
/*                     OGRJSONFGStreamingParser()                       */
/************************************************************************/

OGRJSONFGStreamingParser::OGRJSONFGStreamingParser(OGRJSONFGReader &oReader,
                                                   bool bFirstPass)
    : OGRJSONCollectionStreamingParser(
          bFirstPass, /*bStoreNativeData=*/false,
          OGRJSONFGStreamingParserGetMaxObjectSize()),
      m_oReader(oReader)
{
}

/************************************************************************/
/*                   ~OGRJSONFGStreamingParser()                        */
/************************************************************************/

OGRJSONFGStreamingParser::~OGRJSONFGStreamingParser() = default;

/************************************************************************/
/*                OGRJSONFGStreamingParser::Clone()                     */
/************************************************************************/

std::unique_ptr<OGRJSONFGStreamingParser> OGRJSONFGStreamingParser::Clone()
{
    auto poRet =
        std::make_unique<OGRJSONFGStreamingParser>(m_oReader, IsFirstPass());
    poRet->m_osRequestedLayer = m_osRequestedLayer;
    return poRet;
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

std::pair<std::unique_ptr<OGRFeature>, OGRLayer *>
OGRJSONFGStreamingParser::GetNextFeature()
{
    if (m_nCurFeatureIdx < m_apoFeatures.size())
    {
        auto poRet = std::move(m_apoFeatures[m_nCurFeatureIdx]);
        m_apoFeatures[m_nCurFeatureIdx].first = nullptr;
        m_apoFeatures[m_nCurFeatureIdx].second = nullptr;
        m_nCurFeatureIdx++;
        return poRet;
    }
    m_nCurFeatureIdx = 0;
    m_apoFeatures.clear();
    return std::pair(nullptr, nullptr);
}

/************************************************************************/
/*                          AnalyzeFeature()                            */
/************************************************************************/

void OGRJSONFGStreamingParser::GotFeature(json_object *poObj, bool bFirstPass,
                                          const std::string & /*osJson*/)
{
    if (bFirstPass)
    {
        m_oReader.GenerateLayerDefnFromFeature(poObj);
    }
    else
    {
        OGRJSONFGStreamedLayer *poStreamedLayer = nullptr;
        auto poFeat = m_oReader.ReadFeature(poObj, m_osRequestedLayer.c_str(),
                                            nullptr, &poStreamedLayer);
        if (poFeat)
        {
            CPLAssert(poStreamedLayer);
            m_apoFeatures.emplace_back(
                std::pair(std::move(poFeat), poStreamedLayer));
        }
    }
}

/************************************************************************/
/*                            TooComplex()                              */
/************************************************************************/

void OGRJSONFGStreamingParser::TooComplex()
{
    if (!ExceptionOccurred())
        EmitException("JSON object too complex/large. You may define the "
                      "OGR_JSONFG_MAX_OBJ_SIZE configuration option to "
                      "a value in megabytes to allow "
                      "for larger features, or 0 to remove any size limit.");
}
