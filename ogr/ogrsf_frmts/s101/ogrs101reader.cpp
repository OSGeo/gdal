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
#include "ogrs101featurecatalog.h"

#include "cpl_enumerate.h"

#include <limits>
#include <memory>
#include <utility>

/************************************************************************/
/*                           OGRS101Reader()                            */
/************************************************************************/

OGRS101Reader::OGRS101Reader() = default;

/************************************************************************/
/*                           ~OGRS101Reader()                           */
/************************************************************************/

OGRS101Reader::~OGRS101Reader() = default;

/************************************************************************/
/*                         EmitErrorOrWarning()                         */
/************************************************************************/

/*static */ bool
OGRS101Reader::EmitErrorOrWarning(const char *pszFile, const char *pszFunc,
                                  int nLine, const char *pszMsg, bool bError,
                                  bool bRecoverable)
{
#ifdef _WIN32
    const char *lastPathSep = strrchr(pszFile, '\\');
#else
    const char *lastPathSep = strrchr(pszFile, '/');
#endif
    if (lastPathSep)
        pszFile = lastPathSep + 1;

    if (bError)
    {
        if (bRecoverable)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "at %s:%d (%s()): %s\n"
                "You can potentially try to overcome this error by setting "
                "the STRICT open option to FALSE",
                pszFile, nLine, pszFunc, pszMsg);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "at %s:%d (%s()): %s",
                     pszFile, nLine, pszFunc, pszMsg);
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined, "at %s:%d (%s()): %s", pszFile,
                 nLine, pszFunc, pszMsg);
    }
    return !bError;
}

/************************************************************************/
/*                                Load()                                */
/************************************************************************/

/** Load a dataset.
 *
 * Ingests records in the various DDFRecordIndex members and build layer
 * definitions.
 */
bool OGRS101Reader::Load(GDALOpenInfo *poOpenInfo)
{
    if (!poOpenInfo->fpL)
        return false;

    m_bStrict = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "STRICT", "YES"));
    CPLAssert(!m_poModule);
    m_poModule = std::make_unique<DDFModule>();
    m_osFilename = poOpenInfo->pszFilename;
    VSILFILE *fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    if (!m_poModule->Open(m_osFilename.c_str(), false, fp))
        return false;

    if (!m_poModule->FindFieldDefn(DSID_FIELD))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s is an ISO8211 file, but not a S-101 data file.",
                 m_osFilename.c_str());
        return false;
    }

    DDFRecord *poRecord = m_poModule->ReadRecord();
    if (!ReadDatasetGeneralInformationRecord(poRecord))
        return false;

    poRecord = m_poModule->ReadRecord();
    if (!poRecord)
        return EMIT_ERROR("no Dataset Coordinate Reference System record.");
    bool bSkipFirstReadRecord = false;
    if (!m_bStrict && !poRecord->FindField(CSID_FIELD))
    {
        EMIT_ERROR_OR_WARNING("CSID field not found.");
        bSkipFirstReadRecord = true;
    }
    else if (!ReadCSID(poRecord))
        return false;

    return IngestRecords(bSkipFirstReadRecord ? poRecord : nullptr) &&
           ReadFeatureCatalog() && CreateInformationTypeFeatureDefn() &&
           CreatePointFeatureDefns() && CreateMultiPointFeatureDefns() &&
           CreateCurveFeatureDefn() && CreateCompositeCurveFeatureDefn() &&
           CreateSurfaceFeatureDefn() && CreateFeatureTypeFeatureDefns();
}

/************************************************************************/
/*                         ReadFeatureCatalog()                         */
/************************************************************************/

bool OGRS101Reader::ReadFeatureCatalog()
{
    OGRS101FeatureCatalog::LoadingStatus status;
    std::tie(status, m_poFeatureCatalog) =
        OGRS101FeatureCatalog::GetSingletonFeatureCatalog(m_bStrict);
    return status != OGRS101FeatureCatalog::LoadingStatus::ERROR;
}

