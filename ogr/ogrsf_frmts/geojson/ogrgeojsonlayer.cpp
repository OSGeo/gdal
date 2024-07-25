/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONLayer class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>

#if !DEBUG_JSON
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif
#endif  // !DEBUG_VERBOSE

#include <json.h>

#if !DEBUG_JSON
#ifdef __clang
#pragma clang diagnostic pop
#endif
#endif  // !DEBUG_VERBOSE

#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"

/************************************************************************/
/*                       STATIC MEMBERS DEFINITION                      */
/************************************************************************/

const char *const OGRGeoJSONLayer::DefaultName = "OGRGeoJSON";
const OGRwkbGeometryType OGRGeoJSONLayer::DefaultGeometryType = wkbUnknown;

/************************************************************************/
/*                           OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::OGRGeoJSONLayer(const char *pszName,
                                 OGRSpatialReference *poSRSIn,
                                 OGRwkbGeometryType eGType,
                                 OGRGeoJSONDataSource *poDS,
                                 OGRGeoJSONReader *poReader)
    : OGRMemLayer(pszName, poSRSIn, eGType), poDS_(poDS), poReader_(poReader),
      bHasAppendedFeatures_(false), bOriginalIdModified_(false),
      nTotalFeatureCount_(0)
{
    SetAdvertizeUTF8(true);
    SetUpdatable(poDS->IsUpdatable());
}

/************************************************************************/
/*                          ~OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::~OGRGeoJSONLayer()
{
    TerminateAppendSession();
    delete poReader_;
}

/************************************************************************/
/*                      TerminateAppendSession()                        */
/************************************************************************/

void OGRGeoJSONLayer::TerminateAppendSession()
{
    if (bHasAppendedFeatures_)
    {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        VSILFILE *fp = poReader_->GetFP();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        VSIFPrintfL(fp, "\n]\n}\n");
        VSIFFlushL(fp);
        bHasAppendedFeatures_ = false;
    }
}

/************************************************************************/
/*                           GetFIDColumn                               */
/************************************************************************/

const char *OGRGeoJSONLayer::GetFIDColumn()
{
    return sFIDColumn_.c_str();
}

/************************************************************************/
/*                           SetFIDColumn                               */
/************************************************************************/

