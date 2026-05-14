/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Streaming parser for GeoJSON-like FeatureCollection
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRJSONCOLLECTIONSTREAMING_PARSER_H_INCLUDED
#define OGRJSONCOLLECTIONSTREAMING_PARSER_H_INCLUDED

#include "cpl_json_streaming_parser.h"

#include <json.h>  // JSON-C

/************************************************************************/
/*                   OGRJSONCollectionStreamingParser                   */
/************************************************************************/

/** Streaming parser for GeoJSON-like FeatureCollection */
class OGRJSONCollectionStreamingParser CPL_NON_FINAL
    : public CPLJSonStreamingParser
{
    bool m_bFirstPass = false;

    int m_nDepth = 0;
    bool m_bInFeatures = false;
    bool m_bCanEasilyAppend = false;
    bool m_bInFeaturesArray = false;
    bool m_bInCoordinates = false;
    bool m_bInType = false;
    bool m_bIsTypeKnown = false;
    bool m_bIsFeatureCollection = false;
    bool m_bInMeasures = false;
    bool m_bInMeasuresEnabled = false;
    json_object *m_poRootObj = nullptr;
    size_t m_nRootObjMemEstimate = 0;
    json_object *m_poCurObj = nullptr;
    size_t m_nCurObjMemEstimate = 0;
    GUIntBig m_nTotalOGRFeatureMemEstimate = 0;
    bool m_bKeySet = false;
    std::string m_osCurKey{};
    std::vector<json_object *> m_apoCurObj{};
    std::vector<bool> m_abFirstMember{};
    bool m_bStoreNativeData = false;
    std::string m_osJson{};
    size_t m_nMaxObjectSize = 0;

    bool m_bStartFeature = false;
    bool m_bEndFeature = false;

    void AppendObject(json_object *poNewObj);

    CPL_DISALLOW_COPY_ASSIGN(OGRJSONCollectionStreamingParser)

  protected:
    inline bool IsFirstPass() const
    {
        return m_bFirstPass;
    }

    virtual void GotFeature(json_object *poObj, bool bFirstPass,
                            const std::string &osJson) = 0;
    virtual void TooComplex() = 0;

    bool m_bHasTopLevelMeasures = false;

  public:
    OGRJSONCollectionStreamingParser(bool bFirstPass, bool bStoreNativeData,
                                     size_t nMaxObjectSize);
    ~OGRJSONCollectionStreamingParser() override;

    void String(std::string_view) override;
    void Number(std::string_view) override;
    void Boolean(bool b) override;
    void Null() override;

    void StartObject() override;
    void EndObject() override;
    void StartObjectMember(std::string_view) override;

    void StartArray() override;
    void EndArray() override;
    void StartArrayMember() override;

    void Exception(const char * /*pszMessage*/) override;

    json_object *StealRootObject();

    inline bool HasTopLevelMeasures() const
    {
        return m_bHasTopLevelMeasures;
    }

    inline bool IsTypeKnown() const
    {
        return m_bIsTypeKnown;
    }

    inline bool IsFeatureCollection() const
    {
        return m_bIsFeatureCollection;
    }

    inline GUIntBig GetTotalOGRFeatureMemEstimate() const
    {
        return m_nTotalOGRFeatureMemEstimate;
    }

    inline bool CanEasilyAppend() const
    {
        return m_bCanEasilyAppend;
    }

    inline void ResetFeatureDetectionState()
    {
        m_bStartFeature = false;
        m_bEndFeature = false;
    }

    inline bool IsStartFeature() const
    {
        return m_bStartFeature;
    }

    inline bool IsEndFeature() const
    {
        return m_bEndFeature;
    }
};

#endif  // OGRJSONCOLLECTIONSTREAMING_PARSER_H_INCLUDED
