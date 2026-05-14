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

#include <algorithm>
#include <memory>

/************************************************************************/
/*                      CreateSurfaceFeatureDefn()                      */
/************************************************************************/

/** Create the feature definition for the Surface layer
 */
bool OGRS101Reader::CreateSurfaceFeatureDefn()
{
    if (m_oSurfaceRecordIndex.GetCount() > 0)
    {
        m_poFeatureDefnSurface =
            OGRFeatureDefnRefCountedPtr::makeInstance(OGR_LAYER_NAME_SURFACE);
        m_poFeatureDefnSurface->SetGeomType(wkbPolygon);
        auto oSRSIter = m_oMapSRS.find(HORIZONTAL_CRS_ID);
        if (oSRSIter != m_oMapSRS.end())
        {
            m_poFeatureDefnSurface->GetGeomFieldDefn(0)->SetSpatialRef(
                OGRSpatialReferenceRefCountedPtr::makeClone(&oSRSIter->second)
                    .get());
        }
        m_poFeatureDefnSurface->GetGeomFieldDefn(0)->SetCoordinatePrecision(
            m_coordinatePrecision);
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_ID, OFTInteger);
            m_poFeatureDefnSurface->AddFieldDefn(&oFieldDefn);
        }
        {
            OGRFieldDefn oFieldDefn(OGR_FIELD_NAME_RECORD_VERSION, OFTInteger);
            m_poFeatureDefnSurface->AddFieldDefn(&oFieldDefn);
        }
        if (!InferFeatureDefn(m_oSurfaceRecordIndex, SRID_FIELD, INAS_FIELD, {},
                              *m_poFeatureDefnSurface, m_oMapFieldDomains))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                        ReadSurfaceGeometry()                         */
/************************************************************************/