void OGRGeoJSONLayer::SetFIDColumn(const char *pszFIDColumn)
{
    sFIDColumn_ = pszFIDColumn;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoJSONLayer::ResetReading()
{
    nFeatureReadSinceReset_ = 0;
    if (poReader_)
    {
        TerminateAppendSession();
        poReader_->ResetReading();
    }
    else
        OGRMemLayer::ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoJSONLayer::GetNextFeature()
{
    if (poReader_)
    {
        if (bHasAppendedFeatures_)
        {
            ResetReading();
        }
        while (true)
        {
            OGRFeature *poFeature = poReader_->GetNextFeature(this);
            if (poFeature == nullptr)
                return nullptr;
            if ((m_poFilterGeom == nullptr ||
                 FilterGeometry(
                     poFeature->GetGeomFieldRef(m_iGeomFieldFilter))) &&
                (m_poAttrQuery == nullptr ||
                 m_poAttrQuery->Evaluate(poFeature)))
            {
                nFeatureReadSinceReset_++;
                return poFeature;
            }
            delete poFeature;
        }
    }
    else
    {
        auto ret = OGRMemLayer::GetNextFeature();
        if (ret)
        {
            nFeatureReadSinceReset_++;
        }
        return ret;
    }
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRGeoJSONLayer::GetFeatureCount(int bForce)
{
    if (poReader_)
    {
        if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr &&
            nTotalFeatureCount_ >= 0)
        {
            return nTotalFeatureCount_;
        }
        return OGRLayer::GetFeatureCount(bForce);
    }
    else
    {
        return OGRMemLayer::GetFeatureCount(bForce);
    }
}

/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature *OGRGeoJSONLayer::GetFeature(GIntBig nFID)
{
    if (poReader_)
    {
        if (!IsUpdatable())
        {
            return poReader_->GetFeature(this, nFID);
        }
        return OGRLayer::GetFeature(nFID);
    }
    else
    {
        return OGRMemLayer::GetFeature(nFID);
    }
}

/************************************************************************/
/*                             IngestAll()                              */
/************************************************************************/

bool OGRGeoJSONLayer::IngestAll()
{
    if (poReader_)
    {
        TerminateAppendSession();

        OGRGeoJSONReader *poReader = poReader_;
        poReader_ = nullptr;

        nTotalFeatureCount_ = -1;
        bool bRet = poReader->IngestAll(this);
        delete poReader;
        return bRet;
    }
    else
    {
        return true;
    }
}

/************************************************************************/
/*                     SetOrUpdateFeaturePreparation()                  */
/************************************************************************/

bool OGRGeoJSONLayer::SetOrUpdateFeaturePreparation()
{
    if (!IsUpdatable())
        return false;
    if (poReader_)
    {
        auto nNextIndex = nFeatureReadSinceReset_;
        if (!IngestAll())
            return false;
        SetNextByIndex(nNextIndex);
    }
    return true;
}

/************************************************************************/
/*                           ISetFeature()                              */
/************************************************************************/

OGRErr OGRGeoJSONLayer::ISetFeature(OGRFeature *poFeature)
{
    if (!SetOrUpdateFeaturePreparation())
        return OGRERR_FAILURE;
    return OGRMemLayer::ISetFeature(poFeature);
}

/************************************************************************/
/*                         IUpdateFeature()                             */
/************************************************************************/

OGRErr OGRGeoJSONLayer::IUpdateFeature(OGRFeature *poFeature,
                                       int nUpdatedFieldsCount,
                                       const int *panUpdatedFieldsIdx,
                                       int nUpdatedGeomFieldsCount,
                                       const int *panUpdatedGeomFieldsIdx,
                                       bool bUpdateStyleString)
{
    if (!SetOrUpdateFeaturePreparation())
        return OGRERR_FAILURE;
    return OGRMemLayer::IUpdateFeature(
        poFeature, nUpdatedFieldsCount, panUpdatedFieldsIdx,
        nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx, bUpdateStyleString);
}

/************************************************************************/
/*                         ICreateFeature()                             */
/************************************************************************/

OGRErr OGRGeoJSONLayer::ICreateFeature(OGRFeature *poFeature)
{
    if (!IsUpdatable())
        return OGRERR_FAILURE;
    if (poReader_)
    {
        bool bTryEasyAppend = true;
        while (true)
        {
            // We can trivially append to end of existing file, provided the
            // following conditions are met:
            // * the "features" array member is the last one of the main
            //   object (poReader_->CanEasilyAppend())
            // * there is no "bbox" at feature collection level (could possibly
            //   be supported)
            // * the features have no explicit FID field, so it is trivial to
            //   derive the FID of newly created features without collision
            // * we know the total number of existing features
            if (bTryEasyAppend && poReader_->CanEasilyAppend() &&
                !poReader_->FCHasBBOX() && sFIDColumn_.empty() &&
                GetLayerDefn()->GetFieldIndex("id") < 0 &&
                nTotalFeatureCount_ >= 0)
            {
                VSILFILE *fp = poReader_->GetFP();
                if (!bHasAppendedFeatures_)
                {
                    // Locate "} ] }" (or "[ ] }") pattern at end of file
                    VSIFSeekL(fp, 0, SEEK_END);
                    vsi_l_offset nOffset = VSIFTellL(fp);
                    nOffset -= 10;
                    VSIFSeekL(fp, nOffset, SEEK_SET);
                    char szBuffer[11];
                    VSIFReadL(szBuffer, 10, 1, fp);
                    szBuffer[10] = 0;
                    int i = 9;
                    // Locate final }
                    while (isspace(static_cast<unsigned char>(szBuffer[i])) &&
                           i > 0)
                        i--;
                    if (szBuffer[i] != '}')
                    {
                        bTryEasyAppend = false;
                        continue;
                    }
                    if (i > 0)
                        i--;
                    // Locate ']' ending features array
                    while (isspace(static_cast<unsigned char>(szBuffer[i])) &&
                           i > 0)
                        i--;
                    if (szBuffer[i] != ']')
                    {
                        bTryEasyAppend = false;
                        continue;
                    }
                    if (i > 0)
                        i--;
                    while (isspace(static_cast<unsigned char>(szBuffer[i])) &&
                           i > 0)
                        i--;
                    // Locate '}' ending last feature, or '[' starting features
                    // array
                    if (szBuffer[i] != '}' && szBuffer[i] != '[')
                    {
                        bTryEasyAppend = false;
                        continue;
                    }
                    bool bExistingFeature = szBuffer[i] == '}';
                    nOffset += i + 1;
                    VSIFSeekL(fp, nOffset, SEEK_SET);
                    if (bExistingFeature)
                    {
                        VSIFPrintfL(fp, ",");
                    }
                    VSIFPrintfL(fp, "\n");
                    bHasAppendedFeatures_ = true;
                }
                else
                {
                    VSIFPrintfL(fp, ",\n");
                }
                json_object *poObj =
                    OGRGeoJSONWriteFeature(poFeature, oWriteOptions_);
                VSIFPrintfL(fp, "%s", json_object_to_json_string(poObj));
                json_object_put(poObj);

                if (poFeature->GetFID() == OGRNullFID)
                {
                    poFeature->SetFID(nTotalFeatureCount_);
                }
                nTotalFeatureCount_++;

                return OGRERR_NONE;
            }
            else if (IngestAll())
            {
                break;
            }
            else
            {
                return OGRERR_FAILURE;
            }
        }
    }
    return OGRMemLayer::ICreateFeature(poFeature);
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRGeoJSONLayer::DeleteFeature(GIntBig nFID)
{
    if (!IsUpdatable() || !IngestAll())
        return OGRERR_FAILURE;
    return OGRMemLayer::DeleteFeature(nFID);
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr OGRGeoJSONLayer::CreateField(const OGRFieldDefn *poField, int bApproxOK)
{
    if (!IsUpdatable() || !IngestAll())
        return OGRERR_FAILURE;
    return OGRMemLayer::CreateField(poField, bApproxOK);
}

/************************************************************************/
/*                          DeleteField()                               */
/************************************************************************/

OGRErr OGRGeoJSONLayer::DeleteField(int iField)
{
    if (!IsUpdatable() || !IngestAll())
        return OGRERR_FAILURE;
    return OGRMemLayer::DeleteField(iField);
}

/************************************************************************/
/*                          ReorderFields()                             */
/************************************************************************/

OGRErr OGRGeoJSONLayer::ReorderFields(int *panMap)
{
    if (!IsUpdatable() || !IngestAll())
        return OGRERR_FAILURE;
    return OGRMemLayer::ReorderFields(panMap);
}

/************************************************************************/
/*                         AlterFieldDefn()                             */
/************************************************************************/

OGRErr OGRGeoJSONLayer::AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                       int nFlagsIn)
{
    if (!IsUpdatable() || !IngestAll())
        return OGRERR_FAILURE;
    return OGRMemLayer::AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

/************************************************************************/
/*                         CreateGeomField()                            */
/************************************************************************/

OGRErr OGRGeoJSONLayer::CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                        int bApproxOK)
{
    if (!IsUpdatable() || !IngestAll())
        return OGRERR_FAILURE;
    return OGRMemLayer::CreateGeomField(poGeomField, bApproxOK);
}

OGRErr OGRGeoJSONLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return GetExtent(0, psExtent, bForce);
}

OGRErr OGRGeoJSONLayer::GetExtent(int iGeomField, OGREnvelope *psExtent,
                                  int bForce)
{
    if (iGeomField != 0)
    {
        return OGRERR_FAILURE;
    }

    if (poReader_ && poReader_->ExtentRead() &&
        TestCapability(OLCFastGetExtent))
    {
        *psExtent = poReader_->GetExtent3D();
        return OGRERR_NONE;
    }
    else
    {
        return OGRMemLayer::GetExtentInternal(iGeomField, psExtent, bForce);
    }
}

OGRErr OGRGeoJSONLayer::GetExtent3D(int iGeomField, OGREnvelope3D *psExtent3D,
                                    int bForce)
{

    if (iGeomField != 0)
    {
        return OGRERR_FAILURE;
    }

    if (poReader_ && poReader_->ExtentRead() &&
        TestCapability(OLCFastGetExtent3D))
    {
        *psExtent3D = poReader_->GetExtent3D();
        return OGRERR_NONE;
    }
    else
    {
        return OGRMemLayer::GetExtent3D(iGeomField, psExtent3D, bForce);
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCCurveGeometries))
        return FALSE;
    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastGetExtent) ||
             EQUAL(pszCap, OLCFastGetExtent3D))
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;
    return OGRMemLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                           SyncToDisk()                               */
