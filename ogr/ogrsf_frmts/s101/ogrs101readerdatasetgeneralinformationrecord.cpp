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
#include <array>
#include <cmath>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

/************************************************************************/
/*                ReadDatasetGeneralInformationRecord()                 */
/************************************************************************/

/** Read the general information record */
bool OGRS101Reader::ReadDatasetGeneralInformationRecord(
    const DDFRecord *poRecord)
{
    if (!poRecord)
        return EMIT_ERROR("no Dataset General Information record.");

    return ReadDSID(poRecord) && ReadDSSI(poRecord) && ReadATCS(poRecord) &&
           ReadITCS(poRecord) && ReadFTCS(poRecord) && ReadIACS(poRecord) &&
           ReadFACS(poRecord) && ReadARCS(poRecord);
}

/************************************************************************/
/*                              ReadDSID()                              */
/************************************************************************/

/** Read the Dataset Identification (DSID) field of the general information
 * record.
 */
bool OGRS101Reader::ReadDSID(const DDFRecord *poRecord)
{
    const auto poField = poRecord->FindField(DSID_FIELD);
    if (!poField)
        return EMIT_ERROR("DSID field not found.");

    // Record name
    const RecordName nRCNM =
        poRecord->GetIntSubfield(poField, RCNM_SUBFIELD, 0);
    if (nRCNM != RECORD_NAME_DATASET_IDENTIFICATION &&
        !EMIT_ERROR_OR_WARNING("Invalid value for RCNM subfield of DSID."))
    {
        return false;
    }

    // Record identifier
    const int nRCID = poRecord->GetIntSubfield(poField, RCID_SUBFIELD, 0);
    // Only one record expected
    if (nRCID != 1 &&
        !EMIT_ERROR_OR_WARNING("Invalid value for RCID subfield of DSID."))
    {
        return false;
    }

    static const struct
    {
        const char *pszS101Name;
        const char *pszGDALName;
    } mapMetadataKeys[] = {
        {"ENSP", "ENCODING_SPECIFICATION"},
        {"ENED", "ENCODING_SPECIFICATION_EDITION"},
        {"PRSP", "PRODUCT_IDENTIFIER"},
        {"PRED", "PRODUCT_EDITION"},
        {"PROF", "APPLICATION_PROFILE"},
        {"DSNM", "DATASET_IDENTIFIER"},
        {"DSTL", "DATASET_TITLE"},
        {"DSRD", "DATASET_REFERENCE_DATE"},
        {"DSLG", "DATASET_LANGUAGE"},
        {"DSAB", "DATASET_ABSTRACT"},
        {"DSED", "DATASET_EDITION"},
    };

    for (const auto &item : mapMetadataKeys)
    {
        const char *pszValue =
            poRecord->GetStringSubfield(poField, item.pszS101Name, 0);
        if (!pszValue)
        {
            if (!EMIT_ERROR_OR_WARNING(std::string("no subfield ")
                                           .append(item.pszS101Name)
                                           .append(" in DSID.")
                                           .c_str()))
            {
                return false;
            }
        }
        else
        {
            if (m_bInUpdate && EQUAL(item.pszS101Name, "DSED"))
            {
                const char *pszOldVal =
                    m_aosMetadata.FetchNameValueDef(item.pszGDALName, "");
                if (EQUAL(pszValue, pszOldVal) &&
                    !EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("%s has the same DATASET_EDITION value '%s' "
                                   "than the previous update/initial file.",
                                   m_osFilename.c_str(), pszOldVal)))
                {
                    return false;
                }
            }
            else if (m_bInUpdate && !EQUAL(item.pszS101Name, "PROF") &&
                     !EQUAL(item.pszS101Name, "DSNM") &&
                     !EQUAL(item.pszS101Name, "DSRD"))
            {
                const char *pszOldVal =
                    m_aosMetadata.FetchNameValueDef(item.pszGDALName, "");
                if (!EQUAL(pszValue, pszOldVal) &&
                    !EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("%s has a different value %s='%s' than the "
                                   "previous update/initial file ('%s').",
                                   m_osFilename.c_str(), item.pszS101Name,
                                   pszValue, pszOldVal)))
                {
                    return false;
                }
            }
            if (pszValue[0] ||
                (m_bInUpdate &&
                 m_aosMetadata.FetchNameValueDef(item.pszGDALName, "")[0] != 0))
                m_aosMetadata.SetNameValue(item.pszGDALName, pszValue);
        }
    }

    const char *pszPRSP =
        m_aosMetadata.FetchNameValueDef("PRODUCT_IDENTIFIER", "");
    if (!strstr(pszPRSP, "S-101") &&
        !EMIT_ERROR_OR_WARNING(
            CPLSPrintf("%s is an ISO8211 file, but not a S-101 product. "
                       "Product identifier is '%s'.",
                       m_osFilename.c_str(), pszPRSP)))
    {
        return false;
    }

    // Accept 1.x, but only >= 2.0 is operational
    if (!STARTS_WITH(pszPRSP, "INT.IHO.S-101.1.") &&
        !STARTS_WITH(pszPRSP, "INT.IHO.S-101.2.") &&
        !EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "Product identifier is '%s', but only 'INT.IHO.S-101.2.0' is "
            "nominally handled. Going on but the dataset might not be "
            "correctly read.",
            pszPRSP)))
    {
        return false;
    }

    const char *pszPROF =
        m_aosMetadata.FetchNameValueDef("APPLICATION_PROFILE", "");
    if (EQUAL(pszPROF, "1"))
    {
        if (m_bInUpdate &&
            !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("Update file %s has APPLICATION_PROFILE=1 (Initial)",
                           m_osFilename.c_str())))
        {
            return false;
        }
    }
    else if (EQUAL(pszPROF, "2"))
    {
        if (!m_bInUpdate &&
            !EMIT_ERROR_OR_WARNING(
                "Direct opening of files with APPLICATION_PROFILE=2 (Update) "
                "is not supported. Open the main .000 file"))
        {
            return false;
        }
    }
    else
    {
        if (!EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: APPLICATION_PROFILE='%s' is invalid",
                           m_osFilename.c_str(), pszPROF)))
            return false;
    }

    const char *pszDSED =
        m_aosMetadata.FetchNameValueDef("DATASET_EDITION", "");
    if (EQUAL(pszDSED, "0"))
    {
        m_aosMetadata.SetNameValue("STATUS", "CANCELLED");
        m_bCancelled = true;
    }

    if (!CheckFieldDefinitions(poRecord->GetModule()))
        return false;

    return true;
}

