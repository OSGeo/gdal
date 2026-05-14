/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101Reader
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"
#include "ogrs101featurecatalog.h"
#include "ogrs101readerconstants.h"

#include <algorithm>
#include <memory>

/************************************************************************/
/*                   CreateFeatureTypeFeatureDefns()                    */
/************************************************************************/

/** Create the feature definitions for the various kinds of Feature Type records
 */
bool OGRS101Reader::CreateFeatureTypeFeatureDefns()
{
    const int nCount = m_oFeatureTypeRecordIndex.GetCount();

    struct FeatureTypeDef
    {
        int nMaxMaskCount = 0;
        bool bMultiSpatialAssociations = false;
        bool bPromotedToMultiPointFromPoint = false;
        std::vector<int> anRecordIdx{};
    };

    std::map<FeatureTypeKey, FeatureTypeDef> oMapFeatureClassAndGeomType;

    // First pass to collect all (feature type code, geometry type, CRS Id) triples
    for (int iRecord = 0; iRecord < nCount; ++iRecord)
    {
        const auto poRecord = m_oFeatureTypeRecordIndex.GetByIndex(iRecord);
        FeatureTypeKey key;
        key.nFeatureTypeCode =
            poRecord->GetIntSubfield(FRID_FIELD, 0, NFTC_SUBFIELD, 0);

        const auto NormalizeCurve = [](RecordName name)
        {
            return name == RECORD_NAME_COMPOSITE_CURVE ? RECORD_NAME_CURVE
                                                       : name;
        };

        bool bMultiSpatialAssociations = false;
        // S-101 page 19: A feature may reference multiple geometries
        // but must only reference geometries of a single
        // geometric primitive (point, pointset, curve or surface).
        const auto apoSPASFields = poRecord->GetFields(SPAS_FIELD);
        int nSPASCount = 0;
        bool bHeterogeneous = false;
        for (int iSPASField = 0;
             iSPASField < static_cast<int>(apoSPASFields.size()); ++iSPASField)
        {
            const auto poSPASField = apoSPASFields[iSPASField];
            if (iSPASField == 0)
            {
                key.nGeometryType = NormalizeCurve(
                    poRecord->GetIntSubfield(poSPASField, RRNM_SUBFIELD, 0));
            }

            const int nSPASCountThisIter = poSPASField->GetRepeatCount();
            nSPASCount += nSPASCountThisIter;
            for (int iSPAS = 0; iSPAS < nSPASCountThisIter; ++iSPAS)
            {
                if (NormalizeCurve(poRecord->GetIntSubfield(
                        poSPASField, RRNM_SUBFIELD, iSPAS)) !=
                    key.nGeometryType)
                {
                    bHeterogeneous = true;
                    break;
                }
            }
        }
        if (bHeterogeneous)
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Record index %d of FRID: has %d "
                    "spatial associations with at least 2 "
                    "not being of the same geometry type%s",
                    iRecord, nSPASCount, m_bStrict ? "" : ". Ignoring it")))
            {
                return false;
            }
            continue;
        }
        else if (nSPASCount > 1)
        {
            bMultiSpatialAssociations = true;
        }

        if (key.nGeometryType == RECORD_NAME_POINT)
        {
            const int nRRID =
                poRecord->GetIntSubfield(SPAS_FIELD, 0, RRID_SUBFIELD, 0);
            const auto poGeomRecord = m_oPointRecordIndex.FindRecord(nRRID);
            if (!poGeomRecord && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                     "Record index %d of FRID: Point of id %d "
                                     "does not exist",
                                     iRecord, nRRID)))
            {
                return false;
            }

            const auto nCRSId =
                poGeomRecord ? GetCRSIdForPointRecord(poGeomRecord, -1, nRRID)
                             : HORIZONTAL_CRS_ID;
            if (nCRSId == INVALID_CRS_ID)
            {
                // Error already emitted
                if (m_bStrict)
                    return false;
                continue;
            }
            key.nCRSId = nCRSId;
        }
        else if (key.nGeometryType == RECORD_NAME_MULTIPOINT)
        {
            const int nRRID =
                poRecord->GetIntSubfield(SPAS_FIELD, 0, RRID_SUBFIELD, 0);
            const auto poGeomRecord =
                m_oMultiPointRecordIndex.FindRecord(nRRID);
            if (!poGeomRecord &&
                !EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("Record index %d of FRID: MultiPoint of id %d "
                               "does not exist",
                               iRecord, nRRID)))
            {
                return false;
            }

            const auto nCRSId =
                poGeomRecord
                    ? GetCRSIdForMultiPointRecord(poGeomRecord, -1, nRRID)
                    : HORIZONTAL_CRS_ID;
            if (nCRSId == INVALID_CRS_ID)
            {
                // Error already emitted
                if (m_bStrict)
                    return false;
                continue;
            }
            key.nCRSId = nCRSId;
        }
        else if (!apoSPASFields.empty())
            key.nCRSId = HORIZONTAL_CRS_ID;

        const bool bPromotedToMultiPointFromPoint =
            (key.nGeometryType == RECORD_NAME_POINT &&
             bMultiSpatialAssociations);
        if (bPromotedToMultiPointFromPoint)
            key.nGeometryType = RECORD_NAME_MULTIPOINT;

        auto &featureTypeDef = oMapFeatureClassAndGeomType[key];
        if (bPromotedToMultiPointFromPoint)
        {
            featureTypeDef.bPromotedToMultiPointFromPoint = true;
        }
        else if (bMultiSpatialAssociations)
        {
            featureTypeDef.bMultiSpatialAssociations = true;
        }

        int nMaskCount = 0;
        for (const auto poField : poRecord->GetFields(MASK_FIELD))
        {
            nMaskCount += poField->GetRepeatCount();
        }
        featureTypeDef.nMaxMaskCount =
            std::max(featureTypeDef.nMaxMaskCount, nMaskCount);

        featureTypeDef.anRecordIdx.push_back(iRecord);
    }

    for (auto &[key, def] : oMapFeatureClassAndGeomType)
    {
        std::string osLayerCode;
        std::string osName;
        const auto oIter = m_featureTypeCodes.find(key.nFeatureTypeCode);
        if (oIter != m_featureTypeCodes.end())
        {
            osLayerCode = oIter->second;
            osName = osLayerCode;
        }
        else
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Features pointing at unknown feature type code %d",
                    static_cast<int>(key.nFeatureTypeCode))))
            {
                return false;
            }
            osName = CPLSPrintf("unknownFeatureType%d",
                                static_cast<int>(key.nFeatureTypeCode));
        }

        const auto oIterSRS = m_oMapSRS.find(key.nCRSId);
        CPLAssert(key.nCRSId == INVALID_CRS_ID || oIterSRS != m_oMapSRS.end());
        const OGRSpatialReference *poSRS =
            key.nCRSId != INVALID_CRS_ID ? &(oIterSRS->second) : nullptr;
        const bool bIs2D = key.nCRSId == HORIZONTAL_CRS_ID;

        OGRwkbGeometryType eGeomType = wkbNone;
        const char *pszExpectedPermittedPrimitive = nullptr;
        switch (static_cast<int>(key.nGeometryType))
        {
            case static_cast<int>(PSEUDO_RECORD_NAME_NO_GEOM):
            {
                osName += "_NoGeom";
                pszExpectedPermittedPrimitive =
                    OGRS101FeatureCatalog::PERMITTED_PRIMITIVE_NO_GEOMETRY;
                break;
            }

            case static_cast<int>(RECORD_NAME_POINT):
            {
                CPLAssert(poSRS);
                osName += '_';
                if (def.bMultiSpatialAssociations)
                {
                    osName += GetMultiPointLayerName(*poSRS);
                    eGeomType = bIs2D ? wkbMultiPoint : wkbMultiPoint25D;
                }
                else
                {
                    osName += GetPointLayerName(*poSRS);
                    eGeomType = bIs2D ? wkbPoint : wkbPoint25D;
                }
                pszExpectedPermittedPrimitive =
                    OGRS101FeatureCatalog::PERMITTED_PRIMITIVE_POINT;
                break;
            }

            case static_cast<int>(RECORD_NAME_MULTIPOINT):
            {
                CPLAssert(poSRS);
                osName += '_';
                if (def.bMultiSpatialAssociations)
                {
                    osName += "CollectionOfMultiPoint";
                    eGeomType = wkbGeometryCollection;
                }
                else
                {
                    osName += GetMultiPointLayerName(*poSRS);
                    eGeomType = bIs2D ? wkbMultiPoint : wkbMultiPoint25D;
                }
                pszExpectedPermittedPrimitive =
                    OGRS101FeatureCatalog::PERMITTED_PRIMITIVE_POINTSET;
                break;
            }

            case static_cast<int>(RECORD_NAME_CURVE):
            {
                if (def.bMultiSpatialAssociations)
                {
                    osName += "_MultiLine";
                    eGeomType = wkbMultiLineString;
                }
                else
                {
                    osName += "_Line";
                    eGeomType = wkbLineString;
                }
                pszExpectedPermittedPrimitive =
                    OGRS101FeatureCatalog::PERMITTED_PRIMITIVE_CURVE;
                break;
            }

            case static_cast<int>(RECORD_NAME_SURFACE):
            {
                if (def.bMultiSpatialAssociations)
                {
                    osName += "_MultiPolygon";
                    eGeomType = wkbMultiPolygon;
                }
                else
                {
                    osName += "_Polygon";
                    eGeomType = wkbPolygon;
                }
                pszExpectedPermittedPrimitive =
                    OGRS101FeatureCatalog::PERMITTED_PRIMITIVE_SURFACE;
                break;
            }

            default:
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Features pointing at unknown spatial record type %d",
                        static_cast<int>(key.nGeometryType))))
                {
                    return false;
                }
                osName += "_UnknownGeomType";
                osName += std::to_string(static_cast<int>(key.nGeometryType));
                break;
            }
        }

        const OGRS101FeatureCatalog::FeatureType *psFeatureType = nullptr;
        if (m_poFeatureCatalog)
        {
            const auto oIterFT =
                m_poFeatureCatalog->GetFeatureTypes().find(osLayerCode);
            if (oIterFT != m_poFeatureCatalog->GetFeatureTypes().end())
            {
                psFeatureType = &(oIterFT->second);
            }
            else if (!cpl::starts_with(osLayerCode, "FeatureType") &&
                     !EMIT_ERROR_OR_WARNING(
                         CPLSPrintf("Feature type %s is not referenced in the "
                                    "feature catalog",
                                    osLayerCode.c_str())))
            {
                return false;
            }
        }

        if (pszExpectedPermittedPrimitive && psFeatureType &&
            !cpl::contains(psFeatureType->permittedPrimitives,
                           pszExpectedPermittedPrimitive) &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "Features of %s contain records with primitive %s, whereas "
                "it is not allowed by the feature catalog",
                osLayerCode.c_str(), pszExpectedPermittedPrimitive)))
        {
            return false;
        }

        auto poFDefn =
            OGRFeatureDefnRefCountedPtr::makeInstance(osName.c_str());
        poFDefn->SetGeomType(eGeomType);
        for (const char *pszOGRFieldName :
             {OGR_FIELD_NAME_RECORD_ID, OGR_FIELD_NAME_RECORD_VERSION,
              OGR_FIELD_NAME_AGEN, OGR_FIELD_NAME_FIDN, OGR_FIELD_NAME_FIDS})
        {
            OGRFieldDefn oFieldDefn(pszOGRFieldName, OFTInteger);
            poFDefn->AddFieldDefn(&oFieldDefn);
        }

        if (eGeomType != wkbNone)
        {
            CPLAssert(poSRS);
            poFDefn->GetGeomFieldDefn(0)->SetSpatialRef(
                OGRSpatialReferenceRefCountedPtr::makeClone(poSRS).get());
            const bool bList = def.bMultiSpatialAssociations ||
                               def.bPromotedToMultiPointFromPoint;

            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_GEOMETRY_LAYER_NAME,
                                        bList ? OFTStringList : OFTString);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
            for (const char *pszOGRFieldName :
                 {OGR_FIELD_NAME_GEOMETRY_RECORD_ID})
            {
                OGRFieldDefn oFieldDefn(pszOGRFieldName,
                                        bList ? OFTIntegerList : OFTInteger);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
            if (eGeomType == wkbLineString || eGeomType == wkbMultiLineString)
            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_GEOMETRY_ORIENTATION,
                                        bList ? OFTStringList : OFTString);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
            for (const char *pszOGRFieldName :
                 {OGR_FIELD_NAME_SMIN, OGR_FIELD_NAME_SMAX})
            {
                OGRFieldDefn oFieldDefn(pszOGRFieldName,
                                        bList ? OFTIntegerList : OFTInteger);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
        }

        for (const char *pszAttrFieldName :
             {ATTR_FIELD, INAS_FIELD, FASC_FIELD})
        {
            if (!InferFeatureDefn(
                    m_oFeatureTypeRecordIndex, FRID_FIELD, pszAttrFieldName,
                    def.anRecordIdx, *poFDefn, m_oMapFieldDomains, nullptr,
                    strcmp(pszAttrFieldName, ATTR_FIELD) == 0 ? psFeatureType
                                                              : nullptr))
            {
                return false;
            }
        }

        if (def.nMaxMaskCount == 1)
        {
            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_MASK_LAYER_NAME,
                                        OFTString);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_MASK_RECORD_ID,
                                        OFTInteger);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_MASK_INDICATOR,
                                        OFTString);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }

            OGRGeomFieldDefn oGeomFieldDefn("maskGeometry", wkbLineString);
            oGeomFieldDefn.SetSpatialRef(
                OGRSpatialReferenceRefCountedPtr::makeClone(
                    &(m_oMapSRS[HORIZONTAL_CRS_ID]))
                    .get());
            poFDefn->AddGeomFieldDefn(&oGeomFieldDefn);
        }
        else if (def.nMaxMaskCount > 1)
        {
            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_MASK_LAYER_NAME,
                                        OFTStringList);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_MASK_RECORD_ID,
                                        OFTIntegerList);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }
            {
                OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_MASK_INDICATOR,
                                        OFTStringList);
                poFDefn->AddFieldDefn(&oFieldDefn);
            }

            OGRGeomFieldDefn oGeomFieldDefn("maskGeometry", wkbMultiLineString);
            oGeomFieldDefn.SetSpatialRef(
                OGRSpatialReferenceRefCountedPtr::makeClone(
                    &(m_oMapSRS[HORIZONTAL_CRS_ID]))
                    .get());
            poFDefn->AddGeomFieldDefn(&oGeomFieldDefn);
        }

        for (int nRecordIdx : def.anRecordIdx)
        {
            const int nRCID =
                m_oFeatureTypeRecordIndex.GetByIndex(nRecordIdx)
                    ->GetIntSubfield(FRID_FIELD, 0, RCID_SUBFIELD, 0);
            m_oMapFeatureTypeIdToFDefn[nRCID] = poFDefn.get();
        }

        LayerDef layerDef;
        layerDef.poFeatureDefn = std::move(poFDefn);
        if (psFeatureType)
        {
            layerDef.osName = psFeatureType->name;
            layerDef.osDefinition = psFeatureType->definition;
            layerDef.osAlias = psFeatureType->alias;
        }
        layerDef.anRecordIndices = std::move(def.anRecordIdx);
        m_oMapFeatureKeyToLayerDef[key] = std::move(layerDef);
    }

    return true;
}

