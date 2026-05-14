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
/*                       CreateCurveFeatureDefn()                       */
/************************************************************************/

/** Create the feature definition for the Curve layer
 */
bool OGRS101Reader::CreateCurveFeatureDefn()
{
    if (m_oCurveRecordIndex.GetCount() > 0)
    {
        m_poFeatureDefnCurve =
            OGRFeatureDefnRefCountedPtr::makeInstance(OGR_LAYER_NAME_CURVE);
        m_poFeatureDefnCurve->SetGeomType(wkbLineString);
        auto oSRSIter = m_oMapSRS.find(HORIZONTAL_CRS_ID);
        if (oSRSIter != m_oMapSRS.end())
        {
            m_poFeatureDefnCurve->GetGeomFieldDefn(0)->SetSpatialRef(
                OGRSpatialReferenceRefCountedPtr::makeClone(&oSRSIter->second)
                    .get());
        }
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_ID, OFTInteger);
            m_poFeatureDefnCurve->AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_VERSION, OFTInteger);
            m_poFeatureDefnCurve->AddFieldDefn(&oFieldDefn);
        }
        if (!InferFeatureDefn(m_oCurveRecordIndex, CRID_FIELD, INAS_FIELD, {},
                              *m_poFeatureDefnCurve, m_oMapFieldDomains))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                         ReadCurveGeometry()                          */
/************************************************************************/