/************************************************************************/
/*                       CheckFieldDefinitions()                        */
/************************************************************************/

/** Check that field and subfield definitions conforms to the specification.
 */
bool OGRS101Reader::CheckFieldDefinitions(const DDFModule *poCurModule) const
{
    bool bRet = CheckField0000Definition(poCurModule);

    // Note the '\\\\' has 4 bytes instead of the '\\' that appears in the
    // spec, because of the escaping of backslash in C++
    const std::map<std::string_view, std::pair<const char *, const char *>>
        fieldArrayDescrAndFieldControls = {
            {DSID_FIELD,
             {"RCNM!RCID!ENSP!ENED!PRSP!PRED!PROF!DSNM!DSTL!DSRD!DSLG!DSAB!"
              "DSED\\\\*DSTC",
              "(b11,b14,7A,A(8),3A,(b11))"}},
            {DSSI_FIELD,
             {"DCOX!DCOY!DCOZ!CMFX!CMFY!CMFZ!NOIR!NOPN!NOMN!NOCN!NOXN!NOSN!"
              "NOFR",
              "(3b48,10b14)"}},
            {ATCS_FIELD, {"*ATCD!ANCD", "(A,b12)"}},
            {ITCS_FIELD, {"*ITCD!ITNC", "(A,b12)"}},
            {FTCS_FIELD, {"*FTCD!FTNC", "(A,b12)"}},
            {IACS_FIELD, {"*IACD!IANC", "(A,b12)"}},
            {FACS_FIELD, {"*FACD!FANC", "(A,b12)"}},
            {ARCS_FIELD, {"*ARCD!ARNC", "(A,b12)"}},
            {CSID_FIELD, {"RCNM!RCID!NCRC", "(b11,b14,b11)"}},
            {CRSH_FIELD,
             {"CRIX!CRST!CSTY!CRNM!CRSI!CRSS!SCRI", "(3b11,2A,b11,A)"}},
            {CSAX_FIELD, {"*AXTY!AXUM", "(2b11)"}},
            {VDAT_FIELD, {"DTNM!DTID!DTSR!SCRI", "(2A,b11,A)"}},
            {IRID_FIELD, {"RCNM!RCID!NITC!RVER!RUIN", "(b11,b14,2b12,b11)"}},
            {INAS_FIELD,
             {"RRNM!RRID!NIAC!NARC!IUIN\\\\*NATC!ATIX!PAIX!ATIN!ATVL",
              "(b11,b14,2b12,b11,(3b12,b11,A))"}},
            {ATTR_FIELD, {"*NATC!ATIX!PAIX!ATIN!ATVL", "(3b12,b11,A)"}},
            {C2IT_FIELD, {"YCOO!XCOO", "(2b24)"}},
            {C3IT_FIELD, {"VCID!YCOO!XCOO!ZCOO", "(b11,3b24)"}},
            {C2IL_FIELD, {"*YCOO!XCOO", "(2b24)"}},
            {C3IL_FIELD, {"VCID\\\\*YCOO!XCOO!ZCOO", "(b11,(3b24))"}},
            {PRID_FIELD, {"RCNM!RCID!RVER!RUIN", "(b11,b14,b12,b11)"}},
            {COCC_FIELD, {"COUI!COIX!NCOR", "(b11,2b12)"}},
            {MRID_FIELD, {"RCNM!RCID!RVER!RUIN", "(b11,b14,b12,b11)"}},
            {CRID_FIELD, {"RCNM!RCID!RVER!RUIN", "(b11,b14,b12,b11)"}},
            {PTAS_FIELD, {"*RRNM!RRID!TOPI", "(b11,b14,b11)"}},
            {SECC_FIELD, {"SEUI!SEIX!NSEG", "(b11,2b12)"}},
            {SEGH_FIELD, {"INTP", "(b11)"}},
            {CCID_FIELD, {"RCNM!RCID!RVER!RUIN", "(b11,b14,b12,b11)"}},
            {CCOC_FIELD, {"CCUI!CCIX!NCCO", "(b11,2b12)"}},
            {CUCO_FIELD, {"*RRNM!RRID!ORNT", "(b11,b14,b11)"}},
            {SRID_FIELD, {"RCNM!RCID!RVER!RUIN", "(b11,b14,b12,b11)"}},
            {RIAS_FIELD, {"*RRNM!RRID!ORNT!USAG!RAUI", "(b11,b14,3b11)"}},
            {FRID_FIELD, {"RCNM!RCID!NFTC!RVER!RUIN", "(b11,b14,2b12,b11)"}},
            {FOID_FIELD, {"AGEN!FIDN!FIDS", "(b12,b14,b12)"}},
            {SPAS_FIELD,
             {"*RRNM!RRID!ORNT!SMIN!SMAX!SAUI", "(b11,b14,b11,2b14,b11)"}},
            {FASC_FIELD,
             {"RRNM!RRID!NFAC!NARC!FAUI\\\\*NATC!ATIX!PAIX!ATIN!ATVL",
              "(b11,b14,2b12,b11,(3b12,b11,A))"}},
            {MASK_FIELD, {"*RRNM!RRID!MIND!MUIN", "(b11,b14,2b11)"}},
        };

    for (const auto &poFieldDefn : poCurModule->GetFieldDefns())
    {
        const char *pszFieldName = poFieldDefn->GetName();
        if (strcmp(pszFieldName, _0000_FIELD) != 0)
        {
            const auto oIter =
                fieldArrayDescrAndFieldControls.find(pszFieldName);
            if (oIter == fieldArrayDescrAndFieldControls.end())
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Unknown field definition '%s'.", pszFieldName)))
                {
                    bRet = false;
                }
            }
            else
            {
                const char *pszExpectedArrayDescr = oIter->second.first;
                if (strcmp(poFieldDefn->GetArrayDescr(),
                           pszExpectedArrayDescr) != 0 &&
                    !EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("For array description of field definition "
                                   "'%s', got '%s' whereas '%s' is expected.",
                                   pszFieldName, poFieldDefn->GetArrayDescr(),
                                   pszExpectedArrayDescr)))
                {
                    bRet = false;
                }

                const char *pszExpectedFormatControls = oIter->second.second;
                if (strcmp(poFieldDefn->GetFormatControls(),
                           pszExpectedFormatControls) != 0 &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "For format controls of field definition '%s', got "
                        "'%s' whereas '%s' is expected.",
                        pszFieldName, poFieldDefn->GetFormatControls(),
                        pszExpectedFormatControls)))
                {
                    bRet = false;
                }

                const DDF_data_struct_code eExpectedDataStruct =
                    strstr(pszExpectedArrayDescr, "\\\\") != nullptr
                        ? dsc_concatenated
                    : pszExpectedArrayDescr[0] == '*' ? dsc_array
                                                      : dsc_vector;
                if (poFieldDefn->GetDataStructCode() != eExpectedDataStruct &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Data struct code of field definition '%s', got "
                        "'%d' whereas '%d' is expected.",
                        pszFieldName, poFieldDefn->GetDataStructCode(),
                        eExpectedDataStruct)))
                {
                    bRet = false;
                }

                const bool bHasIntegerSubfields =
                    strstr(pszExpectedFormatControls, "b1") != nullptr ||
                    strstr(pszExpectedFormatControls, "b2") != nullptr;
                const bool bHasFloatSubfields =
                    strstr(pszExpectedFormatControls, "b4") != nullptr;
                const bool bHasStringSubfields =
                    strchr(pszExpectedFormatControls, 'A') != nullptr;
                const DDF_data_type_code eExpectedDataTypeCode =
                    (bHasIntegerSubfields && !bHasFloatSubfields &&
                     !bHasStringSubfields)
                        ? dtc_implicit_point
                        :
                        // Commenting below code, since it doesn't actually
                        // occur with S-101 fields
                        // (!bHasIntegerSubfields && bHasFloatSubfields && !bHasStringSubfields) ?
                        //    dtc_explicit_point :
                        // cppcheck-suppress knownConditionTrueFalse
                        (!bHasIntegerSubfields && !bHasFloatSubfields &&
                         bHasStringSubfields)
                        ? dtc_char_string
                        : dtc_mixed_data_type;
                if (poFieldDefn->GetDataTypeCode() != eExpectedDataTypeCode &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Data type code of field definition '%s', got "
                        "'%d' whereas '%d' is expected.",
                        pszFieldName, poFieldDefn->GetDataTypeCode(),
                        eExpectedDataTypeCode)))
                {
                    bRet = false;
                }
            }
        }
    }

    return bRet;
}