/************************************************************************/
/*                           IngestRecords()                            */
/************************************************************************/

/** Ingest records into the various m_oXXXXXIndex members
 *
 * @param poRecordIn Already read record, or nullptr to advance to the next one
 */
bool OGRS101Reader::IngestRecords(const DDFRecord *poRecordIn)
{
    // Must NOT be set as static, as it depends on "this" !
    /* NOT STATIC */ const struct
    {
        const char *pszFieldName;
        const char *pszType;
        RecordName nRCNM;
        int nExpectedCount;
        DDFRecordIndex &oIndex;
    } asRecordTypeDesc[] = {
        {IRID_FIELD, "information type", RECORD_NAME_INFORMATION_TYPE,
         m_nCountInformationRecord, m_oInformationTypeRecordIndex},
        {PRID_FIELD, "point", RECORD_NAME_POINT, m_nCountPointRecord,
         m_oPointRecordIndex},
        {MRID_FIELD, "multipoint", RECORD_NAME_MULTIPOINT,
         m_nCountMultiPointRecord, m_oMultiPointRecordIndex},
        {CRID_FIELD, "curve", RECORD_NAME_CURVE, m_nCountCurveRecord,
         m_oCurveRecordIndex},
        {CCID_FIELD, "composite curve", RECORD_NAME_COMPOSITE_CURVE,
         m_nCountCompositeCurveRecord, m_oCompositeCurveRecordIndex},
        {SRID_FIELD, "surface", RECORD_NAME_SURFACE, m_nCountSurfaceRecord,
         m_oSurfaceRecordIndex},
        {FRID_FIELD, "feature type", RECORD_NAME_FEATURE_TYPE,
         m_nCountFeatureTypeRecord, m_oFeatureTypeRecordIndex},
    };

    constexpr int STAR = std::numeric_limits<int>::max();
    using NameOccMinOccMax = std::tuple<const char *, int, int>;
    const std::map<RecordName, std::vector<std::vector<NameOccMinOccMax>>>
        mapExpectedFields = {
            {
                RECORD_NAME_INFORMATION_TYPE,
                {{{IRID_FIELD, 1, 1},
                  {ATTR_FIELD, 0, STAR},
                  {INAS_FIELD, 0, STAR}}},
            },
            {RECORD_NAME_POINT,
             {
                 {{{PRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {C2IT_FIELD, 1, 1}}},
                 {{{PRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {C3IT_FIELD, 1, 1}}},
             }},
            {RECORD_NAME_MULTIPOINT,
             {
                 {{{MRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {C2IL_FIELD, 1, STAR}}},
                 {{{MRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {C3IL_FIELD, 1, STAR}}},
             }},
            {RECORD_NAME_CURVE,
             {
                 {{{CRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {PTAS_FIELD, 1, 1},
                   {SEGH_FIELD, 1, 1},
                   {C2IL_FIELD, 1, STAR}}},
             }},
            {RECORD_NAME_COMPOSITE_CURVE,
             {
                 {{{CCID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {CUCO_FIELD, 1, STAR}}},
             }},
            {RECORD_NAME_SURFACE,
             {
                 {{{SRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {RIAS_FIELD, 1, STAR}}},
             }},
            {RECORD_NAME_FEATURE_TYPE,
             {
                 {{{FRID_FIELD, 1, 1},
                   {FOID_FIELD, 1, 1},
                   {ATTR_FIELD, 0, STAR},
                   {INAS_FIELD, 0, STAR},
                   {SPAS_FIELD, 0, STAR},
                   {FASC_FIELD, 0, STAR},
                   {MASK_FIELD, 0, STAR}}},
             }},
        };

    // Loop through all records (except first two DSID and CRID already parsed)
    // and dispatch them to the appropriate DDFRecordIndex member variable.
    const DDFRecord *poRecord = nullptr;
    const int iFirstRecord = poRecordIn ? 1 : 2;
    for (int iRecord = iFirstRecord;
         poRecordIn || (poRecord = m_poModule->ReadRecord()) != nullptr;
         ++iRecord)
    {
        if (poRecordIn)
            std::swap(poRecord, poRecordIn);

        const auto poField = poRecord->GetField(0);
        if (!poField)
            return EMIT_ERROR(
                CPLSPrintf("Record index %d without field.", iRecord));
        const char *pszFieldName = poField->GetFieldDefn()->GetName();

        // Record name
        const RecordName nRCNM =
            poRecord->GetIntSubfield(pszFieldName, 0, RCNM_SUBFIELD, 0);

        // Record identifier
        const int nRCID =
            poRecord->GetIntSubfield(pszFieldName, 0, RCID_SUBFIELD, 0);

        // Check that the fields found in the record match the expectations
        // from the spec. That is check there are no missing required field,
        // no duplicate field or unexpected field.
        std::vector<std::string> fieldsInRecord;
        for (const auto &poIterField : poRecord->GetFields())
        {
            fieldsInRecord.push_back(poIterField->GetFieldDefn()->GetName());
        }
        const auto oIter = mapExpectedFields.find(nRCNM);
        if (oIter != mapExpectedFields.end())
        {
            bool bMatch = false;
            for (const auto &expectedFields : oIter->second)
            {
                bMatch = true;
                size_t iExpected = 0;
                for (size_t i = 0; bMatch && i < fieldsInRecord.size(); ++i)
                {
                    for (; iExpected < expectedFields.size(); ++iExpected)
                    {
                        if (fieldsInRecord[i] ==
                            std::get<0>(expectedFields[iExpected]))
                        {
                            const int nMaxOcc =
                                std::get<2>(expectedFields[iExpected]);
                            if (nMaxOcc == 1)
                            {
                                if (i > 0 &&
                                    fieldsInRecord[i - 1] == fieldsInRecord[i])
                                {
                                    bMatch = false;
                                }
                                else
                                {
                                    ++iExpected;
                                }
                            }
                            else if (i + 1 == fieldsInRecord.size() ||
                                     fieldsInRecord[i + 1] != fieldsInRecord[i])
                            {
                                ++iExpected;
                            }
                            break;
                        }
                        else
                        {
                            const int nMinOcc =
                                std::get<1>(expectedFields[iExpected]);
                            if (nMinOcc == 1)
                            {
                                bMatch = false;
                                break;
                            }
                        }
                    }

                    // If we have reached the end of expected fields but
                    // there are remaining fields, then there are missing
                    // compulsory fields.
                    if (iExpected == expectedFields.size() &&
                        i + 1 < fieldsInRecord.size())
                    {
                        bMatch = false;
                        break;
                    }
                }

                // Skip optional fields after the last one in the record
                if (bMatch && iExpected < expectedFields.size())
                {
                    for (; iExpected < expectedFields.size(); ++iExpected)
                    {
                        const int nMinOcc =
                            std::get<1>(expectedFields[iExpected]);
                        if (nMinOcc > 0)
                        {
                            bMatch = false;
                            break;
                        }
                    }
                }
                bMatch = bMatch && iExpected == expectedFields.size();
                if (bMatch)
                    break;
            }
            if (!bMatch &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Record index %d, RCNM=%d, RCID=%d: invalid "
                    "sequence of fields.",
                    iRecord, static_cast<int>(nRCNM), static_cast<int>(nRCID))))
            {
                return false;
            }
        }

        const auto iterRecordTypeDesc = std::find_if(
            std::begin(asRecordTypeDesc), std::end(asRecordTypeDesc),
            [pszFieldName](const auto &sRecordTypeDesc)
            {
                return strcmp(pszFieldName, sRecordTypeDesc.pszFieldName) == 0;
            });
        if (iterRecordTypeDesc != std::end(asRecordTypeDesc))
        {
            const auto &sRecordTypeDesc = *iterRecordTypeDesc;

            if (nRCNM != sRecordTypeDesc.nRCNM &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Record index %d: invalid value %d for RCNM field of %s.",
                    iRecord, static_cast<int>(nRCNM), pszFieldName)))
            {
                return false;
            }

            if (nRCID <= 0)
            {
                if (!EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("Record index %d: invalid value %d for "
                                   "RCID subfield of %s.",
                                   iRecord, nRCID, pszFieldName)))
                {
                    return false;
                }
                break;
            }

            if (sRecordTypeDesc.oIndex.FindRecord(nRCID) &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Record index %d: several %s records have RCID = %d.",
                    iRecord, pszFieldName, nRCID)))
            {
                return false;
            }
            sRecordTypeDesc.oIndex.AddRecord(nRCID, poRecord->Clone());
        }
        else if (!EMIT_ERROR_OR_WARNING(
                     CPLSPrintf("Record index %d: unknown field name %s.",
                                iRecord, pszFieldName)))
        {
            return false;
        }
    }

    // Check consistency between number of records of each category (information
    // type, point, etc.) and the number actually found.
    for (const auto &sRecordTypeDesc : asRecordTypeDesc)
    {
        if (sRecordTypeDesc.oIndex.GetCount() !=
                sRecordTypeDesc.nExpectedCount &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "%d %s records mentioned in DSSI field, but %d actually found.",
                sRecordTypeDesc.nExpectedCount, sRecordTypeDesc.pszType,
                sRecordTypeDesc.oIndex.GetCount())))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                FillFeatureWithNonAttrAssocSubfields()                */
