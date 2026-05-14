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
#include "ogrs101readerconstants.h"

#include <memory>

/************************************************************************/
/*                         GetPointLayerName()                          */
/************************************************************************/

/* static */ std::string
OGRS101Reader::GetPointLayerName(const OGRSpatialReference &oSRS)
{
    return oSRS.GetAxesCount() == 2
               ? "Point2D"
               : CPLSPrintf("Point3D_%s", LaunderCRSName(oSRS).c_str());
}

/************************************************************************/
/*                      CreatePointFeatureDefns()                       */
/************************************************************************/

/** Create the feature definition(s) for the Point layer(s)
 *
 * There is a layer per CRS used by points.
 */
bool OGRS101Reader::CreatePointFeatureDefns()
{
    bool bError = false;
    m_oMapCRSIdToPointRecordIdx = CreateMapCRSIdToRecordIdxForPoints(bError);
    if (bError)
        return false;
    for (const auto &[nCRSId, anRecordIdx] : m_oMapCRSIdToPointRecordIdx)
    {
        const auto &oSRS = m_oMapSRS[nCRSId];
        const bool bIs2D = nCRSId == HORIZONTAL_CRS_ID;
        auto poFDefn = OGRFeatureDefnRefCountedPtr::makeInstance(
            GetPointLayerName(oSRS).c_str());
        poFDefn->SetGeomType(bIs2D ? wkbPoint : wkbPoint25D);
        poFDefn->GetGeomFieldDefn(0)->SetSpatialRef(
            OGRSpatialReferenceRefCountedPtr::makeClone(&oSRS).get());
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_ID, OFTInteger);
            poFDefn->AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_VERSION, OFTInteger);
            poFDefn->AddFieldDefn(&oFieldDefn);
        }

        if (!InferFeatureDefn(m_oPointRecordIndex, PRID_FIELD, INAS_FIELD,
                              anRecordIdx, *poFDefn, m_oMapFieldDomains))
        {
            return false;
        }
        m_oMapPointFeatureDefn[nCRSId] = std::move(poFDefn);
    }

    return true;
}

/************************************************************************/
/*                       GetCRSIdForPointRecord()                       */
/************************************************************************/

/** Return the CRS id for a given Point record, or INVALID_CRS_ID on error */
OGRS101Reader::CRSId
OGRS101Reader::GetCRSIdForPointRecord(const DDFRecord *poRecord, int iRecord,
                                      int nRecordID) const
{
    if (nRecordID < 0)
        nRecordID = poRecord->GetIntSubfield(PRID_FIELD, 0, RCID_SUBFIELD, 0);

    const auto GetErrorContext = [iRecord, nRecordID]()
    {
        if (iRecord >= 0)
            return CPLSPrintf("Record index=%d of PRID", iRecord);
        else
            return CPLSPrintf("Record ID=%d of PRID", nRecordID);
    };

    if (poRecord->FindField(C3IT_FIELD))
    {
        const CRSId nVCID =
            poRecord->GetIntSubfield(C3IT_FIELD, 0, VCID_SUBFIELD, 0);
        if (nVCID == HORIZONTAL_CRS_ID)
        {
            CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: VCID subfield = %d of C3IT "
                           "field points to a non-3D CRS.",
                           GetErrorContext(), static_cast<int>(nVCID))));
            return INVALID_CRS_ID;
        }
        else if (!cpl::contains(m_oMapSRS, nVCID))
        {
            CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: Unknown value %d for VCID subfield of C3IT "
                           "field.",
                           GetErrorContext(), static_cast<int>(nVCID))));
            return INVALID_CRS_ID;
        }
        else
        {
            return nVCID;
        }
    }
    else if (poRecord->FindField(C2IT_FIELD))
    {
        return HORIZONTAL_CRS_ID;
    }
    else
    {
        CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s: No C2IT or C3IT field found.", GetErrorContext())));
        return INVALID_CRS_ID;
    }
}

/************************************************************************/
/*                 CreateMapCRSIdToRecordIdxForPoints()                 */
/************************************************************************/

