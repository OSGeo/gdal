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
/*                       GetMultiPointLayerName()                       */
/************************************************************************/

/* static */ std::string
OGRS101Reader::GetMultiPointLayerName(const OGRSpatialReference &oSRS)
{
    return oSRS.GetAxesCount() == 2
               ? "MultiPoint2D"
               : CPLSPrintf("MultiPoint3D_%s", LaunderCRSName(oSRS).c_str());
}

/************************************************************************/
/*                    CreateMultiPointFeatureDefns()                    */
/************************************************************************/

/** Create the feature definition(s) for the MultiPoint layer(s)
 *
 * There is a layer per CRS used by multipoints.
 */
bool OGRS101Reader::CreateMultiPointFeatureDefns()
{
    bool bError = false;

    m_oMapCRSIdToMultiPointRecordIdx =
        CreateMapCRSIdToRecordIdxForMultiPoints(bError);
    if (bError)
        return false;
    for (const auto &[nCRSId, anRecordIdx] : m_oMapCRSIdToMultiPointRecordIdx)
    {
        const auto &oSRS = m_oMapSRS[nCRSId];
        const bool bIs2D = nCRSId == HORIZONTAL_CRS_ID;
        auto poFDefn = OGRFeatureDefnRefCountedPtr::makeInstance(
            GetMultiPointLayerName(oSRS).c_str());
        poFDefn->SetGeomType(bIs2D ? wkbMultiPoint : wkbMultiPoint25D);
        poFDefn->GetGeomFieldDefn(0)->SetSpatialRef(
            OGRSpatialReferenceRefCountedPtr::makeClone(&oSRS).get());
        poFDefn->GetGeomFieldDefn(0)->SetCoordinatePrecision(
            m_coordinatePrecision);
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_ID, OFTInteger);
            poFDefn->AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_VERSION, OFTInteger);
            poFDefn->AddFieldDefn(&oFieldDefn);
        }
        if (!InferFeatureDefn(m_oMultiPointRecordIndex, MRID_FIELD, INAS_FIELD,
                              anRecordIdx, *poFDefn, m_oMapFieldDomains))
        {
            return false;
        }
        m_oMapMultiPointFeatureDefn[nCRSId] = std::move(poFDefn);
    }

    return true;
}

/************************************************************************/
/*                    GetCRSIdForMultiPointRecord()                     */
/************************************************************************/

/** Return the CRS id for a given MultiPoint record, or INVALID_CRS_ID on error */
OGRS101Reader::CRSId
OGRS101Reader::GetCRSIdForMultiPointRecord(const DDFRecord *poRecord,
                                           int iRecord, int nRecordID) const
{
    if (nRecordID < 0)
        nRecordID = poRecord->GetIntSubfield(MRID_FIELD, 0, RCID_SUBFIELD, 0);

    const auto GetErrorContext = [iRecord, nRecordID]()
    {
        if (iRecord >= 0)
            return CPLSPrintf("Record index=%d of MRID", iRecord);
        else
            return CPLSPrintf("Record ID=%d of MRID", nRecordID);
    };

    if (poRecord->FindField(C3IL_FIELD))
    {
        const CRSId nVCID =
            poRecord->GetIntSubfield(C3IL_FIELD, 0, VCID_SUBFIELD, 0);
        if (nVCID == HORIZONTAL_CRS_ID)
        {
            CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: VCID subfield = %d of C3IL "
                           "field points to a non-3D CRS.",
                           GetErrorContext(), static_cast<int>(nVCID))));
            return INVALID_CRS_ID;
        }
        else if (!cpl::contains(m_oMapSRS, nVCID))
        {
            CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: Unknown value %d for VCID subfield of C3IL "
                           "field.",
                           GetErrorContext(), static_cast<int>(nVCID))));
            return INVALID_CRS_ID;
        }
        else
        {
            return nVCID;
        }
    }
    else if (poRecord->FindField(C2IL_FIELD))
    {
        return HORIZONTAL_CRS_ID;
    }
    else
    {
        CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s: No C2IL or C3IL field found.", GetErrorContext())));
        return INVALID_CRS_ID;
    }
}

