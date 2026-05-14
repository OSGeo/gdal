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
#include <charconv>
#include <cfloat>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

#include "include_fast_float.h"

/************************************************************************/
/*                          IngestAttributes()                          */
/************************************************************************/

/** For a given record that has a ATTR/INAS/FACS field, ingest all attributes
 * from a particular instance of that field
 */
bool OGRS101Reader::IngestAttributes(
    const DDFRecord *poRecord, int iRecord, const char *pszIDFieldName,
    const char *pszAttrFieldName, const DDFField *poATTRField, int iField,
    bool bMultipleFields, std::vector<S101AttrDef> &asS101AttrDefs) const
{
    std::set<std::tuple<AttrCode, AttrRepeat, AttrIndex>> oSetNATC_ATIX_PAIX;

    const auto GetErrorContext = [iRecord, pszIDFieldName, pszAttrFieldName,
                                  iField, bMultipleFields](AttrIndex iATTR)
    {
        if (bMultipleFields)
        {
            return CPLSPrintf(
                "Record index=%d of %s, %s[%d] field, attribute idx=%d",
                iRecord, pszIDFieldName, pszAttrFieldName, iField,
                static_cast<int>(iATTR));
        }
        else
        {
            return CPLSPrintf(
                "Record index=%d of %s, %s field, attribute idx=%d", iRecord,
                pszIDFieldName, pszAttrFieldName, static_cast<int>(iATTR));
        }
    };

    const AttrCode nLargestNATC(
        !m_attributeCodes.empty() ? m_attributeCodes.rbegin()->first : 0);
    const bool bAttributeCodesSequential =
        !m_attributeCodes.empty() && m_attributeCodes.begin()->first == 1 &&
        static_cast<size_t>(static_cast<int>(nLargestNATC)) ==
            m_attributeCodes.size();

    const int nRepeatCount = poATTRField->GetRepeatCount();
    AttrCode nLastNATC = -1;
    AttrRepeat nLastATIX = -1;
    AttrIndex nLastPAIX = -1;

    // Find multi-valued parts of the path
    std::map<std::pair<AttrCode, AttrIndex>, int> oMapOccurrenceCount;
    for (AttrIndex iATTR = 0; iATTR < nRepeatCount; ++iATTR)
    {
        const auto GetIntSubfield =
            [poRecord, poATTRField, iATTR](const char *pszSubFieldName)
        {
            return poRecord->GetIntSubfield(poATTRField, pszSubFieldName,
                                            static_cast<int>(iATTR));
        };

        ++oMapOccurrenceCount[{GetIntSubfield(NATC_SUBFIELD),
                               GetIntSubfield(PAIX_SUBFIELD)}];
    }

    const size_t nS101AttrDefsBaseIdx = asS101AttrDefs.size();

    for (AttrIndex iATTR = 0; iATTR < nRepeatCount; ++iATTR)
    {
        const auto GetIntSubfield =
            [poRecord, poATTRField, iATTR](const char *pszSubFieldName)
        {
            return poRecord->GetIntSubfield(poATTRField, pszSubFieldName,
                                            static_cast<int>(iATTR));
        };

        const int nATIN = GetIntSubfield(ATIN_SUBFIELD);
        if (nATIN != INSTRUCTION_INSERT)
        {
            if (!EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s: wrong value %d for ATIN subfield.",
                               GetErrorContext(iATTR), nATIN)))
            {
                return false;
            }
            nLastNATC = -1;
            nLastATIX = -1;
            nLastPAIX = -1;
            asS101AttrDefs.push_back(S101AttrDef());
            continue;
        }

        const AttrCode nNATC(GetIntSubfield(NATC_SUBFIELD));
        if (!cpl::contains(m_attributeCodes, nNATC) &&
            !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: cannot find attribute code %d in ATCS field "
                           "of the Dataset General Information Record%s.",
                           GetErrorContext(iATTR), static_cast<int>(nNATC),
                           bAttributeCodesSequential
                               ? CPLSPrintf(". Must be in [1, %d]",
                                            static_cast<int>(nLargestNATC))
                               : "")))
        {
            return false;
        }

        const AttrRepeat nATIX(GetIntSubfield(ATIX_SUBFIELD));
        if (!(nATIX >= 1 && nATIX <= nRepeatCount))
        {
            if (!EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s: wrong value %d for ATIX subfield. "
                               "Must be in [1, %d].",
                               GetErrorContext(iATTR), static_cast<int>(nATIX),
                               nRepeatCount)))
            {
                return false;
            }
            nLastNATC = -1;
            nLastATIX = -1;
            nLastPAIX = -1;
            asS101AttrDefs.push_back(S101AttrDef());
            continue;
        }

        const AttrIndex nPAIX = GetIntSubfield(PAIX_SUBFIELD);
        // The parent index must be lower than the current attribute index,
        // since parents are required to be listed before.
        if (!(nPAIX >= 0 && nPAIX <= iATTR))
        {
            if (!EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s: wrong value %d for PAIX subfield. "
                               "Must be in [0, %d].",
                               GetErrorContext(iATTR), static_cast<int>(nPAIX),
                               static_cast<int>(iATTR))))
            {
                return false;
            }
            nLastNATC = -1;
            nLastATIX = -1;
            nLastPAIX = -1;
            asS101AttrDefs.push_back(S101AttrDef());
            continue;
        }

        if (nPAIX == nLastPAIX)
        {
            if (nNATC == nLastNATC)
            {
                if (nATIX != nLastATIX + 1 &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: wrong value %d for ATIX subfield. Expected %d.",
                        GetErrorContext(iATTR), static_cast<int>(nATIX),
                        static_cast<int>(nLastATIX + 1))))
                {
                    return false;
                }
            }
            else if (nATIX != 1)
            {
                if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: wrong value %d for ATIX subfield. Expected %d.",
                        GetErrorContext(iATTR), static_cast<int>(nATIX), 1)))
                {
                    return false;
                }
            }
        }

        // (NATC,ATIX,PAIX) tuple should be unique within a record
        if (!oSetNATC_ATIX_PAIX.insert({nNATC, nATIX, nPAIX}).second &&
            !EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s: several instances of "
                           "(NATC,ATIX,PAIX)=(%d,%d,%d) "
                           "in field %s of the same record.",
                           GetErrorContext(iATTR), static_cast<int>(nNATC),
                           static_cast<int>(nATIX), static_cast<int>(nPAIX),
                           pszAttrFieldName)))
        {
            return false;
        }

        // Does this attribute have a parent?
        const bool bIsMultiValued = oMapOccurrenceCount[{nNATC, nPAIX}] > 1;
        PathVector oReversedPath{{nNATC, bIsMultiValued ? nATIX : 0}};
        if (nPAIX > 0)
        {
            // Assertion can't trigger because nPAIX <= iATTR < asS101AttrDefs.size() - nS101AttrDefsBaseIdx
            CPLAssert(nS101AttrDefsBaseIdx +
                          static_cast<size_t>(static_cast<int>(nPAIX) - 1) <
                      asS101AttrDefs.size());
            auto &sParentAttrDef = asS101AttrDefs[nS101AttrDefsBaseIdx +
                                                  static_cast<int>(nPAIX) - 1];
            sParentAttrDef.bIsParent = true;
            if (!sParentAttrDef.osVal.empty() &&
                !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s: parent attribute of index PAIX=%d has "
                    "a non empty ATVL subfield.",
                    GetErrorContext(iATTR), static_cast<int>(nPAIX))))
            {
                return false;
            }
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
            oReversedPath.insert(oReversedPath.end(),
                                 sParentAttrDef.oReversedPath.begin(),
                                 sParentAttrDef.oReversedPath.end());
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        }

        const char *pszATVL = poRecord->GetStringSubfield(
            poATTRField, ATVL_SUBFIELD, static_cast<int>(iATTR));
        if (!pszATVL &&
            !EMIT_ERROR_OR_WARNING(CPLSPrintf("%s: cannot read ATVL subfield.",
                                              GetErrorContext(iATTR))))
        {
            return false;
        }

        S101AttrDef sAttrDef;
        sAttrDef.iField = iField;
        sAttrDef.bMultipleFields = bMultipleFields;
        sAttrDef.oReversedPath = std::move(oReversedPath);
        if (pszATVL)
            sAttrDef.osVal = pszATVL;
        asS101AttrDefs.push_back(std::move(sAttrDef));

        nLastNATC = nNATC;
        nLastATIX = nATIX;
        nLastPAIX = nPAIX;
    }

    return true;
}