/** Browse through m_oPointRecordIndex to identify which record belongs to
 * each CRS and create a map from each CRS id to the record indices that use
 * it.
 */
std::map<OGRS101Reader::CRSId, std::vector<int>>
OGRS101Reader::CreateMapCRSIdToRecordIdxForPoints(bool &bError) const
{
    std::map<OGRS101Reader::CRSId, std::vector<int>> map;

    const int nRecords = m_oPointRecordIndex.GetCount();
    for (int iRecord = 0; iRecord < nRecords; ++iRecord)
    {
        const auto poRecord = m_oPointRecordIndex.GetByIndex(iRecord);
        const CRSId nCRSId =
            GetCRSIdForPointRecord(poRecord, iRecord, /* nRecordID = */ -1);
        if (nCRSId == INVALID_CRS_ID)
        {
            if (m_bStrict)
            {
                bError = true;
                return {};
            }
        }
        else
        {
            map[nCRSId].push_back(iRecord);
        }
    }

    return map;
}

/************************************************************************/
/*                         ReadPointGeometry()                          */
/************************************************************************/

std::unique_ptr<OGRPoint> OGRS101Reader::ReadPointGeometryInternal(
    const DDFRecord *poRecord, int iRecord, int nRecordID, int iPnt,
    const OGRSpatialReference *poSRS, const bool bIs3D,
    const DDFField *poCoordField, const char *pszRecordFieldName) const
{
    const auto GetErrorContext = [iRecord, nRecordID, pszRecordFieldName]()
    {
        if (iRecord >= 0)
            return CPLSPrintf("Record index=%d of %s", iRecord,
                              pszRecordFieldName);
        else
            return CPLSPrintf("Record ID=%d of %s", nRecordID,
                              pszRecordFieldName);
    };

    if (!poCoordField)
    {
        CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s: cannot find coordinate field.", GetErrorContext())));
        return nullptr;
    }

    int bSuccess = false;
    const double dfX =
        poRecord->GetIntSubfield(poCoordField, XCOO_SUBFIELD, iPnt, &bSuccess) /
            static_cast<double>(m_nXScale) +
        m_dfXShift;
    if (!bSuccess &&
        !EMIT_ERROR_OR_WARNING(CPLSPrintf("%s: cannot read %s.",
                                          GetErrorContext(), XCOO_SUBFIELD)))
    {
        return nullptr;
    }
    const double dfY =
        poRecord->GetIntSubfield(poCoordField, YCOO_SUBFIELD, iPnt, &bSuccess) /
            static_cast<double>(m_nYScale) +
        m_dfYShift;
    if (!bSuccess &&
        !EMIT_ERROR_OR_WARNING(CPLSPrintf("%s: cannot read %s.",
                                          GetErrorContext(), YCOO_SUBFIELD)))
    {
        return nullptr;
    }
    if (poSRS && !(std::fabs(dfX) <= 180 && std::fabs(dfY) <= 90) &&
        poSRS->IsGeographic())
    {
        if (!EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: wrong coordinate value: lon=%f, lat=%f.",
                           GetErrorContext(), dfX, dfY)))
        {
            return nullptr;
        }
    }
    std::unique_ptr<OGRPoint> poPoint;
    if (bIs3D)
    {
        const double dfZ = poRecord->GetIntSubfield(poCoordField, ZCOO_SUBFIELD,
                                                    iPnt, &bSuccess) /
                               static_cast<double>(m_nZScale) +
                           m_dfZShift;
        if (!bSuccess &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s: cannot read %s.", GetErrorContext(), ZCOO_SUBFIELD)))
        {
            return nullptr;
        }
        poPoint = std::make_unique<OGRPoint>(dfX, dfY, dfZ);
    }
    else
    {
        poPoint = std::make_unique<OGRPoint>(dfX, dfY);
    }
    poPoint->assignSpatialReference(poSRS);
    return poPoint;
}