std::unique_ptr<OGRLineString>
OGRS101Reader::ReadCurveGeometry(const DDFRecord *poRecord, int iRecord,
                                 int nRecordID,
                                 const OGRSpatialReference *poSRS) const
{
    const DDFField *poSEGHField = poRecord->FindField(SEGH_FIELD);
    if (poSEGHField)
    {
        constexpr const char *INTP_SUBFIELD = "INTP";
        const int nINTP =
            poRecord->GetIntSubfield(SEGH_FIELD, 0, INTP_SUBFIELD, 0);
        constexpr int INTERPOLATION_LOXODROMIC = 4;
        if (nINTP != INTERPOLATION_LOXODROMIC &&
            !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("Record index %d of CRID: Invalid value for INTP "
                           "subfield of SEGH field: "
                           "got %d, expected %d.",
                           iRecord, nINTP, INTERPOLATION_LOXODROMIC)))
        {
            return nullptr;
        }
    }

    const auto apoCoordFields = poRecord->GetFields(C2IL_FIELD);
    if (apoCoordFields.empty())
        return nullptr;

    auto poLS = std::make_unique<OGRLineString>();
    poLS->assignSpatialReference(poSRS);
    int nCoordCount = 0;
    for (const auto *poCoordField : apoCoordFields)
    {
        const int nCoordFieldCount = poCoordField->GetRepeatCount();
        nCoordCount += nCoordFieldCount;
    }
    poLS->setNumPoints(nCoordCount);
    int iPnt = 0;
    for (const auto *poCoordField : apoCoordFields)
    {
        const int nCoordFieldCount = poCoordField->GetRepeatCount();
        for (int iPntThisField = 0; iPntThisField < nCoordFieldCount;
             ++iPntThisField)
        {
            auto poPoint = ReadPointGeometryInternal(
                poRecord, iRecord, nRecordID, iPntThisField, poSRS,
                /* bIs3D = */ false, poCoordField, CRID_FIELD);
            if (!poPoint)
                return nullptr;
            poLS->setPoint(iPnt, poPoint.get());
            ++iPnt;
        }
    }

    const DDFField *poPTASField = poRecord->FindField(PTAS_FIELD);
    if (poPTASField)
    {
        const int nPTASMembers = poPTASField->GetRepeatCount();
        constexpr const char *TOPI_SUBFIELD = "TOPI";
        if (nPTASMembers == 1 || nPTASMembers == 2)
        {
            for (int iPTAS = 0; iPTAS < nPTASMembers; ++iPTAS)
            {
                const auto GetIntSubfield =
                    [poRecord, poPTASField, iPTAS](const char *pszSubFieldName)
                {
                    return poRecord->GetIntSubfield(poPTASField,
                                                    pszSubFieldName, iPTAS);
                };

                const RecordName nRRNM = GetIntSubfield(RRNM_SUBFIELD);
                constexpr RecordName nExpectedRRNM = RECORD_NAME_POINT;
                if (nRRNM != nExpectedRRNM &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Record index %d of CRID: Invalid value for RRNM "
                        "subfield of %d instance of PTAS field: "
                        "got %d, expected %d.",
                        iRecord, iPTAS, static_cast<int>(nRRNM),
                        static_cast<int>(nExpectedRRNM))))
                {
                    return nullptr;
                }

                const int nTOPI = GetIntSubfield(TOPI_SUBFIELD);
                constexpr int TOPOLOGY_INDICATOR_BEGINNING_POINT = 1;
                constexpr int TOPOLOGY_INDICATOR_END_POINT = 2;
                constexpr int TOPOLOGY_INDICATOR_BEGINNING_AND_END_POINT = 3;
                const int nExpectedTOPI =
                    nPTASMembers == 1
                        ? TOPOLOGY_INDICATOR_BEGINNING_AND_END_POINT
                    : iPTAS == 0 ? TOPOLOGY_INDICATOR_BEGINNING_POINT
                                 : TOPOLOGY_INDICATOR_END_POINT;

                if (nTOPI != nExpectedTOPI &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Record index %d of CRID: Invalid value for TOPI "
                        "subfield of %d instance of PTAS field: "
                        "got %d, expected %d.",
                        iRecord, iPTAS, nTOPI, nExpectedTOPI)))
                {
                    return nullptr;
                }

                const int nRRID = GetIntSubfield(RRID_SUBFIELD);
                const auto poPointRecord =
                    m_oPointRecordIndex.FindRecord(nRRID);
                if (!poPointRecord &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Record index %d of CRID: No point record matching "
                        "RRID=%d value of %d instance of PTAS field.",
                        iRecord, nRRID, iPTAS)))
                {
                    return nullptr;
                }
                if (poPointRecord)
                {
                    if (nTOPI == TOPOLOGY_INDICATOR_BEGINNING_POINT ||
                        nTOPI == TOPOLOGY_INDICATOR_BEGINNING_AND_END_POINT)
                    {
                        if (poPointRecord->GetIntSubfield(C2IT_FIELD, 0,
                                                          XCOO_SUBFIELD, 0) !=
                                poRecord->GetIntSubfield(C2IL_FIELD, 0,
                                                         XCOO_SUBFIELD, 0) ||
                            poPointRecord->GetIntSubfield(C2IT_FIELD, 0,
                                                          YCOO_SUBFIELD, 0) !=
                                poRecord->GetIntSubfield(C2IL_FIELD, 0,
                                                         YCOO_SUBFIELD, 0))
                        {
                            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                    "Record index %d of CRID: Point record %d "
                                    "pointed by %d instance of PTAS field does "
                                    "not match first point of curve.",
                                    iRecord, nRRID, iPTAS)))
                            {
                                return nullptr;
                            }
                        }
                    }

                    if (nTOPI == TOPOLOGY_INDICATOR_END_POINT ||
                        nTOPI == TOPOLOGY_INDICATOR_BEGINNING_AND_END_POINT)
                    {
                        if (poPointRecord->GetIntSubfield(C2IT_FIELD, 0,
                                                          XCOO_SUBFIELD, 0) !=
                                poRecord->GetIntSubfield(C2IL_FIELD, 0,
                                                         XCOO_SUBFIELD,
                                                         nCoordCount - 1) ||
                            poPointRecord->GetIntSubfield(C2IT_FIELD, 0,
                                                          YCOO_SUBFIELD, 0) !=
                                poRecord->GetIntSubfield(C2IL_FIELD, 0,
                                                         YCOO_SUBFIELD,
                                                         nCoordCount - 1))
                        {
                            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                    "Record index %d of CRID: Point record %d "
                                    "pointed by %d instance of PTAS field does "
                                    "not match end point of curve.",
                                    iRecord, nRRID, iPTAS)))
                            {
                                return nullptr;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Record index %d of CRID: Invalid repeat count for "
                    "PTAS field: got %d, expected 1 or 2.",
                    iRecord, nPTASMembers)))
            {
                return nullptr;
            }
        }
    }

    return poLS;
}