/************************************************************************/

OGRErr OGRGeoJSONLayer::SyncToDisk()
{
    TerminateAppendSession();

    poDS_->FlushCache(false);
    return OGRERR_NONE;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

void OGRGeoJSONLayer::AddFeature(OGRFeature *poFeature)
{
    GIntBig nFID = poFeature->GetFID();

    // Detect potential FID duplicates and make sure they are eventually
    // unique.
    if (-1 == nFID)
    {
        nFID = GetFeatureCount(FALSE);
        OGRFeature *poTryFeature = nullptr;
        while ((poTryFeature = GetFeature(nFID)) != nullptr)
        {
            nFID++;
            delete poTryFeature;
        }
    }
    else
    {
        OGRFeature *poTryFeature = nullptr;
        if ((poTryFeature = GetFeature(nFID)) != nullptr)
        {
            if (!bOriginalIdModified_)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Several features with id = " CPL_FRMT_GIB " have been "
                    "found. Altering it to be unique. This warning will not "
                    "be emitted anymore for this layer",
                    nFID);
                bOriginalIdModified_ = true;
            }
            delete poTryFeature;
            nFID = GetFeatureCount(FALSE);
            while ((poTryFeature = GetFeature(nFID)) != nullptr)
            {
                nFID++;
                delete poTryFeature;
            }
        }
    }
    poFeature->SetFID(nFID);

    if (!CPL_INT64_FITS_ON_INT32(nFID))
        SetMetadataItem(OLMD_FID64, "YES");

    const bool bIsUpdatable = IsUpdatable();
    SetUpdatable(true);  // Temporary toggle on updatable flag.
    CPL_IGNORE_RET_VAL(OGRMemLayer::SetFeature(poFeature));
    SetUpdatable(bIsUpdatable);
    SetUpdated(false);
}

/************************************************************************/
/*                           DetectGeometryType                         */
/************************************************************************/

void OGRGeoJSONLayer::DetectGeometryType()
{
    if (GetLayerDefn()->GetGeomType() != wkbUnknown)
        return;

    ResetReading();
    bool bFirstGeometry = true;
    OGRwkbGeometryType eLayerGeomType = wkbUnknown;
    for (const auto &poFeature : *this)
    {
        const OGRGeometry *poGeometry = poFeature->GetGeometryRef();
        if (nullptr != poGeometry)
        {
            OGRwkbGeometryType eGeomType = poGeometry->getGeometryType();
            if (!OGRGeoJSONUpdateLayerGeomType(bFirstGeometry, eGeomType,
                                               eLayerGeomType))
            {
                break;
            }
        }
    }

    {
        auto poFDefn = GetLayerDefn();
        auto oTemporaryUnsealer(poFDefn->GetTemporaryUnsealer());
        poFDefn->SetGeomType(eLayerGeomType);
    }

    ResetReading();
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRGeoJSONLayer::GetDataset()
{
    return poDS_;
}