std::unique_ptr<OGRPolygon>
OGRS101Reader::ReadSurfaceGeometry(const DDFRecord *poRecord, int iRecord,
                                   int nRecordID,
                                   const OGRSpatialReference *poSRS) const
{
    if (nRecordID < 0)
        nRecordID = poRecord->GetIntSubfield(SRID_FIELD, 0, RCID_SUBFIELD, 0);

    const auto GetErrorContext = [iRecord, nRecordID]()
    {
        if (iRecord >= 0)
            return CPLSPrintf("Record index=%d of SRID", iRecord);
        else
            return CPLSPrintf("Record ID=%d of SRID", nRecordID);
    };

    if (!poRecord->FindField(RIAS_FIELD))
    {
        CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s: no RIAS field", GetErrorContext())));
        return nullptr;
    }

    auto poSurface = std::make_unique<OGRPolygon>();
    poSurface->assignSpatialReference(poSRS);
    std::unique_ptr<OGRLinearRing> poExteriorRing;
    std::vector<std::unique_ptr<OGRLinearRing>> apoInteriorRings;
    for (const auto *poRIASField : poRecord->GetFields(RIAS_FIELD))
    {
        const int nRings = poRIASField->GetRepeatCount();
        for (int iRing = 0; iRing < nRings; ++iRing)
        {
            const auto GetIntSubfield =
                [poRecord, poRIASField, iRing](const char *pszSubFieldName)
            {
                return poRecord->GetIntSubfield(poRIASField, pszSubFieldName,
                                                iRing);
            };

            const int nRAUI = GetIntSubfield(RAUI_SUBFIELD);
            if (nRAUI != INSTRUCTION_INSERT)
            {
                CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s: wrong value %d for RAUI "
                               "subfield of %d instance of RIAS field.",
                               GetErrorContext(), nRAUI, iRing)));
                return nullptr;
            }

            const RecordName nRRNM = GetIntSubfield(RRNM_SUBFIELD);
            if (nRRNM != RECORD_NAME_CURVE &&
                nRRNM != RECORD_NAME_COMPOSITE_CURVE)
            {
                CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s: Invalid value for RRNM "
                    "subfield of %d instance of RIAS field: "
                    "got %d, expected %d or %d.",
                    GetErrorContext(), iRing, static_cast<int>(nRRNM),
                    static_cast<int>(RECORD_NAME_CURVE),
                    static_cast<int>(RECORD_NAME_COMPOSITE_CURVE))));
                return nullptr;
            }

            const int nRRID = GetIntSubfield(RRID_SUBFIELD);
            std::unique_ptr<OGRLineString> poCurvePart;
            if (nRRNM == RECORD_NAME_CURVE)
            {
                const auto poCurveRecord =
                    m_oCurveRecordIndex.FindRecord(nRRID);
                if (!poCurveRecord)
                {
                    CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: Value (RRNM=%d, RRID=%d) "
                        "of instance %d of RIAS field does not point to an "
                        "existing record.",
                        GetErrorContext(), static_cast<int>(nRRNM), nRRID,
                        iRing)));
                    return nullptr;
                }

                poCurvePart = ReadCurveGeometry(
                    poCurveRecord, /* iRecord = */ -1, nRRID, poSRS);
            }
            else
            {
                CPLAssert(nRRNM == RECORD_NAME_COMPOSITE_CURVE);

                const auto poCompositeCurveRecord =
                    m_oCompositeCurveRecordIndex.FindRecord(nRRID);
                if (!poCompositeCurveRecord)
                {
                    CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: Value (RRNM=%d, RRID=%d) "
                        "of instance %d of RIAS field does not point to an "
                        "existing record.",
                        GetErrorContext(), static_cast<int>(nRRNM), nRRID,
                        iRing)));
                    return nullptr;
                }

                poCurvePart = ReadCompositeCurveGeometry(
                    poCompositeCurveRecord, /* iRecord = */ -1, nRRID, poSRS);
            }

            if (!poCurvePart)
            {
                return nullptr;
            }

            bool bReverse = false;
            const int nORNT = GetIntSubfield(ORNT_SUBFIELD);
            if (nORNT == ORNT_REVERSE)
            {
                bReverse = true;
            }
            else if (nORNT != ORNT_FORWARD)
            {
                CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s: Invalid value for ORNT "
                               "subfield of %d instance of RIAS field: "
                               "got %d, expected %d or %d.",
                               GetErrorContext(), iRing, nORNT, ORNT_FORWARD,
                               ORNT_REVERSE)));
                return nullptr;
            }

            auto poRing = std::make_unique<OGRLinearRing>();
            if (bReverse && poCurvePart->getNumPoints() > 0)
                poRing->addSubLineString(poCurvePart.get(),
                                         poCurvePart->getNumPoints() - 1, 0);
            else
                poRing->addSubLineString(poCurvePart.get());
            if (poRing->getNumPoints() <= 2 || !poRing->get_IsClosed())
            {
                CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s: Ring of index %d is not closed.",
                               GetErrorContext(), iRing)));
                return nullptr;
            }

            constexpr int USAG_EXTERIOR = 1;  // Exterior ring
            constexpr int USAG_INTERIOR = 2;  // Interior ring
            const int nUSAG =
                poRecord->GetIntSubfield(RIAS_FIELD, 0, USAG_SUBFIELD, iRing);
            if (nUSAG == USAG_EXTERIOR)
            {
                if (poExteriorRing)
                {
                    CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: several rings tagged as exterior rings.",
                        GetErrorContext())));
                    return nullptr;
                }
                if (!poRing->isClockwise())
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s: exterior ring orientation is not clockwise.",
                            GetErrorContext())))
                    {
                        return nullptr;
                    }
                }
                poExteriorRing = std::move(poRing);
            }
            else if (nUSAG == USAG_INTERIOR)
            {
                if (poRing->isClockwise())
                {
                    if (!EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("%s: orientation of interior ring of "
                                       "index %d is not counter-clockwise.",
                                       GetErrorContext(), iRing)))
                    {
                        return nullptr;
                    }
                }
                apoInteriorRings.push_back(std::move(poRing));
            }
            else
            {
                CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s: Invalid value for USAG "
                               "subfield of %d instance of RIAS field: "
                               "got %d, expected %d or %d.",
                               GetErrorContext(), iRing, nUSAG, USAG_EXTERIOR,
                               USAG_INTERIOR)));
                return nullptr;
            }
        }
    }

    if (!poExteriorRing)
    {
        CPL_IGNORE_RET_VAL(EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s: no ring tagged as exterior ring.", GetErrorContext())));
        return nullptr;
    }

    poSurface->addRing(std::move(poExteriorRing));
    for (auto &poRing : apoInteriorRings)
        poSurface->addRing(std::move(poRing));

    if (OGRGeometryFactory::haveGEOS())
    {
        std::string osReason;
        if (!poSurface->IsValid(&osReason))
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf("%s: surface is invalid: %s.",
                                                  GetErrorContext(),
                                                  osReason.c_str())))
            {
                return nullptr;
            }
        }
    }

    return poSurface;
}

/************************************************************************/
/*                         FillFeatureSurface()                         */
/************************************************************************/

/** Fill the content of the provided feature from the identified record
 * (of m_oSurfaceRecordIndex).
 */
bool OGRS101Reader::FillFeatureSurface(const DDFRecordIndex &oIndex,
                                       int iRecord, OGRFeature &oFeature) const
{
    const auto poRecord = oIndex.GetByIndex(iRecord);
    CPLAssert(poRecord);

    const OGRSpatialReference *poSRS =
        oFeature.GetDefnRef()->GetGeomFieldDefn(0)->GetSpatialRef();
    auto poSurface =
        ReadSurfaceGeometry(poRecord, iRecord, /* nRecordID = */ -1, poSRS);
    if (!poSurface)
    {
        if (m_bStrict)
            return false;  // error message already emitted
    }
    else
    {
        oFeature.SetGeometry(std::move(poSurface));
    }

    return FillFeatureAttributes(oIndex, iRecord, INAS_FIELD, oFeature) &&
           FillFeatureWithNonAttrAssocSubfields(poRecord, iRecord, INAS_FIELD,
                                                oFeature);
}

/************************************************************************/
/*                     ProcessUpdateRecordSurface()                     */
/************************************************************************/

