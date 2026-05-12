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

            constexpr const char *RAUI_SUBFIELD = "RAUI";
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

            constexpr const char *USAG_SUBFIELD = "USAG";
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