/************************************************************************/
/*                          IngestAttributes()                          */
/************************************************************************/

/** For a given record that has a ATTR/INAS/FACS field, ingest all attributes
 * from all instances of this field.
 */
bool OGRS101Reader::IngestAttributes(
    const DDFRecord *poRecord, int iRecord, const char *pszIDFieldName,
    const char *pszAttrFieldName,
    std::vector<S101AttrDef> &asS101AttrDefs) const
{
    asS101AttrDefs.clear();

    const auto apoATTRFields = poRecord->GetFields(pszAttrFieldName);
    const int nATTRFieldCount = static_cast<int>(apoATTRFields.size());
    bool bSuccess = true;
    if (EQUAL(pszAttrFieldName, ATTR_FIELD))
    {
        for (int iATTRField = 0; bSuccess && iATTRField < nATTRFieldCount;
             ++iATTRField)
        {
            bSuccess = IngestAttributes(poRecord, iRecord, pszIDFieldName,
                                        pszAttrFieldName,
                                        apoATTRFields[iATTRField], iATTRField,
                                        nATTRFieldCount > 1, asS101AttrDefs);
        }
    }
    else
    {
        for (int iATTRField = 0; bSuccess && iATTRField < nATTRFieldCount;
             ++iATTRField)
        {
            const auto poINASOrFASCField = apoATTRFields[iATTRField];
            if (poINASOrFASCField->GetParts().size() != 2)
            {
                if (!EMIT_ERROR_OR_WARNING(
                        CPLSPrintf("Record index=%d of %s: missing components "
                                   "in %s field.",
                                   iRecord, pszIDFieldName, pszAttrFieldName)))
                {
                    return false;
                }
                return true;
            }
            const auto poATTRField = poINASOrFASCField->GetParts()[1].get();

            bSuccess = IngestAttributes(
                poRecord, iRecord, pszIDFieldName, pszAttrFieldName,
                poATTRField, iATTRField, nATTRFieldCount > 1, asS101AttrDefs);
        }
    }

    for (auto &sAttrDef : asS101AttrDefs)
    {
        if (sAttrDef.bIsParent || sAttrDef.oReversedPath.empty())
            continue;

        // For last component, set the repetition part to 0, to be
        // actually able to detect multi-valued attributes!
        sAttrDef.oReversedPath.front().second = 0;
    }

    return bSuccess;
}

/************************************************************************/
/*                           BuildFieldName()                           */
/************************************************************************/

/** Returns a string with the concatenation of the parts, in reverse order.
 */
std::string OGRS101Reader::BuildFieldName(const PathVector &oReversedPath,
                                          const char *pszAttrFieldName,
                                          int iField, bool bMultipleFields,
                                          const char *pszIDFieldName) const
{
    std::string osAttrName;
    for (size_t i = oReversedPath.size(); i > 0;)
    {
        --i;
        const auto &oPathComp = oReversedPath[i];
        if (!osAttrName.empty())
            osAttrName += '.';
        const auto nCode = oPathComp.first;
        const auto oIterNATC = m_attributeCodes.find(nCode);
        if (oIterNATC == m_attributeCodes.end())
        {
            osAttrName += CPLSPrintf("code_%d", static_cast<int>(nCode));
        }
        else
        {
            osAttrName += oIterNATC->second;
        }

        if (bMultipleFields && strcmp(pszAttrFieldName, ATTR_FIELD) == 0)
        {
            osAttrName += '[';
            osAttrName += std::to_string(iField + 1);
            osAttrName += ']';
            bMultipleFields = false;
        }

        const auto &nRepeat = oPathComp.second;
        if (nRepeat > 0)
        {
            osAttrName += '[';
            osAttrName += std::to_string(static_cast<int>(nRepeat));
            osAttrName += ']';
        }
    }

    if (strcmp(pszAttrFieldName, ATTR_FIELD) != 0)
    {
        std::string osPrefix;
        if (strcmp(pszIDFieldName, IRID_FIELD) == 0)
        {
            osPrefix = "association";
        }
        else if (strcmp(pszIDFieldName, FRID_FIELD) == 0)
        {
            if (strcmp(pszAttrFieldName, INAS_FIELD) == 0)
                osPrefix = "infoAssociation";
            else
                osPrefix = "featureAssociation";
        }
        if (!osPrefix.empty())
        {
            if (bMultipleFields)
            {
                osPrefix += '[';
                osPrefix += std::to_string(iField + 1);
                osPrefix += ']';
            }
            osPrefix += '_';
        }
        osAttrName = osPrefix + osAttrName;
    }

    return osAttrName;
}

/************************************************************************/
/*                          InferFeatureDefn()                          */
/************************************************************************/

/** Infer the feature definition from the content of INAS or ATTR records
 * of the index.
 */