/************************************************************************/
/*                            ReadGeometry()                            */
/************************************************************************/

template <typename T, typename GeomReaderMethodType>
bool OGRS101Reader::ReadGeometry(
    const DDFRecordIndex &oIndex, const char *pszErrorContext,
    int nGeomRecordID, const char *pszGeomType, bool bReverse,
    OGRFeature &oFeature, std::unique_ptr<OGRGeometryCollection> &poMultiGeom,
    GeomReaderMethodType geomReaderMethod, int iGeomField) const
{
    const auto poGeomRecord = oIndex.FindRecord(nGeomRecordID);

    if (!poGeomRecord && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                             "%s: %s of ID=%d does not exist", pszErrorContext,
                             pszGeomType, nGeomRecordID)))
    {
        return false;
    }

    const auto poGeomFieldDefn =
        oFeature.GetDefnRef()->GetGeomFieldDefn(iGeomField);
    const OGRSpatialReference *poSRS = poGeomFieldDefn->GetSpatialRef();
    std::unique_ptr<T> poGeom;
    if (poGeomRecord)
    {
        poGeom = (this->*geomReaderMethod)(poGeomRecord, /* index = */ -1,
                                           nGeomRecordID, poSRS);
    }
    if (!poGeom)
    {
        if (poGeomRecord && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "%s: %s of ID=%d is invalid", pszErrorContext,
                                pszGeomType, nGeomRecordID)))
        {
            return false;
        }
    }
    else if constexpr (std::is_same_v<T, OGRLineString>)
    {
        if (bReverse)
            poGeom->reversePoints();
    }

    const auto eGeomType = poGeomFieldDefn->GetType();

    if (wkbFlatten(eGeomType) == wkbGeometryCollection ||
        (!std::is_same_v<T, OGRMultiPoint> &&
         OGR_GT_IsSubClassOf(eGeomType, wkbGeometryCollection)))
    {
        if (!poMultiGeom)
        {
            poMultiGeom = std::make_unique<typename T::MultiType>();
            poMultiGeom->assignSpatialReference(poSRS);
        }
        poMultiGeom->addGeometry(poGeom ? std::move(poGeom)
                                        : std::make_unique<T>());
    }
    else
    {
        oFeature.SetGeomField(iGeomField, std::move(poGeom));
    }

    return true;
}

