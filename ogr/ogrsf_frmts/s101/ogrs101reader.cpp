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
    VSILFILE *fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    if (!Load(poOpenInfo->pszFilename, fp, &m_oMainModule))
        return false;

    m_aosMetadata.SetNameValue("STATUS", "VALID");

    // Browse and load update files
    if (poOpenInfo->IsExtensionEqualToCI("000") &&
        EQUAL(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "UPDATES",
                                   "APPLY"),
              "APPLY"))
    {
        m_bInUpdate = true;
        for (int iUpdate = 1; iUpdate <= 999; ++iUpdate)
        {
            DDFModule oTmpModule;
            const std::string osUpdateFilename = CPLResetExtensionSafe(
                poOpenInfo->pszFilename, CPLSPrintf("%03d", iUpdate));
            VSIStatBufL sStatBuf;
            if (VSIStatL(osUpdateFilename.c_str(), &sStatBuf) != 0)
                break;
            CPLDebug("S101", "Loading update file %s",
                     osUpdateFilename.c_str());
            if (!Load(osUpdateFilename.c_str(), nullptr, &oTmpModule))
                return false;
            if (m_bCancelled)
                break;
        }
        m_bInUpdate = false;
    }

    return ReadFeatureCatalog() && CreateInformationTypeFeatureDefn() &&
           CreatePointFeatureDefns() && CreateMultiPointFeatureDefns() &&
           CreateCurveFeatureDefn() && CreateCompositeCurveFeatureDefn() &&
           CreateSurfaceFeatureDefn() && CreateFeatureTypeFeatureDefns();
}

/************************************************************************/
/*                                Load()                                */
/************************************************************************/

bool OGRS101Reader::Load(const std::string &osFilename, VSILFILE *fp,
                         DDFModule *poCurModule)
{
    m_osFilename = osFilename;
    if (!poCurModule->Open(m_osFilename.c_str(), false, fp))
        return false;

    if (!poCurModule->FindFieldDefn(DSID_FIELD))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s is an ISO8211 file, but not a S-101 data file.",
                 m_osFilename.c_str());
        return false;
    }

    DDFRecord *poRecord = poCurModule->ReadRecord();
    if (!ReadDatasetGeneralInformationRecord(poRecord))
        return false;

    poRecord = poCurModule->ReadRecord();
    if (m_bInUpdate)
    {
        if (!poRecord)
            return true;
        if (poRecord->FindField(CSID_FIELD))
        {
            if (!EMIT_ERROR_OR_WARNING(
                    "CSID field found in update file but not expected."))
                return false;
            poRecord = poCurModule->ReadRecord();
            if (!poRecord)
                return true;
        }

        return IngestUpdateRecords(poRecord, poCurModule);
    }
    else
    {
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

        return IngestInitialRecords(bSkipFirstReadRecord ? poRecord : nullptr);
    }
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
/*                       CheckFieldDefinitions()                        */
/************************************************************************/

/** Check that the fields found in the record match the expectations
 * from the spec. That is check there are no missing required field,
 * no duplicate field or unexpected field.
 */