/** Updates the geometry part of poTargetRecord with poUpdateRecord */
bool OGRS101Reader::ProcessUpdateRecordSurface(const DDFRecord *poUpdateRecord,
                                               DDFRecord *poTargetRecord) const
{
    const auto poIDField = poUpdateRecord->GetField(0);
    CPLAssert(poIDField);

    // Record name
    const RecordName nRCNM =
        poUpdateRecord->GetIntSubfield(poIDField, RCNM_SUBFIELD, 0);

    // Record identifier
    const int nRCID =
        poUpdateRecord->GetIntSubfield(poIDField, RCID_SUBFIELD, 0);

    // Ring Association Field field
    const auto apoUpdateFields = poUpdateRecord->GetFields(RIAS_FIELD);
    if (apoUpdateFields.empty())
        return true;

    struct Ring
    {
        int RRNM = 0;
        int RRID = 0;
        int ORNT = 0;
        int USAG = 0;
        int RAUI = 0;

        static Ring Read(const DDFRecord *poRecord, const DDFField *poField,
                         int i)
        {
            Ring ring;
            ring.RRNM = poRecord->GetIntSubfield(poField, RRNM_SUBFIELD, i);
            ring.RRID = poRecord->GetIntSubfield(poField, RRID_SUBFIELD, i);
            ring.ORNT = poRecord->GetIntSubfield(poField, ORNT_SUBFIELD, i);
            ring.USAG = poRecord->GetIntSubfield(poField, USAG_SUBFIELD, i);
            ring.RAUI = poRecord->GetIntSubfield(poField, RAUI_SUBFIELD, i);
            return ring;
        }
    };

    std::vector<Ring> asTarget;
    // Ingest the existing/target record(s)
    auto apoTargetFields = poTargetRecord->GetFields(RIAS_FIELD);
    for (auto *poTargetField : apoTargetFields)
    {
        const int nTargetRepeatCount = poTargetField->GetRepeatCount();
        for (int i = 0; i < nTargetRepeatCount; ++i)
        {
            asTarget.push_back(Ring::Read(poTargetRecord, poTargetField, i));
        }
        poTargetRecord->DeleteField(poTargetField);
    }

    // Apply the update record(s)
    for (auto *poUpdateField : apoUpdateFields)
    {
        const int nUpdateRepeatCount = poUpdateField->GetRepeatCount();
        for (int i = 0; i < nUpdateRepeatCount; ++i)
        {
            Ring ring = Ring::Read(poUpdateRecord, poUpdateField, i);
            if (ring.RAUI == INSTRUCTION_INSERT)
            {
                asTarget.push_back(std::move(ring));
            }
            else if (ring.RAUI == INSTRUCTION_DELETE)
            {
                bool bMatchFound = false;
                for (size_t j = 0; j < asTarget.size(); ++j)
                {
                    if (asTarget[j].RRNM == ring.RRNM &&
                        asTarget[j].RRID == ring.RRID)
                    {
                        bMatchFound = true;
                        asTarget.erase(asTarget.begin() + j);
                        break;
                    }
                }
                if (!bMatchFound &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s, RCNM=%d, RCID=%d, RIAS iSubField=%d: update "
                        "field references RRNM=%d, RRID=%d which does not "
                        "exist in initial or previous update",
                        m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID, i,
                        ring.RRNM, ring.RRID)))
                {
                    return false;
                }
            }
            else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                         "%s, RCNM=%d, RCID=%d, SPAS iSubField=%d: invalid "
                         "RAUI=%d",
                         m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                         i, ring.RAUI)))
            {
                return false;
            }
        }
    }

    if (!asTarget.empty())
    {
        auto poRIASFieldDefn = m_oMainModule.FindFieldDefn(RIAS_FIELD);
        if (!poRIASFieldDefn)
        {
            return EMIT_ERROR("Cannot find RIAS field definition");
        }
        auto poRIASFieldTarget = poTargetRecord->AddField(poRIASFieldDefn);
        CPLAssert(poRIASFieldTarget);
        const auto poRIASFieldUpdate = apoUpdateFields[0];
        if (*(poRIASFieldTarget->GetFieldDefn()) !=
            *(poRIASFieldUpdate->GetFieldDefn()))
        {
            return EMIT_ERROR("RIAS field definitions of update and target "
                              "records are different");
        }

        // Compose raw target field
        std::string s;
        for (const auto &cc : asTarget)
        {
            AppendUInt8(s, static_cast<uint8_t>(cc.RRNM));
            AppendInt32(s, cc.RRID);
            AppendUInt8(s, static_cast<uint8_t>(cc.ORNT));
            AppendUInt8(s, static_cast<uint8_t>(cc.USAG));
            AppendUInt8(s, static_cast<uint8_t>(cc.RAUI));
        }
        AppendUInt8(s, DDF_FIELD_TERMINATOR);

        poTargetRecord->SetFieldRaw(poRIASFieldTarget, s.data(),
                                    static_cast<int>(s.size()));
    }

    return true;
}