/************************************************************************/
/*                      CheckField0000Definition()                      */
/************************************************************************/

/** Check that the special "0000" field conforms to the specification.
 */
bool OGRS101Reader::CheckField0000Definition(const DDFModule *poCurModule) const
{
    bool bRet = true;

    const auto po0000FieldDefn = poCurModule->FindFieldDefn(_0000_FIELD);
    if (!po0000FieldDefn)
    {
        return EMIT_ERROR_OR_WARNING(
            "Field definition of 0000 control field not found.");
    }

    if (po0000FieldDefn->GetDataStructCode() != dsc_elementary &&
        !EMIT_ERROR_OR_WARNING("Data struct code of field definition of 0000 "
                               "control field must be elementary."))
    {
        bRet = false;
    }

    if (po0000FieldDefn->GetDataTypeCode() != dtc_char_string &&
        !EMIT_ERROR_OR_WARNING("Data type code of field definition of 0000 "
                               "control field must be char_string."))
    {
        bRet = false;
    }

    if (po0000FieldDefn->GetFormatControls()[0] != 0 &&
        !EMIT_ERROR_OR_WARNING("Format controls of field definition of 0000 "
                               "control field must be empty."))
    {
        bRet = false;
    }

    // Should contain concatenated pairs of parent_field_name,child_field_name
    // without any separator: e.g. DSIDDSSICSIDCRSHCRSHCSAXCRSHVDAT
    const char *psz0000Descr = po0000FieldDefn->GetArrayDescr();
    const size_t n0000DescrLen = strlen(psz0000Descr);
    constexpr int FIELD_NAME_SIZE = 4;
    constexpr int PARENT_CHILD_PAIR_SIZE = 2 * FIELD_NAME_SIZE;
    if ((n0000DescrLen % PARENT_CHILD_PAIR_SIZE) != 0 &&
        !EMIT_ERROR_OR_WARNING(
            "Length of field tag pairs of field definition of 0000 "
            "control field must be a multiple of 8."))
    {
        bRet = false;
    }

    constexpr int MAX_CHILDREN_COUNT = 8;
    using ArrayOfChildren = std::array<const char *, MAX_CHILDREN_COUNT>;

    static const struct
    {
        const std::string_view svParent;
        ArrayOfChildren apszChildren;
    } knownPairs[] = {
        {DSID_FIELD,
         {DSSI_FIELD, ATCS_FIELD, ITCS_FIELD, FTCS_FIELD, IACS_FIELD,
          FACS_FIELD, ARCS_FIELD}},
        {CSID_FIELD, {CRSH_FIELD}},
        {CRSH_FIELD, {CSAX_FIELD, VDAT_FIELD}},
        {IRID_FIELD, {ATTR_FIELD, INAS_FIELD}},
        {PRID_FIELD, {INAS_FIELD, C2IT_FIELD, C3IT_FIELD}},
        {MRID_FIELD, {INAS_FIELD, COCC_FIELD, C2IL_FIELD, C3IL_FIELD}},
        {CRID_FIELD, {INAS_FIELD, PTAS_FIELD, SECC_FIELD, SEGH_FIELD}},
        {SEGH_FIELD, {COCC_FIELD, C2IL_FIELD}},
        {CCID_FIELD, {INAS_FIELD, CCOC_FIELD, CUCO_FIELD}},
        {SRID_FIELD, {INAS_FIELD, RIAS_FIELD}},
        {FRID_FIELD,
         {FOID_FIELD, ATTR_FIELD, INAS_FIELD, SPAS_FIELD, FASC_FIELD,
          MASK_FIELD}},
    };

    const std::set<std::string> oSetUsedFieldNames = [poCurModule]
    {
        std::set<std::string> s;
        for (const auto &poFieldDefn : poCurModule->GetFieldDefns())
            s.insert(poFieldDefn->GetName());
        return s;
    }();

    // Return an iterator of knownPairs that points to the entry where
    // iter->svParent == svParent, but only if one of its allowed children
    // is in the set of fields actually found in the file.
    const auto GetParentIter =
        [&oSetUsedFieldNames](const std::string_view &svParent)
    {
        const auto iter =
            std::find_if(std::begin(knownPairs), std::end(knownPairs),
                         [&svParent](const auto &item)
                         { return svParent == item.svParent; });
        if (iter != std::end(knownPairs))
        {
            const auto allowedChildren = iter->apszChildren;
            if (std::find_if(allowedChildren.begin(), allowedChildren.end(),
                             [&oSetUsedFieldNames](const char *pszStr)
                             {
                                 return pszStr &&
                                        cpl::contains(oSetUsedFieldNames,
                                                      pszStr);
                             }) != allowedChildren.end())
            {
                return iter;
            }
        }
        return std::end(knownPairs);
    };

    // Returns an iterator of allowedChildren only is svChild is one of the
    // knownPairs::apszChildren.
    const auto IsKnownChild = [](const ArrayOfChildren &allowedChildren,
                                 const std::string_view &svChild)
    {
        return std::find_if(allowedChildren.begin(), allowedChildren.end(),
                            [&svChild](const char *pszStr)
                            { return pszStr && svChild == pszStr; }) !=
               allowedChildren.end();
    };

    const size_t nPairs = n0000DescrLen / PARENT_CHILD_PAIR_SIZE;
    std::set<std::string> oSetReferencedParent;
    std::set<std::string> oSetReferencedChildren;
    std::set<std::pair<std::string, std::string>> oSetFoundPairs;
    for (size_t i = 0; i < nPairs; ++i)
    {
        bool bUnknownParentOrChild = false;
        const std::string osParent(psz0000Descr + i * PARENT_CHILD_PAIR_SIZE,
                                   FIELD_NAME_SIZE);
        oSetReferencedParent.insert(osParent);

        const std::string osChild(psz0000Descr + i * PARENT_CHILD_PAIR_SIZE +
                                      FIELD_NAME_SIZE,
                                  FIELD_NAME_SIZE);
        oSetReferencedChildren.insert(osChild);

        if (!cpl::contains(oSetUsedFieldNames, osParent) &&
            !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("Field '%s' referenced in field definition of 0000 "
                           "control field does not exist.",
                           osParent.c_str())))
        {
            bUnknownParentOrChild = true;
            bRet = false;
        }

        if (!cpl::contains(oSetUsedFieldNames, osChild) &&
            !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("Field '%s' referenced in field definition of 0000 "
                           "control field does not exist.",
                           osChild.c_str())))
        {
            bUnknownParentOrChild = true;
            bRet = false;
        }

        if (!oSetFoundPairs.insert({osParent, osChild}).second &&
            !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("Pair ('%s','%s') referenced multiple time in field "
                           "definition of 0000 control field.",
                           osParent.c_str(), osChild.c_str())))
        {
            bRet = false;
        }

        if (!bUnknownParentOrChild)
        {
            const auto oIter = GetParentIter(osParent);
            if (oIter == std::end(knownPairs))
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Field '%s' referenced in field definition of 0000 "
                        "control field is not a parent of a registered "
                        "(parent,child) pair.",
                        osParent.c_str())))
                {
                    bRet = false;
                }
            }
            else
            {
                if (!IsKnownChild(oIter->apszChildren, osChild) &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "Field '%s' referenced in field definition of 0000 "
                        "control field is not an allowed child of a registered "
                        "('%s',child) pair.",
                        osChild.c_str(), osParent.c_str())))
                {
                    bRet = false;
                }
            }
        }
    }

    if (nPairs > 0)
    {
        for (const std::string &osFieldName : oSetUsedFieldNames)
        {
            if (GetParentIter(osFieldName) != std::end(knownPairs) &&
                !cpl::contains(oSetReferencedParent, osFieldName) &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Field '%s' is not referenced as a parent in field "
                    "definition of 0000 control field.",
                    osFieldName.c_str())))
            {
                bRet = false;
            }

            if (std::find_if(std::begin(knownPairs), std::end(knownPairs),
                             [&IsKnownChild, &osFieldName](const auto &item)
                             {
                                 return IsKnownChild(item.apszChildren,
                                                     osFieldName);
                             }) != std::end(knownPairs) &&
                !cpl::contains(oSetReferencedChildren, osFieldName) &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "Field '%s' is not referenced as a child in field "
                    "definition of 0000 control field.",
                    osFieldName.c_str())))
            {
                bRet = false;
            }
        }
    }

    return bRet;
}