/************************************************************************/
/*                      FillFeatureTypeGeometry()                       */
/************************************************************************/

/** Fill the geometry of the provided feature from the identified record
 * (of m_oFeatureTypeRecordIndex).
 */
bool OGRS101Reader::FillFeatureTypeGeometry(const DDFRecord *poRecord,
                                            int iRecord,
                                            OGRFeature &oFeature) const
{
    // Process geometry (several spatial association per feature possible)
    CPLStringList aosLayerNames, aosOrientations;
    std::vector<int> anRRID, anSMIN, anSMAX;
    std::unique_ptr<OGRGeometryCollection> poMultiGeom;

    const auto apoSPASFields = poRecord->GetFields(SPAS_FIELD);
    for (const auto &poSPASField : apoSPASFields)
    {
        const int nSPASCount = poSPASField->GetRepeatCount();

        for (int iSPAS = 0; iSPAS < nSPASCount; ++iSPAS)
        {
            const auto GetIntSubfield =
                [poRecord, poSPASField, iSPAS](const char *pszSubFieldName)
            {
                return poRecord->GetIntSubfield(poSPASField, pszSubFieldName,
                                                iSPAS);
            };

            const std::string osErrorContext =
                CPLSPrintf("Feature type record index %d, SPAS instance %d",
                           iRecord, iSPAS);

            const int nSAUI = GetIntSubfield(SAUI_SUBFIELD);
            if (nSAUI != INSTRUCTION_INSERT)
            {
                if (!EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("%s: SAUI value %d is invalid",
                                   osErrorContext.c_str(), nSAUI)))
                {
                    return false;
                }
            }

            const RecordName nRRNM = GetIntSubfield(RRNM_SUBFIELD);

            const int nRRID = GetIntSubfield(RRID_SUBFIELD);

            const int nORNT = GetIntSubfield(ORNT_SUBFIELD);

            const bool bIsLine = nRRNM == RECORD_NAME_CURVE ||
                                 nRRNM == RECORD_NAME_COMPOSITE_CURVE;
            switch (nORNT)
            {
                case ORNT_FORWARD:
                {
                    break;
                }

                case ORNT_REVERSE:
                {
                    if (!bIsLine)
                    {
                        if (!EMIT_ERROR_OR_WARNING(
                                CPLSPrintf("%s: "
                                           "ORNT = Reverse is invalid for "
                                           "non-curve geometry",
                                           osErrorContext.c_str())))
                        {
                            return false;
                        }
                    }
                    break;
                }

                case ORNT_NULL:
                {
                    if (bIsLine)
                    {
                        if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "%s: ORNT = Null is invalid for curve geometry",
                                osErrorContext.c_str())))
                        {
                            return false;
                        }
                    }
                    break;
                }

                default:
                {
                    if (!EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("%s: ORNT = %d is invalid",
                                       osErrorContext.c_str(), nORNT)))
                    {
                        return false;
                    }
                    break;
                }
            }

            bool bInvalidType = false;
            switch (static_cast<int>(nRRNM))
            {
                case static_cast<int>(RECORD_NAME_POINT):
                {
                    if (!ReadGeometry<OGRPoint>(
                            m_oPointRecordIndex, osErrorContext.c_str(), nRRID,
                            "Point", false, oFeature, poMultiGeom,
                            &OGRS101Reader::ReadPointGeometry))
                    {
                        return false;
                    }

                    const auto poGeomFieldDefn =
                        oFeature.GetDefnRef()->GetGeomFieldDefn(0);
                    CPLAssert(poGeomFieldDefn);
                    const OGRSpatialReference *poSRS =
                        poGeomFieldDefn->GetSpatialRef();
                    CPLAssert(poSRS);

                    aosLayerNames.push_back(GetPointLayerName(*poSRS).c_str());

                    break;
                }

                case static_cast<int>(RECORD_NAME_MULTIPOINT):
                {
                    if (!ReadGeometry<OGRMultiPoint>(
                            m_oMultiPointRecordIndex, osErrorContext.c_str(),
                            nRRID, "MultiPoint", false, oFeature, poMultiGeom,
                            &OGRS101Reader::ReadMultiPointGeometry))
                    {
                        return false;
                    }

                    const auto poGeomFieldDefn =
                        oFeature.GetDefnRef()->GetGeomFieldDefn(0);
                    CPLAssert(poGeomFieldDefn);
                    const OGRSpatialReference *poSRS =
                        poGeomFieldDefn->GetSpatialRef();
                    CPLAssert(poSRS);

                    aosLayerNames.push_back(
                        GetMultiPointLayerName(*poSRS).c_str());

                    break;
                }

                case static_cast<int>(RECORD_NAME_CURVE):
                case static_cast<int>(RECORD_NAME_COMPOSITE_CURVE):
                {
                    const char *pszLayerName =
                        nRRNM == RECORD_NAME_CURVE
                            ? OGR_LAYER_NAME_CURVE
                            : OGR_LAYER_NAME_COMPOSITE_CURVE;

                    bool ret;
                    if (nRRNM == RECORD_NAME_CURVE)
                    {
                        ret = ReadGeometry<OGRLineString>(
                            m_oCurveRecordIndex, osErrorContext.c_str(), nRRID,
                            pszLayerName, nORNT == ORNT_REVERSE, oFeature,
                            poMultiGeom, &OGRS101Reader::ReadCurveGeometry);
                    }
                    else
                    {
                        ret = ReadGeometry<OGRLineString>(
                            m_oCompositeCurveRecordIndex,
                            osErrorContext.c_str(), nRRID, pszLayerName,
                            nORNT == ORNT_REVERSE, oFeature, poMultiGeom,
                            &OGRS101Reader::ReadCompositeCurveGeometry);
                    }

                    if (!ret)
                    {
                        return false;
                    }

                    aosLayerNames.push_back(pszLayerName);
                    aosOrientations.push_back(
                        nORNT == ORNT_FORWARD ? "forward" : "reverse");

                    break;
                }

                case static_cast<int>(RECORD_NAME_SURFACE):
                {
                    if (!ReadGeometry<OGRPolygon>(
                            m_oSurfaceRecordIndex, osErrorContext.c_str(),
                            nRRID, OGR_LAYER_NAME_SURFACE, false, oFeature,
                            poMultiGeom, &OGRS101Reader::ReadSurfaceGeometry))
                    {
                        return false;
                    }

                    aosLayerNames.push_back(OGR_LAYER_NAME_SURFACE);

                    break;
                }

                default:
                    bInvalidType = true;
                    break;
            }

            if (bInvalidType)
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf("%s: "
                                                      "Invalid RRNM = %d",
                                                      osErrorContext.c_str(),
                                                      static_cast<int>(nRRNM))))
                {
                    return false;
                }
            }
            else
            {
                anRRID.push_back(nRRID);

                anSMIN.push_back(GetIntSubfield(SMIN_SUBFIELD));

                anSMAX.push_back(GetIntSubfield(SMAX_SUBFIELD));
            }
        }
    }

    if (!apoSPASFields.empty())
    {
        CPLAssert(anRRID.size() == anSMIN.size());
        CPLAssert(anRRID.size() == anSMAX.size());
        CPLAssert(anRRID.size() == static_cast<size_t>(aosLayerNames.size()));
        CPLAssert(aosOrientations.empty() ||
                  anRRID.size() == static_cast<size_t>(aosOrientations.size()));

        if (poMultiGeom)
        {
            oFeature.SetGeometry(std::move(poMultiGeom));

            oFeature.SetField(OGR_FIELD_NAME_GEOMETRY_LAYER_NAME,
                              aosLayerNames.List());
            if (!aosOrientations.empty())
            {
                oFeature.SetField(OGR_FIELD_NAME_GEOMETRY_ORIENTATION,
                                  aosOrientations.List());
            }
            oFeature.SetField(OGR_FIELD_NAME_GEOMETRY_RECORD_ID,
                              static_cast<int>(anRRID.size()), anRRID.data());
            if (std::find_if(anSMIN.begin(), anSMIN.end(),
                             [](int x) { return x > 0; }) != anSMIN.end())
            {
                oFeature.SetField(OGR_FIELD_NAME_SMIN,
                                  static_cast<int>(anSMIN.size()),
                                  anSMIN.data());
            }
            if (std::find_if(anSMAX.begin(), anSMAX.end(),
                             [](int x) { return x > 0; }) != anSMAX.end())
            {
                oFeature.SetField(OGR_FIELD_NAME_SMAX,
                                  static_cast<int>(anSMAX.size()),
                                  anSMAX.data());
            }
        }
        else if (!aosLayerNames.empty())
        {
            oFeature.SetField(OGR_FIELD_NAME_GEOMETRY_LAYER_NAME,
                              aosLayerNames[0]);
            oFeature.SetField(OGR_FIELD_NAME_GEOMETRY_RECORD_ID, anRRID[0]);
            if (!aosOrientations.empty())
            {
                oFeature.SetField(OGR_FIELD_NAME_GEOMETRY_ORIENTATION,
                                  aosOrientations[0]);
            }
            if (anSMIN[0] > 0)
                oFeature.SetField(OGR_FIELD_NAME_SMIN, anSMIN[0]);
            if (anSMAX[0] > 0)
                oFeature.SetField(OGR_FIELD_NAME_SMAX, anSMAX[0]);
        }
    }

    return true;
}