std::unique_ptr<OGRPoint>
OGRS101Reader::ReadPointGeometry(const DDFRecord *poRecord, int iRecord,
                                 int nRecordID,
                                 const OGRSpatialReference *poSRS) const
{
    const DDFField *poCoordField = poRecord->FindField(C3IT_FIELD);
    const bool bIs3D = poCoordField != nullptr;
    if (!poCoordField)
        poCoordField = poRecord->FindField(C2IT_FIELD);

    return ReadPointGeometryInternal(poRecord, iRecord, nRecordID,
                                     /* iPnt = */ 0, poSRS, bIs3D, poCoordField,
                                     PRID_FIELD);
}

/************************************************************************/
/*                          FillFeaturePoint()                          */
/************************************************************************/

/** Fill the content of the provided feature from the identified record
 * (of m_oPointRecordIndex).
 */
bool OGRS101Reader::FillFeaturePoint(const DDFRecordIndex &oIndex, int iRecord,
                                     OGRFeature &oFeature) const
{
    const auto poRecord = oIndex.GetByIndex(iRecord);
    CPLAssert(poRecord);

    const OGRSpatialReference *poSRS =
        oFeature.GetDefnRef()->GetGeomFieldDefn(0)->GetSpatialRef();
    auto poPoint =
        ReadPointGeometry(poRecord, iRecord, /* nRecordID = */ -1, poSRS);
    if (poPoint)
        oFeature.SetGeometry(std::move(poPoint));
    else if (m_bStrict)
        return false;

    return FillFeatureAttributes(oIndex, iRecord, INAS_FIELD, oFeature) &&
           FillFeatureWithNonAttrAssocSubfields(poRecord, iRecord, INAS_FIELD,
                                                oFeature);
}

/************************************************************************/
/*                      ProcessUpdateRecordPoint()                      */
/************************************************************************/

/** Updates the geometry part of poTargetRecord with poUpdateRecord */
bool OGRS101Reader::ProcessUpdateRecordPoint(const DDFRecord *poUpdateRecord,
                                             DDFRecord *poTargetRecord) const
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

    if (const auto poC2ITFieldUpdate = poUpdateRecord->FindField(C2IT_FIELD))
    {
        auto poC2ITFieldTarget = poTargetRecord->FindField(C2IT_FIELD);
        if (!poC2ITFieldTarget)
        {
            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s, RCNM=%d, RCID=%d: cannot find C2IT field in target "
                "record",
                m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
        }
        if (*(poC2ITFieldTarget->GetFieldDefn()) !=
            *(poC2ITFieldUpdate->GetFieldDefn()))
        {
            return EMIT_ERROR("C2IT field definitions of update and target "
                              "records are different");
        }
        for (const char *pszSubFieldName : {XCOO_SUBFIELD, YCOO_SUBFIELD})
        {
            poTargetRecord->SetIntSubfield(
                C2IT_FIELD, 0, pszSubFieldName, 0,
                poUpdateRecord->GetIntSubfield(C2IT_FIELD, 0, pszSubFieldName,
                                               0));
        }
    }
    else if (const auto poC3ITFieldUpdate =
                 poUpdateRecord->FindField(C3IT_FIELD))
    {
        auto poC3ITFieldTarget = poTargetRecord->FindField(C3IT_FIELD);
        if (!poC3ITFieldTarget)
        {
            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s, RCNM=%d, RCID=%d: cannot find C3IT field in target "
                "record",
                m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
        }
        if (*(poC3ITFieldTarget->GetFieldDefn()) !=
            *(poC3ITFieldUpdate->GetFieldDefn()))
        {
            return EMIT_ERROR("C3IT field definitions of update and target "
                              "records are different");
        }
        for (const char *pszSubFieldName :
             {XCOO_SUBFIELD, YCOO_SUBFIELD, ZCOO_SUBFIELD})
        {
            poTargetRecord->SetIntSubfield(
                C3IT_FIELD, 0, pszSubFieldName, 0,
                poUpdateRecord->GetIntSubfield(C3IT_FIELD, 0, pszSubFieldName,
                                               0));
        }
    }

    return true;
}