/************************************************************************/
/*                              ReadDSSI()                              */
/************************************************************************/

/** Read the Dataset Structure Information (DSSI) field.
 */
bool OGRS101Reader::ReadDSSI(const DDFRecord *poRecord)
{
    const auto poField = poRecord->FindField(DSSI_FIELD);
    if (!poField)
        return EMIT_ERROR("DSSI field not found");

    int bSuccess = false;

    // must NOT be set as static, as it depens on "this" !
    const struct
    {
        const char *pszKey;
        double *pdfVal;
    } doubleFields[] = {
        {"DCOX", &m_dfXShift},
        {"DCOY", &m_dfYShift},
        {"DCOZ", &m_dfZShift},
    };

    for (const auto &field : doubleFields)
    {
        *(field.pdfVal) = poRecord->GetFloatSubfield(
            DSSI_FIELD, 0, field.pszKey, 0, &bSuccess);
        if (!bSuccess && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                             "no %s subfield of DSSI.", field.pszKey)))
        {
            return false;
        }
        if (std::isnan(*(field.pdfVal)))
        {
            return EMIT_ERROR(
                CPLSPrintf("NaN value in %s subfield of DSSI.", field.pszKey));
        }
    }

    if (m_dfXShift != S101_SHIFT &&
        !EMIT_ERROR_OR_WARNING(
            "Value of DCOX subfield of DSSI is not at official value."))
    {
        return false;
    }

    if (m_dfYShift != S101_SHIFT &&
        !EMIT_ERROR_OR_WARNING(
            "Value of DCOY subfield of DSSI is not at official value."))
    {
        return false;
    }

    if (m_dfZShift != S101_SHIFT &&
        !EMIT_ERROR_OR_WARNING(
            "Value of DCOZ subfield of DSSI is not at official value."))
    {
        return false;
    }

    // must NOT be set as static, as it depens on "this" !
    struct KeyIntVal
    {
        const char *pszKey;
        int *pnVal;
    };

    const KeyIntVal intFieldsInitial[] = {
        {"CMFX", &m_nXScale},
        {"CMFY", &m_nYScale},
        {"CMFZ", &m_nZScale},
        {"NOIR", &m_nCountInformationRecord},
        {"NOPN", &m_nCountPointRecord},
        {"NOMN", &m_nCountMultiPointRecord},
        {"NOCN", &m_nCountCurveRecord},
        {"NOXN", &m_nCountCompositeCurveRecord},
        {"NOSN", &m_nCountSurfaceRecord},
        {"NOFR", &m_nCountFeatureTypeRecord},
    };
    const KeyIntVal intFieldsUpdate[] = {
        {"CMFX", &m_nXScale},
        {"CMFY", &m_nYScale},
        {"CMFZ", &m_nZScale},
    };
    const auto &begin = m_bInUpdate ? std::begin(intFieldsUpdate)
                                    : std::begin(intFieldsInitial);
    const auto &end =
        m_bInUpdate ? std::end(intFieldsUpdate) : std::end(intFieldsInitial);
    for (auto oIter = begin; oIter != end; ++oIter)
    {
        const auto &field = *oIter;
        *(field.pnVal) =
            poRecord->GetIntSubfield(poField, field.pszKey, 0, &bSuccess);
        if (!bSuccess && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                             "no %s subfield of DSSI", field.pszKey)))
        {
            return false;
        }
        if (*(field.pnVal) < 0 &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                "Invalid value for %s subfield of DSSI.", field.pszKey)))
        {
            return false;
        }
    }

    if (m_nXScale <= 0 || m_nYScale <= 0 || m_nZScale <= 0)
    {
        return EMIT_ERROR(
            "Invalid CMFX/CMFY/CMFZ scale factor in DSSI (must be > 0).");
    }

    if (m_nXScale != S101_XSCALE &&
        !EMIT_ERROR_OR_WARNING(
            "Value of CMFX subfield of DSSI is not at official value."))
    {
        return false;
    }

    if (m_nYScale != S101_YSCALE &&
        !EMIT_ERROR_OR_WARNING(
            "Value of CMFY subfield of DSSI is not at official value."))
    {
        return false;
    }

    if (m_nZScale != S101_ZSCALE &&
        !EMIT_ERROR_OR_WARNING(
            CPLSPrintf("Value of CMFZ subfield of DSSI is not at official "
                       "value. Got %d, expected %d.",
                       m_nZScale, S101_ZSCALE)))
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                     ReadGenericCodeAssociation()                     */
/************************************************************************/