/************************************************************************/
/*              CreateMapCRSIdToRecordIdxForMultiPoints()               */
/************************************************************************/

/** Browse through m_oMultiPointRecordIndex to identify which record belongs to
 * each CRS and create a map from each CRS id to the record indices that use
 * it.
 */
std::map<OGRS101Reader::CRSId, std::vector<int>>
OGRS101Reader::CreateMapCRSIdToRecordIdxForMultiPoints(bool &bError) const
{
    std::map<OGRS101Reader::CRSId, std::vector<int>> map;

    const int nRecords = m_oMultiPointRecordIndex.GetCount();
    for (int iRecord = 0; iRecord < nRecords; ++iRecord)
    {
        const auto poRecord = m_oMultiPointRecordIndex.GetByIndex(iRecord);
        const CRSId nCRSId = GetCRSIdForMultiPointRecord(poRecord, iRecord,
                                                         /* nRecordID = */ -1);
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
/*                       ReadMultiPointGeometry()                       */
/************************************************************************/

std::unique_ptr<OGRMultiPoint>
OGRS101Reader::ReadMultiPointGeometry(const DDFRecord *poRecord, int iRecord,
                                      int nRecordID,
                                      const OGRSpatialReference *poSRS) const
{
    const bool bIs3D = poRecord->FindField(C3IL_FIELD) != nullptr;
    const char *pszCoordFieldName = bIs3D ? C3IL_FIELD : C2IL_FIELD;
    const auto apoCoordFields = poRecord->GetFields(pszCoordFieldName);
    if (apoCoordFields.empty())
        return nullptr;

    auto poMP = std::make_unique<OGRMultiPoint>();
    poMP->assignSpatialReference(poSRS);
    for (const auto *poCoordField : apoCoordFields)
    {
        const int nCoordCount =
            bIs3D && poCoordField->GetParts().size() == 2
                ? poCoordField->GetParts()[1]->GetRepeatCount()
                : poCoordField->GetRepeatCount();

        for (int iPnt = 0; iPnt < nCoordCount; ++iPnt)
        {
            auto poPoint = ReadPointGeometryInternal(
                poRecord, iRecord, nRecordID, iPnt, poSRS, bIs3D, poCoordField,
                MRID_FIELD);
            if (!poPoint)
                return nullptr;
            poMP->addGeometry(std::move(poPoint));
        }
    }
    return poMP;
}

/************************************************************************/
/*                       FillFeatureMultiPoint()                        */
/************************************************************************/

/** Fill the content of the provided feature from the identified record
 * (of m_oMultiPointRecordIndex).
 */
bool OGRS101Reader::FillFeatureMultiPoint(const DDFRecordIndex &oIndex,
                                          int iRecord,
                                          OGRFeature &oFeature) const
{
    const auto poRecord = oIndex.GetByIndex(iRecord);
    CPLAssert(poRecord);

    const OGRSpatialReference *poSRS =
        oFeature.GetDefnRef()->GetGeomFieldDefn(0)->GetSpatialRef();
    auto poMP =
        ReadMultiPointGeometry(poRecord, iRecord, /* nRecordID = */ -1, poSRS);
    if (poMP)
    {
        oFeature.SetGeometry(std::move(poMP));
    }
    else if (m_bStrict)
        return false;

    return FillFeatureAttributes(oIndex, iRecord, INAS_FIELD, oFeature) &&
           FillFeatureWithNonAttrAssocSubfields(poRecord, iRecord, INAS_FIELD,
                                                oFeature);
}

/************************************************************************/
/*                   ProcessUpdateRecordMultiPoint()                    */
/************************************************************************/

/** Updates the geometry part of poTargetRecord with poUpdateRecord */
bool OGRS101Reader::ProcessUpdateRecordMultiPoint(
    const DDFRecord *poUpdateRecord, DDFRecord *poTargetRecord) const
{
    return ProcessUpdatePointList(poUpdateRecord, poTargetRecord,
                                  /* bIs3DAllowed = */ true);
}

/************************************************************************/
/*                       ProcessUpdatePointList()                       */
/************************************************************************/

/** Updates the point list of poTargetRecord with poUpdateRecord */
bool OGRS101Reader::ProcessUpdatePointList(const DDFRecord *poUpdateRecord,
                                           DDFRecord *poTargetRecord,
                                           bool bIs3DAllowed) const
{
    const auto poIDField = poUpdateRecord->GetField(0);
    CPLAssert(poIDField);

    // Record name
    const RecordName nRCNM =
        poUpdateRecord->GetIntSubfield(poIDField, RCNM_SUBFIELD, 0);

    // Record identifier
    const int nRCID =
        poUpdateRecord->GetIntSubfield(poIDField, RCID_SUBFIELD, 0);

    // Coordinate Control field
    const auto poControlField = poUpdateRecord->FindField(COCC_FIELD);
    if (!poControlField)
        return true;

    // Number of Coordinates
    constexpr const char *NCOR_SUBFIELD = "NCOR";
    const int nNCOR =
        poUpdateRecord->GetIntSubfield(poControlField, NCOR_SUBFIELD, 0);

    // Coordinate Update Instruction
    constexpr const char *COUI_SUBFIELD = "COUI";
    const int nCOUI =
        poUpdateRecord->GetIntSubfield(poControlField, COUI_SUBFIELD, 0);

    bool bIs3D = false;
    const DDFField *poUpdateField = nullptr;
    DDFField *poTargetField = nullptr;
    if (nCOUI == INSTRUCTION_DELETE)
    {
        if ((poTargetField = poTargetRecord->FindField(C2IL_FIELD)) != nullptr)
        {
            poUpdateField = poUpdateRecord->FindField(C2IL_FIELD);
        }
        else if (bIs3DAllowed && (poTargetField = poTargetRecord->FindField(
                                      C3IL_FIELD)) != nullptr)
        {
            poUpdateField = poUpdateRecord->FindField(C3IL_FIELD);
            bIs3D = true;
        }
        else
        {
            return EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s, RCNM=%d, RCID=%d: missing %s field in "
                           "target record",
                           m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                           bIs3DAllowed ? "C2IL / C3IL" : "C2IL"));
        }
    }
    else if ((poUpdateField = poUpdateRecord->FindField(C2IL_FIELD)) != nullptr)
    {
        if ((poTargetField = poTargetRecord->FindField(C2IL_FIELD)) == nullptr)
        {
            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s, RCNM=%d, RCID=%d: cannot find C2IL field in target "
                "record",
                m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
        }
    }
    else if (bIs3DAllowed &&
             (poUpdateField = poUpdateRecord->FindField(C3IL_FIELD)) != nullptr)
    {
        if ((poTargetField = poTargetRecord->FindField(C3IL_FIELD)) == nullptr)
        {
            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s, RCNM=%d, RCID=%d: cannot find C3IL field in target "
                "record",
                m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
        }
        bIs3D = true;
    }
    else
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s, RCNM=%d, RCID=%d: missing %s field in update "
                       "record",
                       m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                       bIs3DAllowed ? "C2IL / C3IL" : "C2IL"));
    }

    const char *pszCoordFieldName = bIs3D ? C3IL_FIELD : C2IL_FIELD;
    if ((poUpdateField && poUpdateRecord->GetFields(C2IL_FIELD).size() > 1) ||
        poTargetRecord->GetFields(C2IL_FIELD).size() > 1)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s: only one instance of %s supported for update",
                       m_osFilename.c_str(), pszCoordFieldName));
    }

    // Coordinate Index (1-based)
    constexpr const char *COIX_SUBFIELD = "COIX";
    const int nCOIX =
        poUpdateRecord->GetIntSubfield(poControlField, COIX_SUBFIELD, 0);

    const int nTargetRepeatCount =
        (bIs3D && poTargetField->GetParts().size() == 2)
            ? poTargetField->GetParts()[1]->GetRepeatCount()
            : poTargetField->GetRepeatCount();

    // Check that start index and count is consistent with update and
    // target list of points
    const int nMaxCOIXAllowed =
        nTargetRepeatCount + (nCOUI == INSTRUCTION_INSERT ? 1 : 0);
    if (nCOIX <= 0 || nCOIX > nMaxCOIXAllowed)
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d: invalid COIX = %d. Must be in [1,%d].",
            m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID, nCOIX,
            nMaxCOIXAllowed));
    }

    const int nUpdateRepeatCount =
        !poUpdateField ? 0
        : (bIs3D && poUpdateField->GetParts().size() == 2)
            ? poUpdateField->GetParts()[1]->GetRepeatCount()
            : poUpdateField->GetRepeatCount();

    if (poUpdateField && nNCOR != nUpdateRepeatCount)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s, RCNM=%d, RCID=%d: invalid NCOR = %d. Expected %d",
                       m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                       nNCOR, nUpdateRepeatCount));
    }
    else if (poUpdateField && nCOUI == INSTRUCTION_DELETE &&
             !EMIT_ERROR_OR_WARNING(
                 CPLSPrintf("%s, RCNM=%d, RCID=%d: unexpected %s field in "
                            "update record in COUI = %d (delete) mode",
                            m_osFilename.c_str(), static_cast<int>(nRCNM),
                            nRCID, pszCoordFieldName, nCOUI)))
    {
        return false;
    }

    if (nCOUI == INSTRUCTION_UPDATE || nCOUI == INSTRUCTION_DELETE)
    {
        if (nCOIX + nNCOR > nTargetRepeatCount + 1)
        {
            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s, RCNM=%d, RCID=%d: invalid COIX = %d and/or NCOR=%d",
                m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID, nCOIX,
                nNCOR));
        }
    }
    else if (nCOUI != INSTRUCTION_INSERT)
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d: invalid COUI = %d", m_osFilename.c_str(),
            static_cast<int>(nRCNM), nRCID, nCOUI));
    }

    struct Coord
    {
        int Y = 0;
        int X = 0;
        int Z = 0;

        void Read(const DDFRecord *poRecord, const DDFField *poField, int i,
                  bool bIs3D)
        {
            Y = poRecord->GetIntSubfield(poField, YCOO_SUBFIELD, i);
            X = poRecord->GetIntSubfield(poField, XCOO_SUBFIELD, i);
            if (bIs3D)
                Z = poRecord->GetIntSubfield(poField, ZCOO_SUBFIELD, i);
        }
    };

    std::vector<Coord> asTarget;
    // Ingest the existing/target record
    for (int i = 0; i < nTargetRepeatCount; ++i)
    {
        Coord c;
        c.Read(poTargetRecord, poTargetField, i, bIs3D);
        asTarget.push_back(c);
    }

    // Apply the update record
    std::vector<Coord> asUpdate;
    if (poUpdateField)
    {
        for (int i = 0; i < nUpdateRepeatCount; ++i)
        {
            Coord c;
            c.Read(poUpdateRecord, poUpdateField, i, bIs3D);
            asUpdate.push_back(c);
        }
    }

    const auto oTargetBeginIter = asTarget.begin() + (nCOIX - 1);
    if (nCOUI == INSTRUCTION_INSERT)
    {
        asTarget.insert(oTargetBeginIter, asUpdate.begin(), asUpdate.end());
    }
    else if (nCOUI == INSTRUCTION_UPDATE)
    {
        std::copy(asUpdate.begin(), asUpdate.end(), oTargetBeginIter);
    }
    else
    {
        CPLAssert(nCOUI == INSTRUCTION_DELETE);
        asTarget.erase(oTargetBeginIter, oTargetBeginIter + nNCOR);
    }

    // Compose raw target field
    std::string s;
    if (bIs3D)
    {
        const DDFRecord *poRecord =
            poUpdateRecord ? poUpdateRecord : poTargetRecord;
        const DDFField *poField = poUpdateField ? poUpdateField : poTargetField;
        AppendUInt8(s, static_cast<uint8_t>(poRecord->GetIntSubfield(
                           poField, VCID_SUBFIELD, 0)));
    }

    for (const auto &c : asTarget)
    {
        AppendInt32(s, c.Y);
        AppendInt32(s, c.X);
        if (bIs3D)
            AppendInt32(s, c.Z);
    }
    AppendUInt8(s, DDF_FIELD_TERMINATOR);

    poTargetRecord->SetFieldRaw(poTargetField, s.data(),
                                static_cast<int>(s.size()));

    return true;
}