bool OGRS101Reader::InferFeatureDefn(
    const DDFRecordIndex &oIndex, const char *pszIDFieldName,
    const char *pszAttrFieldName, const std::vector<int> &anRecordIndices,
    OGRFeatureDefn &oFeatureDefn,
    std::map<std::string, std::unique_ptr<OGRFieldDomain>> &oMapFieldDomains,
    const OGRS101FeatureCatalogTypes::InformationType * /*psInformationType*/,
    const OGRS101FeatureCatalogTypes::FeatureType *psFeatureType) const
{
    const bool bIsINAS = EQUAL(pszAttrFieldName, INAS_FIELD);

    struct OGRAttrDef
    {
        std::optional<OGRFieldType> oeType{};
        OGRFieldSubType eSubType = OFSTNone;
        bool bIsMultiValued = false;
        bool bMultipleFields = false;
        std::string osLongerName{};
        std::string osDefinition{};
        std::string osFieldDomainName{};
    };

    using PathVectorAndAttrIdx = std::pair<PathVector, int>;
    std::map<PathVectorAndAttrIdx, OGRAttrDef> oMapFieldTypes;

    std::vector<S101AttrDef> asS101AttrDefs;
    std::map<PathVectorAndAttrIdx, int> mapPathToCount;
    bool bFoundValidAssocField = false;

    // Iterate over the records (in the index of interest) to fill the
    // oMapFieldTypes map object that will be afterwards translated as
    // OGR feature definition
    // If anRecordIndices is not empty, it defines the subset of record
    // indices to iterate over. This is used for geometry records that are
    // dispatched to different OGR layers depending on the CRS.
    const int nRecords = anRecordIndices.empty()
                             ? oIndex.GetCount()
                             : static_cast<int>(anRecordIndices.size());
    int nMaxFieldRepeat = 1;
    for (int iter = 0; iter < nRecords; ++iter)
    {
        const int iRecord =
            anRecordIndices.empty() ? iter : anRecordIndices[iter];

        const auto GetErrorContext = [pszIDFieldName, iRecord]()
        {
            return CPLSPrintf("Record index=%d of %s", iRecord, pszIDFieldName);
        };

        const auto poRecord = oIndex.GetByIndex(iRecord);
        CPLAssert(poRecord);

        const int nRUIN =
            poRecord->GetIntSubfield(pszIDFieldName, 0, RUIN_SUBFIELD, 0);
        if (nRUIN != INSTRUCTION_INSERT)
        {
            if (!EMIT_ERROR_OR_WARNING(CPLSPrintf("%s: wrong value %d for RUIN "
                                                  "subfield of %s field.",
                                                  GetErrorContext(), nRUIN,
                                                  pszIDFieldName)))
            {
                return false;
            }
            continue;
        }

        if (!EQUAL(pszAttrFieldName, ATTR_FIELD))
        {
            const auto apoFields = poRecord->GetFields(pszAttrFieldName);
            bool bSkipRecord = false;
            nMaxFieldRepeat =
                std::max(nMaxFieldRepeat, static_cast<int>(apoFields.size()));
            if (!apoFields.empty())
            {
                const DDFField *poField = apoFields[0];

                const RecordName nRRNM =
                    poRecord->GetIntSubfield(poField, RRNM_SUBFIELD, 0);
                const RecordName nExpectedRRNM =
                    bIsINAS ? RECORD_NAME_INFORMATION_TYPE
                            : RECORD_NAME_FEATURE_TYPE;
                if (nRRNM != nExpectedRRNM)
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s: Invalid value for RRNM subfield of %s field: "
                            "got %d, expected %d.",
                            GetErrorContext(), pszAttrFieldName,
                            static_cast<int>(nRRNM),
                            static_cast<int>(nExpectedRRNM))))
                    {
                        return false;
                    }
                }

                const int nRRID =
                    poRecord->GetIntSubfield(poField, RRID_SUBFIELD, 0);
                if ((bIsINAS &&
                     !m_oInformationTypeRecordIndex.FindRecord(nRRID)) ||
                    (!bIsINAS && !m_oFeatureTypeRecordIndex.FindRecord(nRRID)))
                {
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s: Invalid value %d for RRID subfield of %s "
                            "field: "
                            "does not match the record identifier of an "
                            "existing "
                            "%s record.",
                            GetErrorContext(), static_cast<int>(nRRID),
                            pszAttrFieldName,
                            bIsINAS ? "InformationType" : "FeatureType")))
                    {
                        return false;
                    }
                }

                if (bIsINAS)
                {
                    const InfoAssocCode nNIAC =
                        poRecord->GetIntSubfield(poField, NIAC_SUBFIELD, 0);
                    if (!cpl::contains(m_informationAssociationCodes, nNIAC) &&
                        !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s: cannot find attribute code %d in IACS field "
                            "of the Dataset General Information Record.",
                            GetErrorContext(), static_cast<int>(nNIAC))))
                    {
                        return false;
                    }
                }
                else
                {
                    const FeatureAssocCode nNFAC =
                        poRecord->GetIntSubfield(poField, NFAC_SUBFIELD, 0);
                    if (!cpl::contains(m_featureAssociationCodes, nNFAC) &&
                        !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "%s: cannot find attribute code %d in NFAC field "
                            "of the Dataset General Information Record.",
                            GetErrorContext(), static_cast<int>(nNFAC))))
                    {
                        return false;
                    }
                }

                const AssocRoleCode nNARC =
                    poRecord->GetIntSubfield(poField, NARC_SUBFIELD, 0);
                if (!cpl::contains(m_associationRoleCodes, nNARC) &&
                    !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s: cannot find attribute code %d in ARCS field "
                        "of the Dataset General Information Record.",
                        GetErrorContext(), static_cast<int>(nNARC))))
                {
                    return false;
                }

                const char *pszSubFieldName =
                    bIsINAS ? IUIN_SUBFIELD : FAUI_SUBFIELD;
                int nInstruction =
                    poRecord->GetIntSubfield(poField, pszSubFieldName, 0);
                if (nInstruction == 0)
                {
                    // For 101GB00GB302045.000, non conformant
                    nInstruction = poRecord->GetIntSubfield(poField, "APUI", 0);
                }
                if (nInstruction != INSTRUCTION_INSERT)
                {
                    if (!EMIT_ERROR_OR_WARNING(
                            CPLSPrintf("%s: wrong value %d for %s "
                                       "subfield of %s field.",
                                       GetErrorContext(), nInstruction,
                                       pszSubFieldName, pszAttrFieldName)))
                    {
                        return false;
                    }
                    bSkipRecord = true;
                }
                else
                {
                    bFoundValidAssocField = true;
                }
            }
            if (bSkipRecord)
                continue;
        }

        // First (inner) pass over attributes of the current record
        // to fill asS101AttrDefs, and do all needed sanity checks
        if (!IngestAttributes(poRecord, iRecord, pszIDFieldName,
                              pszAttrFieldName, asS101AttrDefs))
            return false;

        mapPathToCount.clear();

        // Update oMapFieldTypes with attributes found in this record
        for (const auto &sAttrDef : asS101AttrDefs)
        {
            if (sAttrDef.bIsParent || sAttrDef.oReversedPath.empty())
                continue;

            // Check that the top-level part of the attribute is expected for
            // that feature type (using feature catalog)
            if (psFeatureType)
            {
                const auto oIterNATC =
                    m_attributeCodes.find(sAttrDef.oReversedPath.back().first);
                if (oIterNATC != m_attributeCodes.end())
                {
                    const std::string &osAttrCode = oIterNATC->second;
                    if (!cpl::contains(psFeatureType->attributeBindings,
                                       osAttrCode))
                    {
                        if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "%s: attribute code %s not expected in feature "
                                "type %s",
                                GetErrorContext(), osAttrCode.c_str(),
                                psFeatureType->code.c_str())))
                        {
                            return false;
                        }
                    }
                }
            }

            const auto key =
                std::make_pair(sAttrDef.oReversedPath, sAttrDef.iField);
            ++mapPathToCount[key];

            // Must be kept in that scope to create a OGR attribute even if
            // there is no field value
            auto &sOGRAttrDef = oMapFieldTypes[key];

            std::string typeFromCatalog;
            if (m_poFeatureCatalog)
            {
                auto oIterNATC =
                    m_attributeCodes.find(sAttrDef.oReversedPath.front().first);
                if (oIterNATC != m_attributeCodes.end())
                {
                    const auto &oMap =
                        m_poFeatureCatalog->GetSimpleAttributes();
                    const std::string &osAttrCode = oIterNATC->second;
                    const auto oIterAttr = oMap.find(osAttrCode);
                    if (oIterAttr != oMap.end())
                    {
                        const auto &attrDef = oIterAttr->second;
                        typeFromCatalog = attrDef.type;

                        sOGRAttrDef.osLongerName = attrDef.name;
                        sOGRAttrDef.osDefinition = attrDef.definition;

                        if (typeFromCatalog ==
                            OGRS101FeatureCatalog::VALUE_TYPE_ENUMERATION)
                        {
                            sOGRAttrDef.osFieldDomainName = osAttrCode;

                            if (!sAttrDef.osVal.empty())
                            {
                                // Checks that the coded value is an allowed code.
                                const int nCode = atoi(sAttrDef.osVal.c_str());
                                if (!cpl::contains(attrDef.enumeratedValues,
                                                   nCode))
                                {
                                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                            "%s: value %s does not belong to "
                                            "enumeration of attribute code %s",
                                            GetErrorContext(),
                                            sAttrDef.osVal.c_str(),
                                            osAttrCode.c_str())))
                                    {
                                        return false;
                                    }
                                }
                            }

                            // Check if field domain exists. If not, create it.
                            if (!cpl::contains(oMapFieldDomains, osAttrCode))
                            {
                                std::vector<OGRCodedValue> asValues;
                                for (const auto &[code, value] :
                                     attrDef.enumeratedValues)
                                {
                                    OGRCodedValue codedValue;
                                    codedValue.pszCode =
                                        CPLStrdup(CPLSPrintf("%d", code));
                                    codedValue.pszValue =
                                        CPLStrdup(value.c_str());
                                    asValues.push_back(std::move(codedValue));
                                }
                                auto poFieldDomain =
                                    std::make_unique<OGRCodedFieldDomain>(
                                        osAttrCode, attrDef.name, OFTString,
                                        OFSTNone, std::move(asValues));
                                oMapFieldDomains[osAttrCode] =
                                    std::move(poFieldDomain);
                            }
                        }
                    }
                }

                for (size_t i = 1; i < sAttrDef.oReversedPath.size(); ++i)
                {
                    oIterNATC =
                        m_attributeCodes.find(sAttrDef.oReversedPath[i].first);
                    if (oIterNATC != m_attributeCodes.end())
                    {
                        const auto &oMap =
                            m_poFeatureCatalog->GetComplexAttributes();
                        const std::string &osAttrCode = oIterNATC->second;
                        const auto oIterAttr = oMap.find(osAttrCode);
                        if (oIterAttr != oMap.end())
                        {
                            const auto &attrDef = oIterAttr->second;
                            sOGRAttrDef.osDefinition += ' ';
                            sOGRAttrDef.osDefinition += oIterNATC->second;
                            sOGRAttrDef.osDefinition += '=';
                            sOGRAttrDef.osDefinition += attrDef.definition;
                        }
                    }
                }
            }

            sOGRAttrDef.bMultipleFields = sAttrDef.bMultipleFields;

            if (!sAttrDef.osVal.empty())
            {
                const bool bNewAttrIsMultiValued = mapPathToCount[key] > 1;
                if (bNewAttrIsMultiValued)
                    sOGRAttrDef.bIsMultiValued = true;
                const auto eCPLType = CPLGetValueType(sAttrDef.osVal.c_str());
                auto eOGRType = eCPLType == CPL_VALUE_STRING    ? OFTString
                                : eCPLType == CPL_VALUE_INTEGER ? OFTInteger
                                                                : OFTReal;

                // Is it YYYYMMDD date ?
                if (eOGRType == OFTInteger && sAttrDef.osVal.size() == 8 &&
                    sAttrDef.osVal[4] <= '1' && sAttrDef.osVal[6] <= '3')
                {
                    const auto it = m_attributeCodes.find(
                        sAttrDef.oReversedPath.front().first);
                    if (it != m_attributeCodes.end())
                    {
                        if (cpl::ends_with(it->second, "Date") ||
                            cpl::ends_with(it->second, "dateStart") ||
                            cpl::ends_with(it->second, "dateEnd"))
                        {
                            eOGRType = OFTDate;
                        }
                    }
                }
                // Is it YYYY---- truncated date ?
                else if (eOGRType == OFTString && sAttrDef.osVal.size() == 8 &&
                         sAttrDef.osVal[4] == '-' && sAttrDef.osVal[5] == '-' &&
                         sAttrDef.osVal[6] == '-' && sAttrDef.osVal[7] == '-')
                {
                    const auto it = m_attributeCodes.find(
                        sAttrDef.oReversedPath.front().first);
                    if (it != m_attributeCodes.end())
                    {
                        if (cpl::ends_with(it->second, "Date") ||
                            cpl::ends_with(it->second, "dateStart") ||
                            cpl::ends_with(it->second, "dateEnd"))
                        {
                            eOGRType = OFTDate;
                        }
                    }
                }
                // Is it time format ? ("094500", "094500", "094500+0100")
                else if ((eOGRType == OFTInteger || eOGRType == OFTString) &&
                         sAttrDef.osVal.size() >= 6 &&
                         sAttrDef.osVal.size() <= 11 &&
                         std::all_of(sAttrDef.osVal.begin(),
                                     sAttrDef.osVal.begin() + 6, [](char c)
                                     { return c >= '0' && c <= '9'; }) &&
                         (sAttrDef.osVal.size() == 6 ||
                          (sAttrDef.osVal.size() == 7 &&
                           sAttrDef.osVal[6] == 'Z') ||
                          (sAttrDef.osVal.size() == 11 &&
                           (sAttrDef.osVal[6] == '+' ||
                            sAttrDef.osVal[6] == '-'))))
                {
                    const auto it = m_attributeCodes.find(
                        sAttrDef.oReversedPath.front().first);
                    if (it != m_attributeCodes.end())
                    {
                        if (cpl::starts_with(it->second, "time"))
                        {
                            eOGRType = OFTTime;
                        }
                    }
                }

                if (!sOGRAttrDef.oeType.has_value())
                {
                    sOGRAttrDef.oeType = eOGRType;
                    if (eOGRType == OFTInteger &&
                        typeFromCatalog ==
                            OGRS101FeatureCatalog::VALUE_TYPE_BOOLEAN)
                    {
                        sOGRAttrDef.eSubType = OFSTBoolean;
                    }
                }
                else if (eOGRType == OFTString &&
                         *sOGRAttrDef.oeType != OFTString)
                {
                    sOGRAttrDef.oeType = OFTString;
                    sOGRAttrDef.eSubType = OFSTNone;
                }
                else if (eOGRType == OFTReal &&
                         *sOGRAttrDef.oeType == OFTInteger)
                {
                    sOGRAttrDef.oeType = OFTReal;
                    sOGRAttrDef.eSubType = OFSTNone;
                }
            }
        }
    }

    if (bFoundValidAssocField)
    {
        for (int i = 0; i < nMaxFieldRepeat; ++i)
        {
            const std::string osSuffix =
                nMaxFieldRepeat > 1 ? CPLSPrintf("[%d]", i + 1) : "";

            if (!bIsINAS)
            {
                OGRFieldDefn oFieldDefn(
                    (OGR_FIELD_NAME_REF_FEAT_LAYER_NAME + osSuffix).c_str(),
                    OFTString);
                oFeatureDefn.AddFieldDefn(&oFieldDefn);
            }

            {
                OGRFieldDefn oFieldDefn(
                    ((bIsINAS ? OGR_FIELD_NAME_REF_INFO_RID
                              : OGR_FIELD_NAME_REF_FEAT_RID) +
                     osSuffix)
                        .c_str(),
                    OFTInteger);
                oFeatureDefn.AddFieldDefn(&oFieldDefn);
            }
            {
                OGRFieldDefn oFieldDefn(
                    ((bIsINAS ? OGR_FIELD_NAME_NIAC : OGR_FIELD_NAME_NFAC) +
                     osSuffix)
                        .c_str(),
                    OFTString);
                oFeatureDefn.AddFieldDefn(&oFieldDefn);
            }
            {
                OGRFieldDefn oFieldDefn(
                    ((bIsINAS ? OGR_FIELD_NAME_NARC
                              : OGR_FIELD_NAME_FEATURE_NARC) +
                     osSuffix)
                        .c_str(),
                    OFTString);
                oFeatureDefn.AddFieldDefn(&oFieldDefn);
            }
        }
    }

    // Final pass to transform oMapFieldTypes into OGRField instances.
    for (const auto &[oReversedPathAndFieldIndex, sOGRAttrDef] : oMapFieldTypes)
    {
        const auto &oReversedPath = oReversedPathAndFieldIndex.first;
        const int iField = oReversedPathAndFieldIndex.second;

        const std::string osAttrName =
            BuildFieldName(oReversedPath, pszAttrFieldName, iField,
                           sOGRAttrDef.bMultipleFields, pszIDFieldName);

        OGRFieldType eType = OFTString;
        if (sOGRAttrDef.oeType.has_value())
        {
            if (sOGRAttrDef.bIsMultiValued)
            {
                eType = (*sOGRAttrDef.oeType) == OFTInteger ? OFTIntegerList
                        : (*sOGRAttrDef.oeType) == OFTReal  ? OFTRealList
                                                            : OFTStringList;
            }
            else
            {
                eType = *sOGRAttrDef.oeType;
            }
        }
        else if (sOGRAttrDef.bIsMultiValued)
        {
            eType = OFTStringList;
        }

        if (oFeatureDefn.GetFieldIndex(osAttrName.c_str()) >= 0)
        {
            if (osAttrName == OGR_FIELD_NAME_SMIN ||
                osAttrName == OGR_FIELD_NAME_SMAX)
            {
                // 101FR00368570.000 has scaleMinimum as an ATTR, and
                // doesn't define SPAS.SMIN
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer %s: %s field already exists",
                         oFeatureDefn.GetName(), osAttrName.c_str());
            }
        }
        else
        {
            OGRFieldDefn oFieldDefn(osAttrName.c_str(), eType);
            oFieldDefn.SetSubType(sOGRAttrDef.eSubType);
            if (!sOGRAttrDef.osLongerName.empty() && oReversedPath.size() == 1)
                oFieldDefn.SetAlternativeName(sOGRAttrDef.osLongerName.c_str());
            if (!sOGRAttrDef.osDefinition.empty())
                oFieldDefn.SetComment(sOGRAttrDef.osDefinition.c_str());
            if (!sOGRAttrDef.osFieldDomainName.empty())
                oFieldDefn.SetDomainName(sOGRAttrDef.osFieldDomainName.c_str());
            oFeatureDefn.AddFieldDefn(&oFieldDefn);
        }
    }

    return true;
}