/** Read fields like ATCS, ITCS, etc. that associate a numeric code to a
 * string
 */
template <class CodeType>
bool OGRS101Reader::ReadGenericCodeAssociation(
    const DDFRecord *poRecord, const char *pszFieldName,
    const char *pszSubField0Name, const char *pszSubField1Name,
    std::map<CodeType, std::string> &map,
    std::map<CodeType, CodeType> &mapRemapping) const
{
    const auto poField = poRecord->FindField(pszFieldName);
    if (!poField)
    {
        CPLDebugOnly("S101", "No %s field found", pszFieldName);
        return true;
    }

    // The remapping is only valid when processing the current update file
    mapRemapping.clear();

    // Map from string value to code (used when processing update files)
    std::map<std::string, int> reversedMap;
    for (const auto &[key, value] : map)
        reversedMap[value] = static_cast<int>(key);

    std::set<CodeType> setCodes;
    const int nRepeatCount = poField->GetRepeatCount();
    for (int i = 0; i < nRepeatCount; ++i)
    {
        int bSuccess = false;
        const char *pszVal = poRecord->GetStringSubfield(
            poField, pszSubField0Name, i, &bSuccess);
        if (!bSuccess)
        {
            if (!m_bStrict)
                continue;
            return false;
        }
        const CodeType nCode =
            poRecord->GetIntSubfield(poField, pszSubField1Name, i, &bSuccess);
        if (!bSuccess)
        {
            if (!m_bStrict)
                continue;
            return false;
        }
        if (!pszVal)
            pszVal = "(invalid)";
        if (!setCodes.insert(nCode).second)
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s: several definitions for %s %d.", pszFieldName,
                    pszSubField1Name, static_cast<int>(nCode))))
            {
                return false;
            }
        }
        else if (m_bInUpdate)
        {
            // Update files may re-use codes that were used in the initial file
            // but for a different string value ! So we must handle potential
            // clashes.
            const auto oReversedIter = reversedMap.find(pszVal);
            if (oReversedIter != reversedMap.end())
            {
                const auto nOldCode = oReversedIter->second;
                mapRemapping.insert({nCode, nOldCode});
            }
            else if (!map.insert({nCode, pszVal}).second)
            {
                const CodeType largestCode = map.rbegin()->first;
                if (largestCode == INT_MAX)
                {
                    // Very unlikely to happen
                    return EMIT_ERROR("Lack of available codes");
                }
                const CodeType newCode = static_cast<CodeType>(largestCode + 1);
                map.insert({newCode, pszVal});
                mapRemapping.insert({nCode, newCode});
            }
            else
            {
                mapRemapping.insert({nCode, nCode});
            }
        }
        else
        {
            const bool bInserted = map.insert({nCode, pszVal}).second;
            CPL_IGNORE_RET_VAL(bInserted);
            CPLAssert(bInserted);
        }
    }

    return true;
}