bool OGRS101Reader::CheckFieldDefinitions(
    const DDFRecord *poRecord, int iRecord, RecordName nRCNM, int nRCID,
    const std::map<RecordName, std::vector<std::vector<NameOccMinOccMax>>>
        &mapExpectedFields) const
{
    std::vector<std::string> fieldsInRecord;
    for (const auto &poField : poRecord->GetFields())
    {
        fieldsInRecord.push_back(poField->GetFieldDefn()->GetName());
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
                    const int nMinOcc = std::get<1>(expectedFields[iExpected]);
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
        if (!bMatch)
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Record index %d, RCNM=%d, RCID=%d: invalid "
                    "sequence of fields.",
                    iRecord, static_cast<int>(nRCNM), static_cast<int>(nRCID))))
            {
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                        IngestInitialRecords()                        */
/************************************************************************/

/** Ingest records of .000 initial file into the various m_oXXXXXIndex members
 *
 * @param poRecordIn Already read record, or nullptr to advance to the next one
 */
bool OGRS101Reader::IngestInitialRecords(const DDFRecord *poRecordIn)
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
         poRecordIn || (poRecord = m_oMainModule.ReadRecord()) != nullptr;
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

        if (!CheckFieldDefinitions(poRecord, iRecord, nRCNM, nRCID,
                                   mapExpectedFields))
            return false;

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
                !EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("Record index %d: invalid value %d for RCNM "
                               "subfield of %s.",
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
/*                        IngestUpdateRecords()                         */
/************************************************************************/

/** Ingest records of .00x (x>=1) update file into the various m_oXXXXXIndex members
 *
 * @param poRecordIn Already read record
 */
bool OGRS101Reader::IngestUpdateRecords(const DDFRecord *poRecordIn,
                                        DDFModule *poCurModule)
{
    // Must NOT be set as static, as it depends on "this" !
    /* NOT STATIC */ const struct
    {
        const char *pszFieldName;
        const char *pszType;
        RecordName nRCNM;
        int &nExpectedCount;
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
                   {C2IT_FIELD, 0, 1}}},
                 {{{PRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {C3IT_FIELD, 0, 1}}},
             }},
            {RECORD_NAME_MULTIPOINT,
             {
                 {{{MRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {COCC_FIELD, 0, 1},
                   {C2IL_FIELD, 0, STAR}}},
                 {{{MRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {COCC_FIELD, 0, 1},
                   {C3IL_FIELD, 0, STAR}}},
             }},
            {RECORD_NAME_CURVE,
             {
                 {{{CRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {PTAS_FIELD, 0, 1},
                   {SECC_FIELD, 0, 1},
                   {SEGH_FIELD, 0, 1},
                   {COCC_FIELD, 0, 1},
                   {C2IL_FIELD, 0, STAR}}},
             }},
            {RECORD_NAME_COMPOSITE_CURVE,
             {
                 {{{CCID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {CCOC_FIELD, 0, STAR},
                   {CUCO_FIELD, 0, STAR}}},
             }},
            {RECORD_NAME_SURFACE,
             {
                 {{{SRID_FIELD, 1, 1},
                   {INAS_FIELD, 0, STAR},
                   {RIAS_FIELD, 0, STAR}}},
             }},
            {RECORD_NAME_FEATURE_TYPE,
             {
                 {{{FRID_FIELD, 1, 1},
                   {FOID_FIELD, 0, 1},
                   {ATTR_FIELD, 0, STAR},
                   {INAS_FIELD, 0, STAR},
                   {SPAS_FIELD, 0, STAR},
                   {FASC_FIELD, 0, STAR},
                   {MASK_FIELD, 0, STAR}}},
             }},
        };

    // Loop through all records (except first DSID already parsed)
    // and dispatch them to the appropriate DDFRecordIndex member variable.
    const DDFRecord *poRecord = nullptr;
    const int iFirstRecord = 1;
    for (int iRecord = iFirstRecord;
         poRecordIn || (poRecord = poCurModule->ReadRecord()) != nullptr;
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

        if (!CheckFieldDefinitions(poRecord, iRecord, nRCNM, nRCID,
                                   mapExpectedFields))
            return false;

        // Record version
        const int nRVER =
            poRecord->GetIntSubfield(pszFieldName, 0, RVER_SUBFIELD, 0);

        // Record update instruction
        const int nRUIN =
            poRecord->GetIntSubfield(pszFieldName, 0, RUIN_SUBFIELD, 0);

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
                !EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("Record index %d: invalid value %d for RCNM "
                               "subfield of %s.",
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

            DDFRecord *poExistingRecord =
                sRecordTypeDesc.oIndex.FindRecord(nRCID);
            if (nRUIN == INSTRUCTION_UPDATE || nRUIN == INSTRUCTION_DELETE)
            {
                if (!poExistingRecord)
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "Record index %d, RCNM=%d, RCID=%d: no such "
                            "record.",
                            iRecord, static_cast<int>(nRCNM), nRCID)))
                    {
                        return false;
                    }
                    continue;
                }

                const int nOldRVER = poExistingRecord->GetIntSubfield(
                    pszFieldName, 0, RVER_SUBFIELD, 0);
                if (nRVER != nOldRVER + 1)
                {
                    if (!EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("Record index %d, RCNM=%d, RCID=%d: got "
                                       "RVER=%d, expected %d.",
                                       iRecord, static_cast<int>(nRCNM), nRCID,
                                       nRVER, nOldRVER + 1)))
                    {
                        return false;
                    }
                }
            }

            if (nRUIN == INSTRUCTION_INSERT)
            {
                if (poExistingRecord &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Record index %d: several %s records have RCID = %d.",
                        iRecord, pszFieldName, nRCID)))
                {
                    return false;
                }

                auto poClone = poRecord->Clone();
                if (!UpdateCodesInRecord(poClone.get()))
                    return false;

                if (!poClone->TransferTo(&m_oMainModule))
                    return false;

                sRecordTypeDesc.oIndex.AddRecord(nRCID, std::move(poClone));

                sRecordTypeDesc.nExpectedCount++;
            }
            else if (nRUIN == INSTRUCTION_UPDATE)
            {
                CPLAssert(poExistingRecord);

                auto poClone = poRecord->Clone();
                if (!UpdateCodesInRecord(poClone.get()))
                    return false;

                if (!ProcessUpdateRecord(poClone.get(), poExistingRecord))
                    return false;
            }
            else if (nRUIN == INSTRUCTION_DELETE)
            {
                CPLAssert(poExistingRecord);

                sRecordTypeDesc.oIndex.RemoveRecord(nRCID);

                sRecordTypeDesc.nExpectedCount--;
            }
            else if (!EMIT_ERROR_OR_WARNING(
                         CPLSPrintf("Record index %d, RCNM=%d, RCID=%d: wrong "
                                    "value %d for RUIN "
                                    "subfield of %s field.",
                                    iRecord, static_cast<int>(nRCNM), nRCID,
                                    nRUIN, pszFieldName)))
            {
                return false;
            }
        }
        else if (!EMIT_ERROR_OR_WARNING(
                     CPLSPrintf("Record index %d: unknown field name %s.",
                                iRecord, pszFieldName)))
        {
            return false;
        }
    }