/************************************************************************/

/** Fill attribute fields of the provided feature with the fixed subfields
 * of the INAS or FASC field.
 */
bool OGRS101Reader::FillFeatureWithNonAttrAssocSubfields(
    const DDFRecord *poRecord, int iRecord, const char *pszFieldName,
    OGRFeature &oFeature) const
{
    const bool bIsINAS = EQUAL(pszFieldName, INAS_FIELD);

    const auto apoAssocFields = poRecord->GetFields(pszFieldName);
    const bool bMultipleAssocs = oFeature.GetDefnRef()->GetFieldIndex(
                                     bIsINAS ? OGR_FIELD_NAME_REF_INFO_RID
                                             : OGR_FIELD_NAME_REF_FEAT_RID) < 0;
    const auto &assocRecord =
        bIsINAS ? m_oInformationTypeRecordIndex : m_oFeatureTypeRecordIndex;

    for (const auto &[iField, poField] : cpl::enumerate(apoAssocFields))
    {
        const std::string osSuffix =
            bMultipleAssocs ? CPLSPrintf("[%d]", static_cast<int>(iField) + 1)
                            : "";

        const auto GetErrorContext = [poRecord, iRecord]()
        {
            const auto poIDField = poRecord->GetField(0);
            CPLAssert(poIDField);
            const char *pszIDFieldName = poIDField->GetFieldDefn()->GetName();
            return CPLSPrintf("Record index=%d of %s", iRecord, pszIDFieldName);
        };

        const RecordName nRRNM =
            poRecord->GetIntSubfield(poField, RRNM_SUBFIELD, 0);
        const RecordName nExpectedRRNM =
            bIsINAS ? RECORD_NAME_INFORMATION_TYPE : RECORD_NAME_FEATURE_TYPE;
        if (nRRNM != nExpectedRRNM)
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s: Invalid value for RRNM subfield of %s field: "
                    "got %d, expected %d.",
                    GetErrorContext(), pszFieldName, static_cast<int>(nRRNM),
                    static_cast<int>(nExpectedRRNM))))
            {
                return false;
            }
        }

        const int nRRID = poRecord->GetIntSubfield(poField, RRID_SUBFIELD, 0);
        if (!assocRecord.FindRecord(nRRID))
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s: Invalid value %d for RRID subfield of %s field: "
                    "does not match the record identifier of an existing "
                    "InformationType record.",
                    GetErrorContext(), static_cast<int>(nRRID), pszFieldName)))
            {
                return false;
            }
        }

        if (bIsINAS)
        {
            oFeature.SetField((OGR_FIELD_NAME_REF_INFO_RID + osSuffix).c_str(),
                              nRRID);

            const InfoAssocCode nNIAC =
                poRecord->GetIntSubfield(poField, NIAC_SUBFIELD, 0);
            const auto iterIAC = m_informationAssociationCodes.find(nNIAC);
            if (iterIAC == m_informationAssociationCodes.end())
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: cannot find attribute code %d in IACS field "
                        "of the Dataset General Information Record.",
                        GetErrorContext(), static_cast<int>(nNIAC))))
                {
                    return false;
                }
                else
                {
                    oFeature.SetField((OGR_FIELD_NAME_NIAC + osSuffix).c_str(),
                                      CPLSPrintf("informationAssociationCode%d",
                                                 static_cast<int>(nNIAC)));
                }
            }
            else
            {
                oFeature.SetField((OGR_FIELD_NAME_NIAC + osSuffix).c_str(),
                                  iterIAC->second.c_str());
            }
        }
        else
        {
            const auto oIterFID = m_oMapFeatureTypeIdToFDefn.find(nRRID);
            if (oIterFID != m_oMapFeatureTypeIdToFDefn.end())
            {
                oFeature.SetField(
                    (OGR_FIELD_NAME_REF_FEAT_LAYER_NAME + osSuffix).c_str(),
                    oIterFID->second->GetName());
            }

            oFeature.SetField((OGR_FIELD_NAME_REF_FEAT_RID + osSuffix).c_str(),
                              nRRID);

            const FeatureAssocCode nNFAC =
                poRecord->GetIntSubfield(poField, NFAC_SUBFIELD, 0);
            const auto iterFAC = m_featureAssociationCodes.find(nNFAC);
            if (iterFAC == m_featureAssociationCodes.end())
            {
                if (!EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("%s: cannot find feature association code "
                                   "%d in FACS field "
                                   "of the Dataset General Information Record.",
                                   GetErrorContext(), static_cast<int>(nNFAC))))
                {
                    return false;
                }
                else
                {
                    oFeature.SetField((OGR_FIELD_NAME_NFAC + osSuffix).c_str(),
                                      CPLSPrintf("featureAssociationCode%d",
                                                 static_cast<int>(nNFAC)));
                }
            }
            else
            {
                oFeature.SetField((OGR_FIELD_NAME_NFAC + osSuffix).c_str(),
                                  iterFAC->second.c_str());
            }
        }

        const AssocRoleCode nNARC =
            poRecord->GetIntSubfield(poField, NARC_SUBFIELD, 0);
        const auto iterARC = m_associationRoleCodes.find(nNARC);
        if (iterARC == m_associationRoleCodes.end())
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s: cannot find attribute code %d in ARCS field "
                    "of the Dataset General Information Record.",
                    GetErrorContext(), static_cast<int>(nNARC))))
            {
                return false;
            }
            else
            {
                oFeature.SetField(((bIsINAS ? OGR_FIELD_NAME_NARC
                                            : OGR_FIELD_NAME_FEATURE_NARC) +
                                   osSuffix)
                                      .c_str(),
                                  CPLSPrintf("associationRoleCode%d",
                                             static_cast<int>(nNARC)));
            }
        }
        else
        {
            oFeature.SetField(
                ((bIsINAS ? OGR_FIELD_NAME_NARC : OGR_FIELD_NAME_FEATURE_NARC) +
                 osSuffix)
                    .c_str(),
                iterARC->second.c_str());
        }

        const char *pszSubFieldName = bIsINAS ? IUIN_SUBFIELD : FAUI_SUBFIELD;
        const int nInstruction =
            poRecord->GetIntSubfield(poField, pszSubFieldName, 0);
        if (nInstruction != INSTRUCTION_INSERT)
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf("%s: wrong value %d for %s "
                                                  "subfield of %s field.",
                                                  GetErrorContext(),
                                                  nInstruction, pszSubFieldName,
                                                  pszFieldName)))
            {
                return false;
            }
        }
    }

    return true;
}