/************************************************************************/
/*                        FillFeatureTypeMask()                         */
/************************************************************************/

/** Fill the mask info of the provided feature from the identified record
 * (of m_oFeatureTypeRecordIndex).
 */
bool OGRS101Reader::FillFeatureTypeMask(const DDFRecord *poRecord, int iRecord,
                                        OGRFeature &oFeature) const
{
    std::unique_ptr<OGRGeometryCollection> poMultiGeom;

    std::vector<int> anRRID;
    CPLStringList aosLayerNames;
    CPLStringList aosMaskIndicators;

    constexpr int MASK_GEOM_FIELD_IDX = 1;

    const auto apoMASKFields = poRecord->GetFields(MASK_FIELD);
    for (const auto *poMASKField : apoMASKFields)
    {
        const int nMaskCount = poMASKField->GetRepeatCount();

        for (int iMASK = 0; iMASK < nMaskCount; ++iMASK)
        {
            const auto GetIntSubfield =
                [poRecord, poMASKField, iMASK](const char *pszSubFieldName)
            {
                return poRecord->GetIntSubfield(poMASKField, pszSubFieldName,
                                                iMASK);
            };

            const std::string osErrorContext =
                CPLSPrintf("Feature type record index %d, MASK instance %d",
                           iRecord, iMASK);

            const int nMUIN = GetIntSubfield(MUIN_SUBFIELD);
            if (nMUIN != INSTRUCTION_INSERT)
            {
                if (!EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("%s: MUIN value %d is invalid",
                                   osErrorContext.c_str(), nMUIN)))
                {
                    return false;
                }
            }

            const int nMIND = GetIntSubfield(MIND_SUBFIELD);
            constexpr int MASK_INDICATOR_TRUNCATED_BY_DATA_COVERAGE_LIMIT = 1;
            constexpr int MASK_INDICATOR_SUPPRESS_PORTRAYAL = 2;
            if (nMIND == MASK_INDICATOR_TRUNCATED_BY_DATA_COVERAGE_LIMIT)
            {
                aosMaskIndicators.push_back("truncatedByDataCoverageLimit");
            }
            else if (nMIND == MASK_INDICATOR_SUPPRESS_PORTRAYAL)
            {
                aosMaskIndicators.push_back("suppressPortrayal");
            }
            else
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: MIND value %d is invalid. Expected %d or %d",
                        osErrorContext.c_str(), nMIND,
                        MASK_INDICATOR_TRUNCATED_BY_DATA_COVERAGE_LIMIT,
                        MASK_INDICATOR_SUPPRESS_PORTRAYAL)))
                {
                    return false;
                }
                aosMaskIndicators.push_back(CPLSPrintf("unknown%d", nMIND));
            }

            const int nRRID = GetIntSubfield(RRID_SUBFIELD);
            anRRID.push_back(nRRID);

            const RecordName nRRNM = GetIntSubfield(RRNM_SUBFIELD);
            if (nRRNM == RECORD_NAME_CURVE ||
                nRRNM == RECORD_NAME_COMPOSITE_CURVE)
            {
                const char *pszLayerName = nRRNM == RECORD_NAME_CURVE
                                               ? OGR_LAYER_NAME_CURVE
                                               : OGR_LAYER_NAME_COMPOSITE_CURVE;

                bool ret;
                if (nRRNM == RECORD_NAME_CURVE)
                {
                    ret = ReadGeometry<OGRLineString>(
                        m_oCurveRecordIndex, osErrorContext.c_str(), nRRID,
                        pszLayerName, false, oFeature, poMultiGeom,
                        &OGRS101Reader::ReadCurveGeometry, MASK_GEOM_FIELD_IDX);
                }
                else
                {
                    ret = ReadGeometry<OGRLineString>(
                        m_oCompositeCurveRecordIndex, osErrorContext.c_str(),
                        nRRID, pszLayerName, false, oFeature, poMultiGeom,
                        &OGRS101Reader::ReadCompositeCurveGeometry,
                        MASK_GEOM_FIELD_IDX);
                }

                if (!ret)
                {
                    return false;
                }

                aosLayerNames.push_back(pszLayerName);
            }
            else
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: Invalid value for RRNM subfield: "
                        "got %d, expected %d or %d.",
                        osErrorContext.c_str(), static_cast<int>(nRRNM),
                        static_cast<int>(RECORD_NAME_CURVE),
                        static_cast<int>(RECORD_NAME_COMPOSITE_CURVE))))
                {
                    return false;
                }

                aosLayerNames.push_back("");
            }
        }
    }

    if (!aosLayerNames.empty())
    {
        CPLAssert(anRRID.size() == static_cast<size_t>(aosLayerNames.size()));
        CPLAssert(anRRID.size() ==
                  static_cast<size_t>(aosMaskIndicators.size()));

        if (poMultiGeom)
        {
            oFeature.SetGeomField(MASK_GEOM_FIELD_IDX, std::move(poMultiGeom));

            oFeature.SetField(OGR_FIELD_NAME_MASK_LAYER_NAME,
                              aosLayerNames.List());

            oFeature.SetField(OGR_FIELD_NAME_MASK_RECORD_ID,
                              static_cast<int>(anRRID.size()), anRRID.data());

            oFeature.SetField(OGR_FIELD_NAME_MASK_INDICATOR,
                              aosMaskIndicators.List());
        }
        else
        {
            oFeature.SetField(OGR_FIELD_NAME_MASK_LAYER_NAME, aosLayerNames[0]);

            oFeature.SetField(OGR_FIELD_NAME_MASK_RECORD_ID, anRRID[0]);

            oFeature.SetField(OGR_FIELD_NAME_MASK_INDICATOR,
                              aosMaskIndicators[0]);
        }
    }

    return true;
}