#ifdef DEBUG
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
#endif

    return true;
}

/************************************************************************/
/*                        UpdateCodesInRecord()                         */
/************************************************************************/

/** Update NITC, NFTC, NATC, NIAC, NFAC and NARC subfields from the value
 * used in the update file to the value of the merged dataset.
 */
bool OGRS101Reader::UpdateCodesInRecord(DDFRecord *poRecord) const
{
    const auto poIDField = poRecord->GetField(0);
    CPLAssert(poIDField);
    const char *pszIDFieldName = poIDField->GetFieldDefn()->GetName();
    CPLAssert(pszIDFieldName);

    // Record name
    const RecordName nRCNM =
        poRecord->GetIntSubfield(pszIDFieldName, 0, RCNM_SUBFIELD, 0);

    // Record identifier
    const int nRCID =
        poRecord->GetIntSubfield(pszIDFieldName, 0, RCID_SUBFIELD, 0);

    if (EQUAL(pszIDFieldName, IRID_FIELD))
    {
        const InfoTypeCode nNITC(
            poRecord->GetIntSubfield(poIDField, NITC_SUBFIELD, 0));

        const auto oIter = m_informationTypeCodesRemapping.find(nNITC);
        if (oIter == m_informationTypeCodesRemapping.end())
        {
            if (!EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s, RCNM=%d, RCID=%d: unknown NITC=%d",
                               m_osFilename.c_str(), static_cast<int>(nRCNM),
                               nRCID, static_cast<int>(nNITC))))
            {
                return false;
            }
        }
        else
        {
            poRecord->SetIntSubfield(pszIDFieldName, 0, NITC_SUBFIELD, 0,
                                     static_cast<int>(oIter->second));
        }
    }
    else if (EQUAL(pszIDFieldName, FRID_FIELD))
    {
        const FeatureTypeCode nNFTC(
            poRecord->GetIntSubfield(poIDField, NFTC_SUBFIELD, 0));

        const auto oIter = m_featureTypeCodesRemapping.find(nNFTC);
        if (oIter == m_featureTypeCodesRemapping.end())
        {
            if (!EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s, RCNM=%d, RCID=%d: unknown NFTC=%d",
                               m_osFilename.c_str(), static_cast<int>(nRCNM),
                               nRCID, static_cast<int>(nNFTC))))
            {
                return false;
            }
        }
        else
        {
            poRecord->SetIntSubfield(pszIDFieldName, 0, NFTC_SUBFIELD, 0,
                                     static_cast<int>(oIter->second));
        }
    }

    for (const char *pszFieldName : {ATTR_FIELD, INAS_FIELD, FASC_FIELD})
    {
        auto apoFields = poRecord->GetFields(pszFieldName);
        for (auto [fieldIdxSizeT, poField] : cpl::enumerate(apoFields))
        {
            const int fieldIdx = static_cast<int>(fieldIdxSizeT);
            const int nRepeatCount =
                poField->GetParts().size() == 2
                    ? poField->GetParts()[1]->GetRepeatCount()
                    : poField->GetRepeatCount();
            for (int iSubField = 0; iSubField < nRepeatCount; ++iSubField)
            {
                const AttrCode nNATC =
                    poRecord->GetIntSubfield(poField, NATC_SUBFIELD, iSubField);
                const auto oIter = m_attributeCodesRemapping.find(nNATC);
                if (oIter == m_attributeCodesRemapping.end())
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s, RCNM=%d, RCID=%d, %s[iField=%d, "
                            "iSubField=%d]: unknown NATC=%d",
                            m_osFilename.c_str(), static_cast<int>(nRCNM),
                            nRCID, pszFieldName, fieldIdx, iSubField,
                            static_cast<int>(nNATC))))
                    {
                        return false;
                    }
                }
                else
                {
                    poRecord->SetIntSubfield(pszFieldName, fieldIdx,
                                             NATC_SUBFIELD, iSubField,
                                             static_cast<int>(oIter->second));
                }
            }
        }
    }

    for (const char *pszFieldName : {INAS_FIELD, FASC_FIELD})
    {
        auto apoFields = poRecord->GetFields(pszFieldName);
        for (auto [fieldIdxSizeT, poField] : cpl::enumerate(apoFields))
        {
            const int fieldIdx = static_cast<int>(fieldIdxSizeT);
            const int nRepeatCount =
                poField->GetParts().size() == 2
                    ? poField->GetParts()[1]->GetRepeatCount()
                    : poField->GetRepeatCount();
            for (int iSubField = 0; iSubField < nRepeatCount; ++iSubField)
            {
                if (EQUAL(pszFieldName, INAS_FIELD))
                {
                    const InfoAssocCode nNIAC = poRecord->GetIntSubfield(
                        poField, NIAC_SUBFIELD, iSubField);
                    const auto oIter =
                        m_informationAssociationCodesRemapping.find(nNIAC);
                    if (oIter == m_informationAssociationCodesRemapping.end())
                    {
                        if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "%s, RCNM=%d, RCID=%d, %s[iField=%d, "
                                "iSubField=%d]: unknown NIAC=%d",
                                m_osFilename.c_str(), static_cast<int>(nRCNM),
                                nRCID, pszFieldName, fieldIdx, iSubField,
                                static_cast<int>(nNIAC))))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        poRecord->SetIntSubfield(
                            pszFieldName, fieldIdx, NIAC_SUBFIELD, iSubField,
                            static_cast<int>(oIter->second));
                    }
                }
                else
                {
                    const FeatureAssocCode nNFAC = poRecord->GetIntSubfield(
                        poField, NFAC_SUBFIELD, iSubField);
                    const auto oIter =
                        m_featureAssociationCodesRemapping.find(nNFAC);
                    if (oIter == m_featureAssociationCodesRemapping.end())
                    {
                        if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "%s, RCNM=%d, RCID=%d, %s[iField=%d, "
                                "iSubField=%d]: unknown NFAC=%d",
                                m_osFilename.c_str(), static_cast<int>(nRCNM),
                                nRCID, pszFieldName, fieldIdx, iSubField,
                                static_cast<int>(nNFAC))))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        poRecord->SetIntSubfield(
                            pszFieldName, fieldIdx, NFAC_SUBFIELD, iSubField,
                            static_cast<int>(oIter->second));
                    }
                }

                {
                    const AssocRoleCode nNARC = poRecord->GetIntSubfield(
                        poField, NARC_SUBFIELD, iSubField);
                    const auto oIter =
                        m_associationRoleCodesRemapping.find(nNARC);
                    if (oIter == m_associationRoleCodesRemapping.end())
                    {
                        if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "%s, RCNM=%d, RCID=%d, %s[iField=%d, "
                                "iSubField=%d]: unknown NARC=%d",
                                m_osFilename.c_str(), static_cast<int>(nRCNM),
                                nRCID, pszFieldName, fieldIdx, iSubField,
                                static_cast<int>(nNARC))))
                        {
                            return false;
                        }
                    }
                    else
                    {
                        poRecord->SetIntSubfield(
                            pszFieldName, fieldIdx, NARC_SUBFIELD, iSubField,
                            static_cast<int>(oIter->second));
                    }
                }
            }
        }
    }

    return true;
}