/************************************************************************/
/*                       FillFeatureAttributes()                        */
/************************************************************************/

/** Fill attribute fields of the provided feature.
 */
bool OGRS101Reader::FillFeatureAttributes(const DDFRecordIndex &oIndex,
                                          int iRecord,
                                          const char *pszAttrFieldName,
                                          OGRFeature &oFeature) const
{
    const auto poRecord = oIndex.GetByIndex(iRecord);
    if (!poRecord)
    {
        return EMIT_ERROR("Invalid record number");
    }

    const auto poIDField = poRecord->GetField(0);
    CPLAssert(poIDField);
    const char *pszIDFieldName = poIDField->GetFieldDefn()->GetName();

    const int nRCID =
        poRecord->GetIntSubfield(pszIDFieldName, 0, RCID_SUBFIELD, 0);
    if (nRCID < 1 && !EMIT_ERROR_OR_WARNING(CPLSPrintf(
                         "Wrong value %d for RCID subfield of %s field.", nRCID,
                         pszIDFieldName)))
    {
        return false;
    }
    oFeature.SetField(OGR_FIELD_NAME_RECORD_ID, nRCID);

    const int nRVER =
        poRecord->GetIntSubfield(pszIDFieldName, 0, RVER_SUBFIELD, 0);
    oFeature.SetField(OGR_FIELD_NAME_RECORD_VERSION, nRVER);

    const int nRUIN =
        poRecord->GetIntSubfield(pszIDFieldName, 0, RUIN_SUBFIELD, 0);
    if (nRUIN != INSTRUCTION_INSERT)
    {
        return EMIT_ERROR_OR_WARNING(
            CPLSPrintf("Wrong value %d for RUIN subfield of %s field.", nRUIN,
                       pszIDFieldName));
    }

    const auto poFeatureDefn = oFeature.GetDefnRef();

    // First pass to detect which attribute entries correspond to parent nodes
    // that don't directly hold a field value.
    std::vector<S101AttrDef> asS101AttrDefs;
    if (!IngestAttributes(poRecord, iRecord, pszIDFieldName, pszAttrFieldName,
                          asS101AttrDefs))
        return false;

    struct OGRFieldIndexTag
    {
    };

    using OGRFieldIndex = cpl::IntWrapper<OGRFieldIndexTag>;

    std::map<OGRFieldIndex, CPLStringList> stringListAttrs;
    std::map<OGRFieldIndex, std::vector<int>> intListAttrs;
    std::map<OGRFieldIndex, std::vector<double>> doubleListAttrs;
    // Second pass to set single-valued attributes, or store multi-valued
    // attributes in the 3 above maps.
    for (const auto &sAttrDef : asS101AttrDefs)
    {
        if (sAttrDef.bIsParent || sAttrDef.oReversedPath.empty())
            continue;

        const std::string osAttrName = BuildFieldName(
            sAttrDef.oReversedPath, pszAttrFieldName, sAttrDef.iField,
            sAttrDef.bMultipleFields, pszIDFieldName);

        const int iOGRFieldIdx =
            poFeatureDefn->GetFieldIndex(osAttrName.c_str());
        // Shouldn't normally happen given all preceding run logic
        CPLAssert(iOGRFieldIdx >= 0);
        const auto eType = poFeatureDefn->GetFieldDefn(iOGRFieldIdx)->GetType();
        const char *const pszATVL = sAttrDef.osVal.c_str();
        switch (eType)
        {
            case OFTInteger:
            case OFTIntegerList:
            {
                if (pszATVL[0])
                {
                    const char *const last = pszATVL + sAttrDef.osVal.size();
                    int nVal = -1;
                    auto [ptr, ec] = std::from_chars(pszATVL, last, nVal);
                    if (ec == std::errc() && ptr == last)
                    {
                        if (eType == OFTInteger)
                            oFeature.SetField(iOGRFieldIdx, nVal);
                        else
                            intListAttrs[iOGRFieldIdx].push_back(nVal);
                    }
                    else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                 "Record index=%d of %s, attribute %s: "
                                 "non integer value '%s'.",
                                 iRecord, pszIDFieldName, osAttrName.c_str(),
                                 pszATVL)))
                    {
                        return false;
                    }
                }
                else if (eType == OFTIntegerList)
                {
                    intListAttrs[iOGRFieldIdx].push_back(
                        std::numeric_limits<int>::min());
                }
                break;
            }

            case OFTReal:
            case OFTRealList:
            {
                if (pszATVL[0])
                {
                    const char *const last = pszATVL + sAttrDef.osVal.size();
                    double dfVal = -1;
                    const fast_float::parse_options options{
                        fast_float::chars_format::general, '.'};
                    auto [ptr, ec] = fast_float::from_chars_advanced(
                        pszATVL, last, dfVal, options);
                    if (ec == std::errc() && ptr == last)
                    {
                        if (eType == OFTReal)
                            oFeature.SetField(iOGRFieldIdx, dfVal);
                        else
                            doubleListAttrs[iOGRFieldIdx].push_back(dfVal);
                    }
                    else if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                 "Record index=%d of %s, attribute %s: "
                                 "non double value '%s'.",
                                 iRecord, pszIDFieldName, osAttrName.c_str(),
                                 pszATVL)))
                    {
                        return false;
                    }
                }
                else if (eType == OFTRealList)
                {
                    doubleListAttrs[iOGRFieldIdx].push_back(
                        std::numeric_limits<double>::quiet_NaN());
                }
                break;
            }

            case OFTString:
            case OFTStringList:
            {
                std::unique_ptr<char, VSIFreeReleaser> pszTmpStr;
                const char *pszStr = pszATVL;
                if (!CPLIsUTF8(pszATVL,
                               static_cast<int>(sAttrDef.osVal.size())))
                {
                    // Not supposed to happen in compliant products
                    if (!EMIT_ERROR_OR_WARNING(CPLSPrintf(
                            "Record index=%d of %s, attribute %s: non "
                            "UTF-8 string '%s'.",
                            iRecord, pszIDFieldName, osAttrName.c_str(),
                            pszATVL)))
                    {
                        return false;
                    }
                    pszTmpStr.reset(CPLUTF8ForceToASCII(pszATVL, '_'));
                    pszStr = pszTmpStr.get();
                }
                if (eType == OFTString)
                    oFeature.SetField(iOGRFieldIdx, pszStr);
                else
                    stringListAttrs[iOGRFieldIdx].push_back(pszStr);
                break;
            }

            case OFTDate:
            {
                if (sAttrDef.osVal.size() == 8 &&
                    std::all_of(sAttrDef.osVal.begin(), sAttrDef.osVal.end(),
                                [](char c) { return c >= '0' && c <= '9'; }))
                {
                    const int nYear = (sAttrDef.osVal[0] - '0') * 1000 +
                                      (sAttrDef.osVal[1] - '0') * 100 +
                                      (sAttrDef.osVal[2] - '0') * 10 +
                                      (sAttrDef.osVal[3] - '0');
                    const int nMonth = (sAttrDef.osVal[4] - '0') * 10 +
                                       (sAttrDef.osVal[5] - '0');
                    const int nDay = (sAttrDef.osVal[6] - '0') * 10 +
                                     (sAttrDef.osVal[7] - '0');
                    oFeature.SetField(iOGRFieldIdx, nYear, nMonth, nDay);
                }
                else if (sAttrDef.osVal.size() == 8 &&
                         sAttrDef.osVal[0] >= '0' && sAttrDef.osVal[0] <= '9' &&
                         sAttrDef.osVal[1] >= '0' && sAttrDef.osVal[1] <= '9' &&
                         sAttrDef.osVal[2] >= '0' && sAttrDef.osVal[2] <= '9' &&
                         sAttrDef.osVal[3] >= '0' && sAttrDef.osVal[3] <= '9' &&
                         sAttrDef.osVal[4] == '-' && sAttrDef.osVal[5] == '-' &&
                         sAttrDef.osVal[6] == '-' && sAttrDef.osVal[7] == '-')
                {
                    const int nYear = (sAttrDef.osVal[0] - '0') * 1000 +
                                      (sAttrDef.osVal[1] - '0') * 100 +
                                      (sAttrDef.osVal[2] - '0') * 10 +
                                      (sAttrDef.osVal[3] - '0');
                    if (cpl::ends_with(osAttrName, "dateEnd"))
                    {
                        oFeature.SetField(iOGRFieldIdx, nYear, 12, 31);
                    }
                    else
                    {
                        oFeature.SetField(iOGRFieldIdx, nYear, 1, 1);
                    }
                }
                break;
            }

            case OFTTime:
            {
                if (sAttrDef.osVal.size() >= 6 &&
                    std::all_of(sAttrDef.osVal.begin(),
                                sAttrDef.osVal.begin() + 6,
                                [](char c) { return c >= '0' && c <= '9'; }))
                {
                    const int nHour = (sAttrDef.osVal[0] - '0') * 10 +
                                      (sAttrDef.osVal[1] - '0');
                    const int nMin = (sAttrDef.osVal[2] - '0') * 10 +
                                     (sAttrDef.osVal[3] - '0');
                    const int nSec = (sAttrDef.osVal[4] - '0') * 10 +
                                     (sAttrDef.osVal[5] - '0');
                    int nTZFlag = OGR_TZFLAG_UNKNOWN;
                    if (sAttrDef.osVal.size() == 7 && sAttrDef.osVal[6] == 'Z')
                        nTZFlag = OGR_TZFLAG_UTC;
                    else if (sAttrDef.osVal.size() == 11)
                    {
                        const int nTZHour = (sAttrDef.osVal[7] - '0') * 10 +
                                            (sAttrDef.osVal[8] - '0');
                        const int nTZMin = (sAttrDef.osVal[9] - '0') * 10 +
                                           (sAttrDef.osVal[10] - '0');
                        const int n15Minutes = (nTZHour * 60 + nTZMin) / 15;
                        if (sAttrDef.osVal[6] == '+')
                            nTZFlag = OGR_TZFLAG_UTC + n15Minutes;
                        else
                            nTZFlag = OGR_TZFLAG_UTC - n15Minutes;
                    }
                    oFeature.SetField(iOGRFieldIdx, 0, 0, 0, nHour, nMin,
                                      static_cast<float>(nSec), nTZFlag);
                }
                break;
            }

            default:
                CPLAssert(false);
        }
    }

    // Set multi-valued fields.
    for (const auto &[iOGRFieldIdx, aosStrings] : stringListAttrs)
        oFeature.SetField(static_cast<int>(iOGRFieldIdx), aosStrings.List());

    for (const auto &[iOGRFieldIdx, anVals] : intListAttrs)
        oFeature.SetField(static_cast<int>(iOGRFieldIdx),
                          static_cast<int>(anVals.size()), anVals.data());

    for (const auto &[iOGRFieldIdx, adfVals] : doubleListAttrs)
        oFeature.SetField(static_cast<int>(iOGRFieldIdx),
                          static_cast<int>(adfVals.size()), adfVals.data());

    return true;
}