/************************************************************************/
/*                              ReadATCS()                              */
/************************************************************************/

/** Read optional Attribute Codes field
 */
bool OGRS101Reader::ReadATCS(const DDFRecord *poRecord)
{
    return ReadGenericCodeAssociation<AttrCode>(poRecord, ATCS_FIELD, "ATCD",
                                                "ANCD", m_attributeCodes,
                                                m_attributeCodesRemapping);
}

/************************************************************************/
/*                              ReadITCS()                              */
/************************************************************************/

/** Read optional Feature Type Codes field
 */
bool OGRS101Reader::ReadITCS(const DDFRecord *poRecord)
{
    return ReadGenericCodeAssociation<InfoTypeCode>(
        poRecord, ITCS_FIELD, "ITCD", "ITNC", m_informationTypeCodes,
        m_informationTypeCodesRemapping);
}

/************************************************************************/
/*                              ReadFTCS()                              */
/************************************************************************/

/** Read optional Feature Type Codes field
 */
bool OGRS101Reader::ReadFTCS(const DDFRecord *poRecord)
{
    return ReadGenericCodeAssociation<FeatureTypeCode>(
        poRecord, FTCS_FIELD, "FTCD", "FTNC", m_featureTypeCodes,
        m_featureTypeCodesRemapping);
}