/************************************************************************/
/*                        ProcessUpdateRecord()                         */
/************************************************************************/

bool OGRS101Reader::ProcessUpdateRecord(const DDFRecord *poUpdateRecord,
                                        DDFRecord *poTargetRecord) const
{
    const auto poIDField = poUpdateRecord->GetField(0);
    CPLAssert(poIDField);
    const char *pszIDFieldName = poIDField->GetFieldDefn()->GetName();
    CPLAssert(pszIDFieldName);

    // Record version
    const int nRVER =
        poUpdateRecord->GetIntSubfield(pszIDFieldName, 0, RVER_SUBFIELD, 0);
    poTargetRecord->SetIntSubfield(pszIDFieldName, 0, RVER_SUBFIELD, 0, nRVER);

    if (!ProcessUpdateINASOrFASC(poUpdateRecord, poTargetRecord, INAS_FIELD))
        return false;

    bool bRet = true;
    if (EQUAL(pszIDFieldName, IRID_FIELD))
    {
        bRet = ProcessUpdateATTR(poUpdateRecord, poTargetRecord);
    }
    else if (EQUAL(pszIDFieldName, PRID_FIELD))
    {
        bRet = ProcessUpdateRecordPoint(poUpdateRecord, poTargetRecord);
    }
    else if (EQUAL(pszIDFieldName, MRID_FIELD))
    {
        bRet = ProcessUpdateRecordMultiPoint(poUpdateRecord, poTargetRecord);
    }
    else if (EQUAL(pszIDFieldName, CRID_FIELD))
    {
        bRet = ProcessUpdateRecordCurve(poUpdateRecord, poTargetRecord);
    }
    else if (EQUAL(pszIDFieldName, CCID_FIELD))
    {
        bRet =
            ProcessUpdateRecordCompositeCurve(poUpdateRecord, poTargetRecord);
    }
    else if (EQUAL(pszIDFieldName, SRID_FIELD))
    {
        bRet = ProcessUpdateRecordSurface(poUpdateRecord, poTargetRecord);
    }
    else if (EQUAL(pszIDFieldName, FRID_FIELD))
    {
        bRet = ProcessUpdateATTR(poUpdateRecord, poTargetRecord) &&
               ProcessUpdateINASOrFASC(poUpdateRecord, poTargetRecord,
                                       FASC_FIELD) &&
               ProcessUpdateRecordFeatureType(poUpdateRecord, poTargetRecord);
    }

    return bRet;
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