/************************************************************************/
/*                               AttrNode                               */
/************************************************************************/

namespace
{
using AttrCode = OGRS101Reader::AttrCode;
using AttrRepeat = OGRS101Reader::AttrRepeat;
using AttrIndex = OGRS101Reader::AttrIndex;

struct AttrNode
{
    AttrCode code = 0;
    AttrRepeat indexOfSameCode = 0;
    std::string value{};
    std::vector<std::shared_ptr<AttrNode>> children{};

    std::string Encode() const
    {
        std::string s;
        AttrIndex thisIdx = 0;
        AttrIndex curIdx = 0;
        for (const auto &child : children)
        {
            child->Encode(s, thisIdx, curIdx, "");
        }
        return s;
    }

  private:
    void Encode(std::string &s, AttrIndex parentIdx, AttrIndex &curIdx,
                const std::string &indent);
};

void AttrNode::Encode(std::string &s, AttrIndex parentIdx, AttrIndex &curIdx,
                      const std::string &indent)
{
    ++curIdx;
    if constexpr (false)
    {
        const int idx = static_cast<int>(curIdx);
        CPLDebug("S101", "%s[%d].code = %d", indent.c_str(), idx,
                 static_cast<int>(code));
        CPLDebug("S101", "%s[%d].indexOfSameCode = %d\n", indent.c_str(), idx,
                 static_cast<int>(indexOfSameCode));
        CPLDebug("S101", "%s[%d].parentIdx = %d", indent.c_str(), idx,
                 static_cast<int>(parentIdx));
        CPLDebug("S101", "%s[%d].value = %s", indent.c_str(), idx,
                 value.c_str());
    }
    OGRS101Reader::AppendUInt16(s,
                                static_cast<uint16_t>(static_cast<int>(code)));
    OGRS101Reader::AppendUInt16(
        s, static_cast<uint16_t>(static_cast<int>(indexOfSameCode)));
    OGRS101Reader::AppendUInt16(
        s, static_cast<uint16_t>(static_cast<int>(parentIdx)));
    OGRS101Reader::AppendUInt8(s, static_cast<uint8_t>(INSTRUCTION_INSERT));
    s.append(value);
    OGRS101Reader::AppendUInt8(s, static_cast<uint8_t>(DDF_UNIT_TERMINATOR));

    if (!children.empty())
    {
        const auto thisIdx = curIdx;
        const std::string newIndent = indent + "  ";
        for (const auto &child : children)
        {
            child->Encode(s, thisIdx, curIdx, newIndent);
        }
    }
}

}  // namespace