/************************************************************************/
/*                       FillFeatureFeatureType()                       */
/************************************************************************/

/** Fill the content of the provided feature from the identified record
 * (of m_oFeatureTypeRecordIndex).
 */
bool OGRS101Reader::FillFeatureFeatureType(const DDFRecordIndex &oIndex,
                                           int iRecord,
                                           OGRFeature &oFeature) const
{
    const auto poRecord = oIndex.GetByIndex(iRecord);
    CPLAssert(poRecord);

    if (const auto poFOIDField = poRecord->FindField(FOID_FIELD))
    {
        const int nAGEN =
            poRecord->GetIntSubfield(poFOIDField, AGEN_SUBFIELD, 0);
        oFeature.SetField(OGR_FIELD_NAME_AGEN, nAGEN);

        const int nFIDN =
            poRecord->GetIntSubfield(poFOIDField, FIDN_SUBFIELD, 0);
        oFeature.SetField(OGR_FIELD_NAME_FIDN, nFIDN);

        const int nFIDS =
            poRecord->GetIntSubfield(poFOIDField, FIDS_SUBFIELD, 0);
        oFeature.SetField(OGR_FIELD_NAME_FIDS, nFIDS);
    }
    else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                 "Feature type record index %d: no FOID field", iRecord)))
    {
        return false;
    }

    // A FRID record might have a ATTR field, a INAS field (pointing to a IRID
    // record), a FASC field (thus pointing to another FRID record), or any
    // combination of the 3.

    return FillFeatureTypeGeometry(poRecord, iRecord, oFeature) &&
           FillFeatureTypeMask(poRecord, iRecord, oFeature) &&
           FillFeatureAttributes(oIndex, iRecord, ATTR_FIELD, oFeature) &&
           FillFeatureAttributes(oIndex, iRecord, INAS_FIELD, oFeature) &&
           FillFeatureAttributes(oIndex, iRecord, FASC_FIELD, oFeature) &&
           FillFeatureWithNonAttrAssocSubfields(poRecord, iRecord, INAS_FIELD,
                                                oFeature) &&
           FillFeatureWithNonAttrAssocSubfields(poRecord, iRecord, FASC_FIELD,
                                                oFeature);
}

