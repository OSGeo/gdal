/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Streaming parser for GeoJSON-like FeatureCollection
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#ifndef OGRJSONCOLLECTIONSTREAMING_PARSER_H_INCLUDED
#define OGRJSONCOLLECTIONSTREAMING_PARSER_H_INCLUDED

#include "cpl_json_streaming_parser.h"

#include <json.h>  // JSON-C

/************************************************************************/
/*                      OGRJSONCollectionStreamingParser                */
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

  public:
    OGRJSONCollectionStreamingParser(bool bFirstPass, bool bStoreNativeData,
                                     size_t nMaxObjectSize);
    ~OGRJSONCollectionStreamingParser();

    virtual void String(const char * /*pszValue*/, size_t) override;
    virtual void Number(const char * /*pszValue*/, size_t) override;
    virtual void Boolean(bool b) override;
    virtual void Null() override;

    virtual void StartObject() override;
    virtual void EndObject() override;
    virtual void StartObjectMember(const char * /*pszKey*/, size_t) override;

    virtual void StartArray() override;
    virtual void EndArray() override;
    virtual void StartArrayMember() override;

    virtual void Exception(const char * /*pszMessage*/) override;

    json_object *StealRootObject();

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