/************************************************************************/
/*                              ReadIACS()                              */
/************************************************************************/

/** Read optional Information Association Codes field
 */
bool OGRS101Reader::ReadIACS(const DDFRecord *poRecord)
{
    return ReadGenericCodeAssociation<InfoAssocCode>(
        poRecord, IACS_FIELD, "IACD", "IANC", m_informationAssociationCodes,
        m_informationAssociationCodesRemapping);
}

/************************************************************************/
/*                              ReadFACS()                              */
/************************************************************************/

/** Read optional Feature Association Codes field
 */
bool OGRS101Reader::ReadFACS(const DDFRecord *poRecord)
{
    return ReadGenericCodeAssociation<FeatureAssocCode>(
        poRecord, FACS_FIELD, "FACD", "FANC", m_featureAssociationCodes,
        m_featureAssociationCodesRemapping);
}

/************************************************************************/
/*                              ReadARCS()                              */
/************************************************************************/

/** Read optional Association Role Codes field
 */
bool OGRS101Reader::ReadARCS(const DDFRecord *poRecord)
{
    return ReadGenericCodeAssociation<AssocRoleCode>(
        poRecord, ARCS_FIELD, "ARCD", "ARNC", m_associationRoleCodes,
        m_associationRoleCodesRemapping);
}