/************************************************************************/
/*                   ProcessUpdateRecordFeatureType()                   */
/************************************************************************/

bool OGRS101Reader::ProcessUpdateRecordFeatureType(
    const DDFRecord *poUpdateRecord, DDFRecord *poTargetRecord) const
{
    const auto poIDField = poUpdateRecord->GetField(0);
    CPLAssert(poIDField);
    const char *pszIDFieldName = poIDField->GetFieldDefn()->GetName();
    CPLAssert(pszIDFieldName);

    // Record name
    const RecordName nRCNM =
        poUpdateRecord->GetIntSubfield(pszIDFieldName, 0, RCNM_SUBFIELD, 0);

    // Record identifier
    const int nRCID =
        poUpdateRecord->GetIntSubfield(pszIDFieldName, 0, RCID_SUBFIELD, 0);

    // Deal with FOID field updates
    if (const auto poFOIDFieldUpdate = poUpdateRecord->FindField(FOID_FIELD))
    {
        auto poFOIDFieldTarget = poTargetRecord->FindField(FOID_FIELD);
        if (!poFOIDFieldTarget)
        {
            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s, RCNM=%d, RCID=%d: missing FOID field in "
                "target record",
                m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
        }

        for (const char *pszSubFieldName :
             {AGEN_SUBFIELD, FIDN_SUBFIELD, FIDS_SUBFIELD})
        {
            const int nVal = poUpdateRecord->GetIntSubfield(poFOIDFieldUpdate,
                                                            pszSubFieldName, 0);
            poTargetRecord->SetIntSubfield(FOID_FIELD, 0, pszSubFieldName, 0,
                                           nVal);
        }
    }

    // Deal with SPAS field updates
    const auto apoSPASFieldUpdates = poUpdateRecord->GetFields(SPAS_FIELD);
    if (!apoSPASFieldUpdates.empty())
    {
        struct SPASField
        {
            int RRNM = 0;
            int RRID = 0;
            int ORNT = 0;
            int SMIN = 0;
            int SMAX = 0;
            int SAUI = 0;

            static SPASField Read(const DDFRecord *poRecord,
                                  const DDFField *poField, int i)
            {
                SPASField f;
                f.RRNM = poRecord->GetIntSubfield(poField, RRNM_SUBFIELD, i);
                f.RRID = poRecord->GetIntSubfield(poField, RRID_SUBFIELD, i);
                f.ORNT = poRecord->GetIntSubfield(poField, ORNT_SUBFIELD, i);
                f.SMIN = poRecord->GetIntSubfield(poField, SMIN_SUBFIELD, i);
                f.SMAX = poRecord->GetIntSubfield(poField, SMAX_SUBFIELD, i);
                f.SAUI = poRecord->GetIntSubfield(poField, SAUI_SUBFIELD, i);
                return f;
            }
        };

        std::vector<SPASField> asSPASFields;
        // Ingest the existing/target record(s)
        for (auto *poField : poTargetRecord->GetFields(SPAS_FIELD))
        {
            const int nRepeatCount = poField->GetRepeatCount();
            for (int i = 0; i < nRepeatCount; ++i)
            {
                asSPASFields.push_back(
                    SPASField::Read(poTargetRecord, poField, i));
            }

            poTargetRecord->DeleteField(poField);
        }

        // Apply the update record(s)
        for (const auto *poSPASFieldUpdate : apoSPASFieldUpdates)
        {
            const int nUpdateRepeatCount = poSPASFieldUpdate->GetRepeatCount();
            for (int i = 0; i < nUpdateRepeatCount; ++i)
            {
                SPASField updateFld =
                    SPASField::Read(poUpdateRecord, poSPASFieldUpdate, i);
                if (updateFld.SAUI == INSTRUCTION_INSERT)
                {
                    asSPASFields.push_back(updateFld);
                }
                else if (updateFld.SAUI == INSTRUCTION_DELETE)
                {
                    bool bMatchFound = false;
                    for (size_t j = 0; j < asSPASFields.size(); ++j)
                    {
                        if (asSPASFields[j].RRNM == updateFld.RRNM &&
                            asSPASFields[j].RRID == updateFld.RRID)
                        {
                            bMatchFound = true;
                            asSPASFields.erase(asSPASFields.begin() + j);
                            break;
                        }
                    }
                    if (!bMatchFound &&
                        !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s, RCNM=%d, RCID=%d, SPAS iSubField=%d: update "
                            "field references RRNM=%d, RRID=%d which does not "
                            "exist in initial or previous update",
                            m_osFilename.c_str(), static_cast<int>(nRCNM),
                            nRCID, i, updateFld.RRNM, updateFld.RRID)))
                    {
                        return false;
                    }
                }
                else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                             "%s, RCNM=%d, RCID=%d, SPAS iSubField=%d: invalid "
                             "SAUI=%d",
                             m_osFilename.c_str(), static_cast<int>(nRCNM),
                             nRCID, i, updateFld.SAUI)))
                {
                    return false;
                }
            }
        }

        if (!asSPASFields.empty())
        {
            const auto poSPASFieldDefn =
                m_oMainModule.FindFieldDefn(SPAS_FIELD);
            if (!poSPASFieldDefn)
            {
                return EMIT_ERROR("Cannot find SPAS field definition");
            }
            auto poSPASFieldTarget = poTargetRecord->AddField(poSPASFieldDefn);
            CPLAssert(poSPASFieldTarget);
            const auto *poSPASFieldUpdate = apoSPASFieldUpdates[0];
            if (*(poSPASFieldTarget->GetFieldDefn()) !=
                *(poSPASFieldUpdate->GetFieldDefn()))
            {
                return EMIT_ERROR("SPAS field definitions of update and target "
                                  "records are different");
            }

            for (int i = 0; i < static_cast<int>(asSPASFields.size()); ++i)
            {
                poTargetRecord->SetIntSubfield(SPAS_FIELD, 0, RRNM_SUBFIELD, i,
                                               asSPASFields[i].RRNM);
                poTargetRecord->SetIntSubfield(SPAS_FIELD, 0, RRID_SUBFIELD, i,
                                               asSPASFields[i].RRID);
                poTargetRecord->SetIntSubfield(SPAS_FIELD, 0, ORNT_SUBFIELD, i,
                                               asSPASFields[i].ORNT);
                poTargetRecord->SetIntSubfield(SPAS_FIELD, 0, SMIN_SUBFIELD, i,
                                               asSPASFields[i].SMIN);
                poTargetRecord->SetIntSubfield(SPAS_FIELD, 0, SMAX_SUBFIELD, i,
                                               asSPASFields[i].SMAX);
                poTargetRecord->SetIntSubfield(SPAS_FIELD, 0, SAUI_SUBFIELD, i,
                                               asSPASFields[i].SAUI);
            }
        }
    }

    // Deal with MASK field updates
    const auto apoMASKFieldUpdates = poUpdateRecord->GetFields(MASK_FIELD);
    if (!apoMASKFieldUpdates.empty())
    {
        struct MASKField
        {
            int RRNM = 0;
            int RRID = 0;
            int MIND = 0;
            int MUIN = 0;

            static MASKField Read(const DDFRecord *poRecord,
                                  const DDFField *poField, int i)
            {
                MASKField f;
                f.RRNM = poRecord->GetIntSubfield(poField, RRNM_SUBFIELD, i);
                f.RRID = poRecord->GetIntSubfield(poField, RRID_SUBFIELD, i);
                f.MIND = poRecord->GetIntSubfield(poField, MIND_SUBFIELD, i);
                f.MUIN = poRecord->GetIntSubfield(poField, MUIN_SUBFIELD, i);
                return f;
            }
        };

        std::vector<MASKField> asMASKFields;
        // Ingest the existing/target record(s)
        for (auto *poField : poTargetRecord->GetFields(MASK_FIELD))
        {
            const int nRepeatCount = poField->GetRepeatCount();
            for (int i = 0; i < nRepeatCount; ++i)
            {
                asMASKFields.push_back(
                    MASKField::Read(poTargetRecord, poField, i));
            }

            poTargetRecord->DeleteField(poField);
        }

        // Apply the update record(s)
        for (const auto *poMASKFieldUpdate : apoMASKFieldUpdates)
        {
            const int nUpdateRepeatCount = poMASKFieldUpdate->GetRepeatCount();
            for (int i = 0; i < nUpdateRepeatCount; ++i)
            {
                MASKField updateFld =
                    MASKField::Read(poUpdateRecord, poMASKFieldUpdate, i);
                if (updateFld.MUIN == INSTRUCTION_INSERT)
                {
                    asMASKFields.push_back(updateFld);
                }
                else if (updateFld.MUIN == INSTRUCTION_DELETE)
                {
                    bool bMatchFound = false;
                    for (size_t j = 0; j < asMASKFields.size(); ++j)
                    {
                        if (asMASKFields[j].RRNM == updateFld.RRNM &&
                            asMASKFields[j].RRID == updateFld.RRID)
                        {
                            bMatchFound = true;
                            asMASKFields.erase(asMASKFields.begin() + j);
                            break;
                        }
                    }
                    if (!bMatchFound &&
                        !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s, RCNM=%d, RCID=%d, MASK iSubField=%d: update "
                            "field references RRNM=%d, RRID=%d which does not "
                            "exist in initial or previous update",
                            m_osFilename.c_str(), static_cast<int>(nRCNM),
                            nRCID, i, updateFld.RRNM, updateFld.RRID)))
                    {
                        return false;
                    }
                }
                else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                             "%s, RCNM=%d, RCID=%d, MASK iSubField=%d: invalid "
                             "MUIN=%d",
                             m_osFilename.c_str(), static_cast<int>(nRCNM),
                             nRCID, i, updateFld.MUIN)))
                {
                    return false;
                }
            }
        }

        if (!asMASKFields.empty())
        {
            const auto poMASKFieldDefn =
                m_oMainModule.FindFieldDefn(MASK_FIELD);
            if (!poMASKFieldDefn)
            {
                return EMIT_ERROR("Cannot find MASK field definition");
            }
            auto poMASKFieldTarget = poTargetRecord->AddField(poMASKFieldDefn);
            CPLAssert(poMASKFieldTarget);
            const auto *poMASKFieldUpdate = apoMASKFieldUpdates[0];
            if (*(poMASKFieldTarget->GetFieldDefn()) !=
                *(poMASKFieldUpdate->GetFieldDefn()))
            {
                return EMIT_ERROR("MASK field definitions of update and target "
                                  "records are different");
            }

            for (int i = 0; i < static_cast<int>(asMASKFields.size()); ++i)
            {
                poTargetRecord->SetIntSubfield(MASK_FIELD, 0, RRNM_SUBFIELD, i,
                                               asMASKFields[i].RRNM);
                poTargetRecord->SetIntSubfield(MASK_FIELD, 0, RRID_SUBFIELD, i,
                                               asMASKFields[i].RRID);
                poTargetRecord->SetIntSubfield(MASK_FIELD, 0, MIND_SUBFIELD, i,
                                               asMASKFields[i].MIND);
                poTargetRecord->SetIntSubfield(MASK_FIELD, 0, MUIN_SUBFIELD, i,
                                               asMASKFields[i].MUIN);
            }
        }
    }

    return true;
}