/************************************************************************/
/*                  ProcessUpdateAttributeLikeField()                   */
/************************************************************************/

/** Implement update of ATTR/INAS/FASC field described in 10a-5.1.2
 * "Updating of the Attribute field"
 */
bool OGRS101Reader::ProcessUpdateAttributeLikeField(
    const DDFRecord *poUpdateRecord, const DDFField *poUpdateField,
    DDFRecord *poTargetRecord, DDFField *poTargetField,
    int iFieldInstance) const
{
    const char *pszAttrFieldName = poUpdateField->GetFieldDefn()->GetName();
    CPLAssert(EQUAL(pszAttrFieldName, ATTR_FIELD) ||
              EQUAL(pszAttrFieldName, INAS_FIELD) ||
              EQUAL(pszAttrFieldName, FASC_FIELD));

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

    AttrNode root;

    auto poActualTargetField = poTargetField->GetParts().size() == 2
                                   ? poTargetField->GetParts()[1].get()
                                   : poTargetField;
    const int nTargetRepeatCount = poActualTargetField->GetRepeatCount();

    const auto poActualUpdateField = poUpdateField->GetParts().size() == 2
                                         ? poUpdateField->GetParts()[1].get()
                                         : poUpdateField;
    const int nUpdateRepeatCount = poActualUpdateField->GetRepeatCount();

    constexpr int PASS_TARGET = 0;
    constexpr int PASS_UPDATE = 1;
    for (int iPass = PASS_TARGET; iPass <= PASS_UPDATE; ++iPass)
    {
        const auto poCurRecord =
            (iPass == PASS_TARGET) ? poTargetRecord : poUpdateRecord;
        const auto poCurField =
            (iPass == PASS_TARGET) ? poTargetField : poUpdateField;
        const int nRepeatCount =
            (iPass == PASS_TARGET) ? nTargetRepeatCount : nUpdateRepeatCount;

        // mapAttributeIndexToNode and oSetNATC_ATIX_PAIX do need to be reset
        // at each pass
        std::map<AttrIndex, std::weak_ptr<AttrNode>> mapAttributeIndexToNode;
        std::set<std::tuple<AttrCode, AttrRepeat, AttrIndex>>
            oSetNATC_ATIX_PAIX;

        for (int i = 0; i < nRepeatCount; ++i)
        {
            const AttrIndex curIdx = i + 1;
            const int nInstruction =
                poCurRecord->GetIntSubfield(poCurField, ATIN_SUBFIELD, i);
            if (nInstruction != INSTRUCTION_INSERT &&
                nInstruction != INSTRUCTION_UPDATE &&
                nInstruction != INSTRUCTION_DELETE)
            {
                return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s, RCNM=%d, RCID=%d, %s field, instance %d, entry %d: "
                    "invalid ATIN=%d",
                    m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                    pszAttrFieldName, iFieldInstance, i, nInstruction));
            }

            auto poNode = std::make_shared<AttrNode>();
            poNode->code =
                poCurRecord->GetIntSubfield(poCurField, NATC_SUBFIELD, i);
            poNode->indexOfSameCode =
                poCurRecord->GetIntSubfield(poCurField, ATIX_SUBFIELD, i);
            const AttrIndex parentIndex =
                poCurRecord->GetIntSubfield(poCurField, PAIX_SUBFIELD, i);

            // (NATC,ATIX,PAIX) tuple should be unique within a record
            if (!oSetNATC_ATIX_PAIX
                     .insert(
                         {poNode->code, poNode->indexOfSameCode, parentIndex})
                     .second)
            {
                return EMIT_ERROR_OR_WARNING(
                    CPLSPrintf("%s, RCNM=%d, RCID=%d, %s field, instance %d: "
                               "entry %d refers to (NATC,ATIX,PAIX)=(%d,%d,%d) "
                               "already encountered.",
                               m_osFilename.c_str(), static_cast<int>(nRCNM),
                               nRCID, pszAttrFieldName, iFieldInstance, i,
                               static_cast<int>(poNode->code),
                               static_cast<int>(poNode->indexOfSameCode),
                               static_cast<int>(parentIndex)));
            }

            const char *pszATVL =
                poCurRecord->GetStringSubfield(poCurField, ATVL_SUBFIELD, i);
            if (pszATVL)
                poNode->value = pszATVL;

            // Find the parent node (which is the root node if no parent)
            std::shared_ptr<AttrNode> poParentNodeSharedPtr;
            AttrNode *poParentNode = &root;
            if (parentIndex > 0)
            {
                auto oIter = mapAttributeIndexToNode.find(parentIndex);
                if (oIter == mapAttributeIndexToNode.end())
                {
                    return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s, RCNM=%d, RCID=%d, %s field, instance %d: entry %d "
                        "refers to a PAIX=%d that does not exist",
                        m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                        pszAttrFieldName, iFieldInstance, i,
                        static_cast<int>(parentIndex)));
                }
                poParentNodeSharedPtr = oIter->second.lock();
                if (!poParentNodeSharedPtr)
                {
                    // I don't think that can happen given the
                    // (NATC,ATIX,PAIX) unicity check
                    return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s, RCNM=%d, RCID=%d, %s field, instance %d: entry %d "
                        "refers to a PAIX=%d that has been deleted",
                        m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                        pszAttrFieldName, iFieldInstance, i,
                        static_cast<int>(parentIndex)));
                }
                poParentNode = poParentNodeSharedPtr.get();
            }

            if (nInstruction == INSTRUCTION_INSERT)
            {
                // Find a sibling of same code and whose index is just one before
                // the one to insert.
                auto iterInsertion = poParentNode->children.begin();
                for (; iterInsertion != poParentNode->children.end();
                     ++iterInsertion)
                {
                    if ((*iterInsertion)->code == poNode->code &&
                        (*iterInsertion)->indexOfSameCode >=
                            poNode->indexOfSameCode)
                    {
                        break;
                    }
                }

                if (iterInsertion != poParentNode->children.end())
                {
                    if (poCurRecord == poTargetRecord)
                    {
                        const auto iterNext = std::next(iterInsertion);
                        if (iterNext != poParentNode->children.end() &&
                            (*iterNext)->code == poNode->code &&
                            (*iterNext)->indexOfSameCode ==
                                poNode->indexOfSameCode)
                        {
                            return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                                "%s, RCNM=%d, RCID=%d, %s field, instance %d: "
                                "entry %d collides with another entry of same "
                                "(NATC, ATIX)=(%d,%d)",
                                m_osFilename.c_str(), static_cast<int>(nRCNM),
                                nRCID, pszAttrFieldName, iFieldInstance, i,
                                static_cast<int>(poNode->code),
                                static_cast<int>(poNode->indexOfSameCode)));
                        }
                    }
                    else
                    {
                        // Renumber indexOfSameCode of children right to the inserted one
                        for (auto iterChild = iterInsertion;
                             iterChild != poParentNode->children.end();
                             ++iterChild)
                        {
                            if ((*iterChild)->code == poNode->code &&
                                (*iterChild)->indexOfSameCode >=
                                    poNode->indexOfSameCode)
                            {
                                ++((*iterChild)->indexOfSameCode);
                            }
                        }
                    }
                }

                mapAttributeIndexToNode[curIdx] = poNode;

                poParentNode->children.insert(iterInsertion, poNode);
            }
            else
            {
                // Identify child with desired (code, indexOfSameCode)
                auto iterChild = poParentNode->children.begin();
                for (; iterChild != poParentNode->children.end(); ++iterChild)
                {
                    if ((*iterChild)->code == poNode->code &&
                        (*iterChild)->indexOfSameCode ==
                            poNode->indexOfSameCode)
                    {
                        break;
                    }
                }
                if (iterChild == poParentNode->children.end())
                {
                    return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                        "%s, RCNM=%d, RCID=%d, %s field, instance %d: entry %d "
                        "references unexisting entry (NATC, ATIX)=(%d,%d)",
                        m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                        pszAttrFieldName, iFieldInstance, i,
                        static_cast<int>(poNode->code),
                        static_cast<int>(poNode->indexOfSameCode)));
                }

                if (nInstruction == INSTRUCTION_DELETE)
                {
                    poParentNode->children.erase(iterChild);
                    // No need to explicitly modify mapAttributeIndexToNode
                    // As it contains weak pointers, if the removed node or
                    // one of its children was in the map, the weak pointer
                    // will be invalidated.
                }
                else
                {
                    const auto &poModifiedNode = *iterChild;
                    mapAttributeIndexToNode[curIdx] = poModifiedNode;
                    poModifiedNode->value = poNode->value;
                }
            }
        }
    }

    std::string s;
    if (EQUAL(pszAttrFieldName, INAS_FIELD) ||
        EQUAL(pszAttrFieldName, FASC_FIELD))
    {
        constexpr int SIZE_OF_NON_REPEATED_FIELDS = 1 + 4 + 2 + 2 + 1;
        if (poUpdateField->GetDataSize() < SIZE_OF_NON_REPEATED_FIELDS)
        {
            // Should probably not occur given earlier checks, but...
            return EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s, RCNM=%d, RCID=%d, %s field, instance %d: "
                           "invalid update field",
                           m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                           pszAttrFieldName, iFieldInstance));
        }
        s.append(poUpdateField->GetData(), SIZE_OF_NON_REPEATED_FIELDS - 1);
        AppendUInt8(s, INSTRUCTION_INSERT);
    }
    s += root.Encode();
    AppendUInt8(s, DDF_FIELD_TERMINATOR);
    poTargetRecord->SetFieldRaw(poTargetField, s.data(),
                                static_cast<int>(s.size()));

    return true;
}