/************************************************************************/
/*                          FillFeatureCurve()                          */
/************************************************************************/

/** Fill the content of the provided feature from the identified record
 * (of m_oCurveRecordIndex).
 */
bool OGRS101Reader::FillFeatureCurve(const DDFRecordIndex &oIndex, int iRecord,
                                     OGRFeature &oFeature) const
{
    const auto poRecord = oIndex.GetByIndex(iRecord);
    CPLAssert(poRecord);

    const OGRSpatialReference *poSRS =
        oFeature.GetDefnRef()->GetGeomFieldDefn(0)->GetSpatialRef();
    auto poLS =
        ReadCurveGeometry(poRecord, iRecord, /* nRecordID = */ -1, poSRS);
    if (!poLS)
    {
        if (m_bStrict)
            return false;  // error message already emitted
    }
    else
    {
        oFeature.SetGeometry(std::move(poLS));
    }

    return FillFeatureAttributes(oIndex, iRecord, INAS_FIELD, oFeature) &&
           FillFeatureWithNonAttrAssocSubfields(poRecord, iRecord, INAS_FIELD,
                                                oFeature);
}

/************************************************************************/
/*                      ProcessUpdateRecordCurve()                      */
/************************************************************************/

/** Updates the geometry part of poTargetRecord with poUpdateRecord */
bool OGRS101Reader::ProcessUpdateRecordCurve(const DDFRecord *poUpdateRecord,
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

    const auto poUpdatePTASField = poUpdateRecord->FindField(PTAS_FIELD);
    if (poUpdatePTASField)
    {
        const auto poTargetPTASField = poTargetRecord->FindField(PTAS_FIELD);
        if (!poTargetPTASField)
        {
            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%s, RCNM=%d, RCID=%d: missing PTAS field in "
                "target record",
                m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
        }

        // Replace whole target PTAS field with update PTAS field
        poTargetRecord->SetFieldRaw(poTargetPTASField,
                                    poUpdatePTASField->GetData(),
                                    poUpdatePTASField->GetDataSize());
    }

    // Segment Control field
    // Update rules described at S-100 Ed 5.2 "10a-7.2.4.1 Encoding rules"
    const auto poSECCField = poUpdateRecord->FindField(SECC_FIELD);
    if (!poSECCField)
        return true;

    const auto poUpdateC2ILField = poUpdateRecord->FindField(C2IL_FIELD);
    if (!poUpdateC2ILField)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s, RCNM=%d, RCID=%d: missing C2IL field in "
                       "update record",
                       m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
    }

    const auto poTargetC2ILField = poTargetRecord->FindField(C2IL_FIELD);
    if (!poTargetC2ILField)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s, RCNM=%d, RCID=%d: missing C2IL field in "
                       "target record",
                       m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID));
    }

    // Segment update instruction
    const int SEUI = poUpdateRecord->GetIntSubfield(poSECCField, "SEUI", 0);

    if (SEUI == INSTRUCTION_INSERT)
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d: SEUI=%d (insert) not supported",
            m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID, SEUI));
    }
    else if (SEUI == INSTRUCTION_DELETE)
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d: SEUI=%d (delete) not supported",
            m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID, SEUI));
    }
    else if (SEUI != INSTRUCTION_UPDATE)
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d: invalid SEUI = %d", m_osFilename.c_str(),
            static_cast<int>(nRCNM), nRCID, SEUI));
    }

    // Segment index
    const int SEIX = poUpdateRecord->GetIntSubfield(poSECCField, "SEIX", 0);
    if (SEIX != 1)
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d: invalid SEIX = %d", m_osFilename.c_str(),
            static_cast<int>(nRCNM), nRCID, SEIX));
    }

    // Number of segments
    const int NSEG = poUpdateRecord->GetIntSubfield(poSECCField, "NSEG", 0);
    if (NSEG != 1)
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d: invalid NSEG = %d", m_osFilename.c_str(),
            static_cast<int>(nRCNM), nRCID, NSEG));
    }

    return ProcessUpdatePointList(poUpdateRecord, poTargetRecord,
                                  /* bIs3DAllowed = */ false);
}