/************************************************************************/
/*                         ProcessUpdateATTR()                          */
/************************************************************************/

/** Update all instances of ATTR field
 */
bool OGRS101Reader::ProcessUpdateATTR(const DDFRecord *poUpdateRecord,
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

    auto apoUpdateFields = poUpdateRecord->GetFields(ATTR_FIELD);
    if (apoUpdateFields.empty())
        return true;
    auto apoTargetFields = poTargetRecord->GetFields(ATTR_FIELD);
    if (apoTargetFields.size() != apoUpdateFields.size())
    {
        return EMIT_ERROR_OR_WARNING(CPLSPrintf(
            "%s, RCNM=%d, RCID=%d, %s field: target record has %d field "
            "instances, whereas update record has %d",
            m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID, ATTR_FIELD,
            static_cast<int>(apoTargetFields.size()),
            static_cast<int>(apoUpdateFields.size())));
    }

    for (size_t i = 0; i < apoUpdateFields.size(); ++i)
    {
        if (!ProcessUpdateAttributeLikeField(poUpdateRecord, apoUpdateFields[i],
                                             poTargetRecord, apoTargetFields[i],
                                             static_cast<int>(i)))
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                      ProcessUpdateINASOrFASC()                       */
/************************************************************************/

/** Update all instances of INAS or FASC field
 */
bool OGRS101Reader::ProcessUpdateINASOrFASC(const DDFRecord *poUpdateRecord,
                                            DDFRecord *poTargetRecord,
                                            const char *pszFieldName) const
{
    CPLAssert(EQUAL(pszFieldName, INAS_FIELD) ||
              EQUAL(pszFieldName, FASC_FIELD));

    const auto poIDField = poUpdateRecord->GetField(0);
    CPLAssert(poIDField);

    // Record name
    const RecordName nRCNM =
        poUpdateRecord->GetIntSubfield(poIDField, RCNM_SUBFIELD, 0);

    // Record identifier
    const int nRCID =
        poUpdateRecord->GetIntSubfield(poIDField, RCID_SUBFIELD, 0);

    auto apoUpdateFields = poUpdateRecord->GetFields(pszFieldName);
    if (apoUpdateFields.empty())
        return true;

    auto apoTargetFields = poTargetRecord->GetFields(pszFieldName);

    for (int iUpdate = 0; iUpdate < static_cast<int>(apoUpdateFields.size());
         ++iUpdate)
    {
        const auto poUpdateField = apoUpdateFields[iUpdate];
        const int nInstruction = poUpdateRecord->GetIntSubfield(
            poUpdateField,
            EQUAL(pszFieldName, INAS_FIELD) ? IUIN_SUBFIELD : FAUI_SUBFIELD, 0);
        if (nInstruction == INSTRUCTION_INSERT)
        {
            const auto poINASFieldDefn =
                m_oMainModule.FindFieldDefn(pszFieldName);
            if (!poINASFieldDefn)
            {
                return EMIT_ERROR(CPLSPrintf("Cannot find %s field definition",
                                             pszFieldName));
            }
            auto poFieldTarget = poTargetRecord->AddField(poINASFieldDefn);
            CPLAssert(poFieldTarget);

            poTargetRecord->SetFieldRaw(poFieldTarget, poUpdateField->GetData(),
                                        poUpdateField->GetDataSize());
            apoTargetFields.push_back(poFieldTarget);
        }
        else if (nInstruction == INSTRUCTION_UPDATE ||
                 nInstruction == INSTRUCTION_DELETE)
        {
            const RecordName RRNM =
                poUpdateRecord->GetIntSubfield(poUpdateField, RRNM_SUBFIELD, 0);
            const int RRID =
                poUpdateRecord->GetIntSubfield(poUpdateField, RRID_SUBFIELD, 0);

            bool bMatchFound = false;
            for (size_t iTarget = 0; iTarget < apoTargetFields.size();
                 ++iTarget)
            {
                auto poTargetField = apoTargetFields[iTarget];
                const RecordName RRNMTarget = poTargetRecord->GetIntSubfield(
                    poTargetField, RRNM_SUBFIELD, 0);
                const int RRIDTarget = poTargetRecord->GetIntSubfield(
                    poTargetField, RRID_SUBFIELD, 0);
                if (RRNM == RRNMTarget && RRID == RRIDTarget)
                {
                    bMatchFound = true;
                    if (nInstruction == INSTRUCTION_DELETE)
                    {
                        poTargetRecord->DeleteField(poTargetField);
                        apoTargetFields.erase(apoTargetFields.begin() +
                                              iTarget);
                    }
                    else
                    {
                        if (!ProcessUpdateAttributeLikeField(
                                poUpdateRecord, poUpdateField, poTargetRecord,
                                poTargetField, iUpdate))
                        {
                            return false;
                        }
                    }
                    break;
                }
            }
            if (!bMatchFound)
            {
                return EMIT_ERROR_OR_WARNING(CPLSPrintf(
                    "%s, RCNM=%d, RCID=%d, %s field, %d instance: found no "
                    "matching (RRNM,RRID)=(%d,%d) to %s",
                    m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                    pszFieldName, iUpdate, static_cast<int>(RRNM), RRID,
                    nInstruction == INSTRUCTION_UPDATE ? "update" : "delete"));
            }
        }
        else
        {
            return EMIT_ERROR_OR_WARNING(
                CPLSPrintf("%s, RCNM=%d, RCID=%d, %s field, %d instance: "
                           "invalid instruction = %d",
                           m_osFilename.c_str(), static_cast<int>(nRCNM), nRCID,
                           pszFieldName, iUpdate, nInstruction));
        }
    }

    return true;
}
