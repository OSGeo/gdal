/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFFieldDefn class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2026, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "iso8211.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"

#define CPLE_DiscardedFormat 1301

/************************************************************************/
/*                            DDFFieldDefn()                            */
/************************************************************************/

DDFFieldDefn::DDFFieldDefn() = default;

/************************************************************************/
/*                           ~DDFFieldDefn()                            */
/************************************************************************/

DDFFieldDefn::~DDFFieldDefn() = default;

/************************************************************************/
/*                            AddSubfield()                             */
/************************************************************************/

void DDFFieldDefn::AddSubfield(const char *pszName, const char *pszFormat)

{
    auto poSFDefn = std::make_unique<DDFSubfieldDefn>();

    poSFDefn->SetName(pszName);
    poSFDefn->SetFormat(pszFormat);
    AddSubfield(std::move(poSFDefn));
}

/************************************************************************/
/*                            AddSubfield()                             */
/************************************************************************/

void DDFFieldDefn::AddSubfield(std::unique_ptr<DDFSubfieldDefn> poNewSFDefn,
                               bool bDontAddToFormat)

{
    if (bDontAddToFormat)
    {
        apoSubfields.push_back(std::move(poNewSFDefn));
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Add this format to the format list.  We don't bother            */
    /*      aggregating formats here.                                       */
    /* -------------------------------------------------------------------- */
    if (_formatControls.empty())
    {
        _formatControls = "()";
    }

    std::string osNewFormatControls = _formatControls;
    osNewFormatControls.pop_back();
    if (!osNewFormatControls.empty() && osNewFormatControls.back() != '(')
        osNewFormatControls += ',';
    osNewFormatControls += poNewSFDefn->GetFormat();
    osNewFormatControls += ')';

    _formatControls = std::move(osNewFormatControls);

    /* -------------------------------------------------------------------- */
    /*      Add the subfield name to the list.                              */
    /* -------------------------------------------------------------------- */
    if (!_arrayDescr.empty() &&
        (_arrayDescr[0] != '*' || _arrayDescr.size() > 1))
        _arrayDescr += '!';
    _arrayDescr += poNewSFDefn->GetName();

    apoSubfields.push_back(std::move(poNewSFDefn));
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Initialize a new field defn from application input, instead     */
/*      of from an existing file.                                       */
/************************************************************************/

int DDFFieldDefn::Create(const char *pszTagIn, const char *pszFieldName,
                         const char *pszDescription,
                         DDF_data_struct_code eDataStructCode,
                         DDF_data_type_code eDataTypeCode,
                         const char *pszFormat)

{
    CPLAssert(osTag.empty());
    poModule = nullptr;
    osTag = pszTagIn;
    _fieldName = pszFieldName;
    _arrayDescr = pszDescription ? pszDescription : "";

    _data_struct_code = eDataStructCode;
    _data_type_code = eDataTypeCode;

    _formatControls = pszFormat ? pszFormat : "";

    bRepeatingSubfields = (pszDescription != nullptr && *pszDescription == '*');

    if (!_formatControls.empty() && _data_struct_code != dsc_elementary)
    {
        if (!BuildSubfields())
            return false;

        if (apoFieldParts.empty() && !ApplyFormats())
            return false;
    }

    return TRUE;
}

/************************************************************************/
/*                          GenerateDDREntry()                          */
/************************************************************************/

int DDFFieldDefn::GenerateDDREntry(DDFModule *poModuleIn, char **ppachData,
                                   int *pnLength)

{
    const int iFDOffset = poModuleIn->GetFieldControlLength();
    CPLAssert(iFDOffset >= 6 && iFDOffset <= 9);
    *pnLength = static_cast<int>(iFDOffset + _fieldName.size() + 1 +
                                 _arrayDescr.size() + 1);
    if (!_formatControls.empty())
    {
        *pnLength += static_cast<int>(_formatControls.size() + 1);
    }

    if (ppachData == nullptr)
        return TRUE;

    *ppachData = static_cast<char *>(CPLMalloc(*pnLength + 1));
    (*ppachData)[*pnLength] = 0;

    if (_data_struct_code == dsc_elementary)
        (*ppachData)[0] = '0';
    else if (_data_struct_code == dsc_vector)
        (*ppachData)[0] = '1';
    else if (_data_struct_code == dsc_array)
        (*ppachData)[0] = '2';
    else if (_data_struct_code == dsc_concatenated)
        (*ppachData)[0] = '3';

    if (_data_type_code == dtc_char_string)
        (*ppachData)[1] = '0';
    else if (_data_type_code == dtc_implicit_point)
        (*ppachData)[1] = '1';
    else if (_data_type_code == dtc_explicit_point)
        (*ppachData)[1] = '2';
    else if (_data_type_code == dtc_explicit_point_scaled)
        (*ppachData)[1] = '3';
    else if (_data_type_code == dtc_char_bit_string)
        (*ppachData)[1] = '4';
    else if (_data_type_code == dtc_bit_string)
        (*ppachData)[1] = '5';
    else if (_data_type_code == dtc_mixed_data_type)
        (*ppachData)[1] = '6';

    (*ppachData)[2] = '0';
    (*ppachData)[3] = '0';
    (*ppachData)[4] = ';';
    (*ppachData)[5] = '&';
    if (iFDOffset > 6 && _escapeSequence.size() >= 1)
        (*ppachData)[6] = _escapeSequence[0];
    if (iFDOffset > 7 && _escapeSequence.size() >= 2)
        (*ppachData)[7] = _escapeSequence[1];
    if (iFDOffset > 8 && _escapeSequence.size() >= 3)
        (*ppachData)[8] = _escapeSequence[2];
    snprintf(*ppachData + iFDOffset, *pnLength + 1 - iFDOffset, "%s",
             _fieldName.c_str());
    snprintf(*ppachData + strlen(*ppachData),
             *pnLength + 1 - strlen(*ppachData), "%c%s", DDF_UNIT_TERMINATOR,
             _arrayDescr.c_str());
    if (!_formatControls.empty())
    {
        // empty for '0000' of S-57 & S-111
        snprintf(*ppachData + strlen(*ppachData),
                 *pnLength + 1 - strlen(*ppachData), "%c%s",
                 DDF_UNIT_TERMINATOR, _formatControls.c_str());
    }
    snprintf(*ppachData + strlen(*ppachData),
             *pnLength + 1 - strlen(*ppachData), "%c", DDF_FIELD_TERMINATOR);

    return TRUE;
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Initialize the field definition from the information in the     */
/*      DDR record.  This is called by DDFModule::Open().               */
/************************************************************************/

int DDFFieldDefn::Initialize(DDFModule *poModuleIn, const char *pszTagIn,
                             int nFieldEntrySize, const char *pachFieldArea)

{
    int iFDOffset = poModuleIn->GetFieldControlLength();

    poModule = poModuleIn;

    osTag = pszTagIn;

    /* -------------------------------------------------------------------- */
    /*      Set the data struct and type codes.                             */
    /* -------------------------------------------------------------------- */
    switch (pachFieldArea[0])
    {
        case ' ': /* for ADRG, DIGEST USRP, DIGEST ASRP files */
        case '0':
            _data_struct_code = dsc_elementary;
            break;

        case '1':
            _data_struct_code = dsc_vector;
            break;

        case '2':
            _data_struct_code = dsc_array;
            break;

        case '3':
            _data_struct_code = dsc_concatenated;
            break;

        default:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unrecognized data_struct_code value %c.\n"
                     "Field %s initialization incorrect.",
                     pachFieldArea[0], osTag.c_str());
            _data_struct_code = dsc_elementary;
    }

    switch (pachFieldArea[1])
    {
        case ' ': /* for ADRG, DIGEST USRP, DIGEST ASRP files */
        case '0':
            _data_type_code = dtc_char_string;
            break;

        case '1':
            _data_type_code = dtc_implicit_point;
            break;

        case '2':
            _data_type_code = dtc_explicit_point;
            break;

        case '3':
            _data_type_code = dtc_explicit_point_scaled;
            break;

        case '4':
            _data_type_code = dtc_char_bit_string;
            break;

        case '5':
            _data_type_code = dtc_bit_string;
            break;

        case '6':
            _data_type_code = dtc_mixed_data_type;
            break;

        default:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unrecognized data_type_code value %c.\n"
                     "Field %s initialization incorrect.",
                     pachFieldArea[1], osTag.c_str());
            _data_type_code = dtc_char_string;
    }

    if (nFieldEntrySize >= iFDOffset && iFDOffset > 6)
        _escapeSequence.assign(pachFieldArea + 6, iFDOffset - 6);

    /* -------------------------------------------------------------------- */
    /*      Capture the field name, description (sub field names), and      */
    /*      format statements.                                              */
    /* -------------------------------------------------------------------- */

    int nCharsConsumed = 0;
    _fieldName = DDFFetchVariable(
        pachFieldArea + iFDOffset, nFieldEntrySize - iFDOffset,
        DDF_UNIT_TERMINATOR, DDF_FIELD_TERMINATOR, &nCharsConsumed);
    iFDOffset += nCharsConsumed;

    _arrayDescr = DDFFetchVariable(
        pachFieldArea + iFDOffset, nFieldEntrySize - iFDOffset,
        DDF_UNIT_TERMINATOR, DDF_FIELD_TERMINATOR, &nCharsConsumed);
    iFDOffset += nCharsConsumed;

    _formatControls = DDFFetchVariable(
        pachFieldArea + iFDOffset, nFieldEntrySize - iFDOffset,
        DDF_UNIT_TERMINATOR, DDF_FIELD_TERMINATOR, &nCharsConsumed);

    /* -------------------------------------------------------------------- */
    /*      Parse the subfield info.                                        */
    /* -------------------------------------------------------------------- */
    if (_data_struct_code != dsc_elementary)
    {
        if (!BuildSubfields())
            return false;

        if (apoFieldParts.empty() && !ApplyFormats())
            return false;
    }

    return TRUE;
}

/************************************************************************/
/*                         SetEscapeSequence()                          */
/************************************************************************/

void DDFFieldDefn::SetEscapeSequence(const std::string &val)
{
    _escapeSequence = val;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out field definition info to debugging file.
 *
 * A variety of information about this field definition, and all its
 * subfields is written to the give debugging file handle.
 *
 * @param fp The standard IO file handle to write to.  i.e. stderr
 */

void DDFFieldDefn::Dump(FILE *fp, int nNestingLevel) const

{
    std::string osIndent;
    for (int i = 0; i < nNestingLevel; ++i)
        osIndent += "  ";

#define Print(...)                                                             \
    do                                                                         \
    {                                                                          \
        fprintf(fp, "%s", osIndent.c_str());                                   \
        fprintf(fp, __VA_ARGS__);                                              \
    } while (0)

    const char *pszValue = "";
    CPL_IGNORE_RET_VAL(pszValue);  // Make CSA happy

    Print("DDFFieldDefn:\n");
    Print("    Tag = `%s'\n", osTag.c_str());
    Print("    _fieldName = `%s'\n", _fieldName.c_str());
    Print("    _arrayDescr = `%s'\n", _arrayDescr.c_str());
    Print("    _formatControls = `%s'\n", _formatControls.c_str());

    switch (_data_struct_code)
    {
        case dsc_elementary:
            pszValue = "elementary";
            break;

        case dsc_vector:
            pszValue = "vector";
            break;

        case dsc_array:
            pszValue = "array";
            break;

        case dsc_concatenated:
            pszValue = "concatenated";
            break;
    }

    Print("    _data_struct_code = %s\n", pszValue);

    switch (_data_type_code)
    {
        case dtc_char_string:
            pszValue = "char_string";
            break;

        case dtc_implicit_point:
            pszValue = "implicit_point";
            break;

        case dtc_explicit_point:
            pszValue = "explicit_point";
            break;

        case dtc_explicit_point_scaled:
            pszValue = "explicit_point_scaled";
            break;

        case dtc_char_bit_string:
            pszValue = "char_bit_string";
            break;

        case dtc_bit_string:
            pszValue = "bit_string";
            break;

        case dtc_mixed_data_type:
            pszValue = "mixed_data_type";
            break;
    }

    Print("    _data_type_code = %s\n", pszValue);

    for (const auto &poField : apoFieldParts)
        poField->Dump(fp, nNestingLevel + 1);

    for (const auto &poSubfield : apoSubfields)
        poSubfield->Dump(fp, nNestingLevel + 1);
}

/************************************************************************/
/*                           BuildSubfields()                           */
/*                                                                      */
/*      Based on the _arrayDescr build a set of subfields.              */
/************************************************************************/

bool DDFFieldDefn::BuildSubfields()

{
    const char *pszSublist = _arrayDescr.c_str();

    if (_data_struct_code == dsc_concatenated)
    {
        // Split on two consecutive backslashes.
        std::vector<std::string> aosPartDescr;
        {
            std::string osCur;
            for (size_t i = 0; i < _arrayDescr.size(); ++i)
            {
                const char c = _arrayDescr[i];
                if (c == '\\' && _arrayDescr[i + 1] == '\\')
                {
                    aosPartDescr.push_back(osCur);
                    osCur.clear();
                    ++i;
                }
                else
                {
                    osCur += c;
                }
            }
            aosPartDescr.push_back(std::move(osCur));
        }
        if (aosPartDescr.size() > 1 && !_formatControls.empty() &&
            _formatControls.front() == '(' && _formatControls.back() == ')')
        {
            const char *pszFormatCur = _formatControls.c_str() + 1;
            for (size_t i = 0; i < aosPartDescr.size(); ++i)
            {
                const std::string &osPartDescr = aosPartDescr[i];
                // Check there are no repeated subfields but in the last part
                if (i < aosPartDescr.size() - 1 &&
                    osPartDescr.find('*') != std::string::npos)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Tag %s: repeated fields found in a part that is "
                             "not the last one: %s",
                             osTag.c_str(), _arrayDescr.c_str());
                    return false;
                }

                const int nSubfieldsInPart = static_cast<int>(
                    std::count(osPartDescr.begin(), osPartDescr.end(), '!') +
                    1);
                int nSubFieldCounter = 0;
                const char *pszFormatStart = pszFormatCur;
                bool justAfterFieldFormat = true;
                int nParenthesisLevel = 0;
                while (i < aosPartDescr.size() - 1 && *pszFormatCur != '\0')
                {
                    if (justAfterFieldFormat && *pszFormatCur >= '1' &&
                        *pszFormatCur <= '9')
                    {
                        char *pszNext = nullptr;
                        const int nRepeat = static_cast<int>(
                            strtol(pszFormatCur, &pszNext, 10));
                        if (!(*pszNext) || nRepeat <= 0 || nRepeat > 1000)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Tag %s: invalid formatControls: %s",
                                     osTag.c_str(), _formatControls.c_str());
                            return false;
                        }
                        if (*pszNext == '(')
                        {
                            pszFormatCur = pszNext + 1;
                            int nGroupSubFieldCount = 0;
                            while (*pszFormatCur)
                            {
                                if (*pszFormatCur == '(')
                                {
                                    // Implementation limitation. Perhaps OK per the standard
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Tag %s: unsupported "
                                             "formatControls: %s",
                                             osTag.c_str(),
                                             _formatControls.c_str());
                                    return false;
                                }
                                else if (*pszFormatCur >= '1' &&
                                         *pszFormatCur <= '9')
                                {
                                    nGroupSubFieldCount += static_cast<int>(
                                        strtol(pszFormatCur, &pszNext, 10));
                                    if (!(*pszNext))
                                    {
                                        CPLError(CE_Failure, CPLE_AppDefined,
                                                 "Tag %s: invalid "
                                                 "formatControls: %s",
                                                 osTag.c_str(),
                                                 _formatControls.c_str());
                                        return false;
                                    }
                                    pszFormatCur = pszNext;
                                }
                                else if (*pszFormatCur == ',')
                                {
                                    nGroupSubFieldCount++;
                                    ++pszFormatCur;
                                }
                                else if (*pszFormatCur == ')')
                                {
                                    nGroupSubFieldCount++;
                                    break;
                                }
                                else
                                {
                                    ++pszFormatCur;
                                }
                            }
                            if (!*pszFormatCur)
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Tag %s: invalid formatControls: %s",
                                         osTag.c_str(),
                                         _formatControls.c_str());
                                return false;
                            }
                            ++pszFormatCur;
                            if (nGroupSubFieldCount < 0 ||
                                nGroupSubFieldCount > INT_MAX / nRepeat)
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Tag %s: invalid "
                                         "formatControls: %s",
                                         osTag.c_str(),
                                         _formatControls.c_str());
                                return false;
                            }
                            nSubFieldCounter += nGroupSubFieldCount * nRepeat;
                            if (*pszFormatCur == ')')
                                break;
                            if (*pszFormatCur != ',')
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Tag %s: invalid formatControls: %s",
                                         osTag.c_str(),
                                         _formatControls.c_str());
                                return false;
                            }
                            ++pszFormatCur;
                        }
                        else
                        {
                            nSubFieldCounter += nRepeat;
                            pszFormatCur = pszNext;
                            while (*pszFormatCur && *pszFormatCur != ',' &&
                                   *pszFormatCur != ')')
                            {
                                ++pszFormatCur;
                            }
                            if (*pszFormatCur == ')')
                                break;
                            if (*pszFormatCur != ',')
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Tag %s: invalid formatControls: %s",
                                         osTag.c_str(),
                                         _formatControls.c_str());
                                return false;
                            }
                            ++pszFormatCur;
                        }
                        justAfterFieldFormat = true;
                    }
                    else if (*pszFormatCur == ',')
                    {
                        ++nSubFieldCounter;
                        ++pszFormatCur;
                        justAfterFieldFormat = true;
                    }
                    else if (*pszFormatCur == '(')
                    {
                        ++nParenthesisLevel;
                        ++pszFormatCur;
                        justAfterFieldFormat = false;
                    }
                    else if (*pszFormatCur == ')')
                    {
                        if (--nParenthesisLevel < 0)
                        {
                            ++nSubFieldCounter;
                            break;
                        }
                        ++pszFormatCur;
                        justAfterFieldFormat = false;
                    }
                    else
                    {
                        ++pszFormatCur;
                        justAfterFieldFormat = false;
                    }

                    if (nSubFieldCounter > nSubfieldsInPart)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Tag %s: mismatch between arrayDescr:%s and "
                                 "formatControls: %s",
                                 osTag.c_str(), _arrayDescr.c_str(),
                                 _formatControls.c_str());
                        return false;
                    }
                    else if (nSubFieldCounter == nSubfieldsInPart)
                    {
                        break;
                    }
                }
                if (i < aosPartDescr.size() - 1)
                {
                    if (*pszFormatCur == 0 || pszFormatCur[-1] != ',' ||
                        nParenthesisLevel > 0)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Tag %s: invalid formatControls: %s",
                                 osTag.c_str(), _formatControls.c_str());
                        return false;
                    }
                    if (nSubFieldCounter != nSubfieldsInPart)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Tag %s: mismatch between arrayDescr:%s and "
                                 "formatControls: %s",
                                 osTag.c_str(), _arrayDescr.c_str(),
                                 _formatControls.c_str());
                        return false;
                    }
                }

                std::string osPartFormatControls;
                if (i < aosPartDescr.size() - 1)
                {
                    if (pszFormatCur == pszFormatStart)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Tag %s: mismatch between arrayDescr:%s and "
                                 "formatControls or invalid formatControls: %s",
                                 osTag.c_str(), _arrayDescr.c_str(),
                                 _formatControls.c_str());
                        return false;
                    }
                    osPartFormatControls = '(';
                    osPartFormatControls.append(
                        pszFormatStart, pszFormatCur - pszFormatStart - 1);
                    osPartFormatControls += ')';
                }
                else
                {
                    if (*pszFormatStart == '(' && _formatControls.size() > 2 &&
                        _formatControls[_formatControls.size() - 2] == ')')
                    {
                        // S101 2.0
                        osPartFormatControls.append(pszFormatStart,
                                                    strlen(pszFormatStart) - 1);
                    }
                    else
                    {
                        // Earlier versions
                        osPartFormatControls = '(';
                        osPartFormatControls += pszFormatStart;
                    }
                }

                auto poPartFieldDefn = std::make_unique<DDFFieldDefn>();
                poPartFieldDefn->poModule = poModule;
                // poPartFieldDefn->osTag: not set on purpose
                // poPartFieldDefn->_fieldName: not set on purpose
                poPartFieldDefn->_arrayDescr = osPartDescr;
                poPartFieldDefn->_formatControls =
                    std::move(osPartFormatControls);
                poPartFieldDefn->_data_struct_code =
                    dsc_vector;  // not necessarily exact, but good enough
                poPartFieldDefn->_data_type_code = _data_type_code;
                poPartFieldDefn->bRepeatingSubfields =
                    !osPartDescr.empty() && osPartDescr.front() == '*';

                if (!poPartFieldDefn->BuildSubfields() ||
                    !poPartFieldDefn->ApplyFormats())
                    return false;

                apoFieldParts.push_back(std::move(poPartFieldDefn));
            }

            return true;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      It is valid to define a field with _arrayDesc                   */
    /*      '*STPT!CTPT!ENPT*YCOO!XCOO' and formatControls '(2b24)'.        */
    /*      This basically indicates that there are 3 (YCOO,XCOO)           */
    /*      structures named STPT, CTPT and ENPT.  But we can't handle      */
    /*      such a case gracefully here, so we just ignore the              */
    /*      "structure names" and treat such a thing as a repeating         */
    /*      YCOO/XCOO array.  This occurs with the AR2D field of some       */
    /*      AML S-57 files for instance.                                    */
    /*                                                                      */
    /*      We accomplish this by ignoring everything before the last       */
    /*      '*' in the subfield list.                                       */
    /* -------------------------------------------------------------------- */
    if (strrchr(pszSublist, '*') != nullptr)
        pszSublist = strrchr(pszSublist, '*');

    /* -------------------------------------------------------------------- */
    /*      Strip off the repeating marker, when it occurs, but mark our    */
    /*      field as repeating.                                             */
    /* -------------------------------------------------------------------- */
    if (pszSublist[0] == '*')
    {
        bRepeatingSubfields = TRUE;
        pszSublist++;
    }

    /* -------------------------------------------------------------------- */
    /*      split list of fields .                                          */
    /* -------------------------------------------------------------------- */
    const CPLStringList aosSubfieldNames(
        CSLTokenizeStringComplex(pszSublist, "!", FALSE, FALSE));

    /* -------------------------------------------------------------------- */
    /*      minimally initialize the subfields.  More will be done later.   */
    /* -------------------------------------------------------------------- */
    for (const char *pszSubfieldName : cpl::Iterate(aosSubfieldNames))
    {
        auto poSFDefn = std::make_unique<DDFSubfieldDefn>();

        poSFDefn->SetName(pszSubfieldName);
        AddSubfield(std::move(poSFDefn), true);
    }

    return true;
}

/************************************************************************/
/*                          ExtractSubstring()                          */
/*                                                                      */
/*      Extract a substring terminated by a comma (or end of            */
/*      string).  Commas in brackets are ignored as terminated with     */
/*      bracket nesting understood gracefully.  If the returned         */
/*      string would begin and end with a bracket then strip off the    */
/*      brackets.                                                       */
/*                                                                      */
/*      Given a string like "(A,3(B,C),D),X,Y)" return "A,3(B,C),D".    */
/*      Giveh a string like "3A,2C" return "3A".                        */
/*      Giveh a string like "(3A,2C" return an empty string             */
/*      Giveh a string like "3A),2C" return an empty string             */
/************************************************************************/

std::string DDFFieldDefn::ExtractSubstring(const char *pszSrc)

{
    int nBracket = 0;
    int i = 0;  // Used after for.
    for (; pszSrc[i] != '\0' && (nBracket > 0 || pszSrc[i] != ','); i++)
    {
        if (pszSrc[i] == '(')
            nBracket++;
        else if (pszSrc[i] == ')')
        {
            nBracket--;
            if (nBracket < 0)
                return std::string();
        }
    }
    if (nBracket > 0)
        return std::string();

    if (pszSrc[0] == '(')
    {
        CPLAssert(i >= 2);
        return std::string(pszSrc + 1, i - 2);
    }
    else
    {
        return std::string(pszSrc, i);
    }
}

/************************************************************************/
/*                            ExpandFormat()                            */
/************************************************************************/

std::string DDFFieldDefn::ExpandFormat(const char *pszSrc)

{
    std::string osDest;
    size_t iSrc = 0;

    while (pszSrc[iSrc] != '\0')
    {
        // This is presumably an extra level of brackets around some
        // binary stuff related to rescanning which we don't care to do
        // (see 6.4.3.3 of the standard.  We just strip off the extra
        // layer of brackets.
        if ((iSrc == 0 || pszSrc[iSrc - 1] == ',') && pszSrc[iSrc] == '(')
        {
            const std::string osContents = ExtractSubstring(pszSrc + iSrc);
            if (osContents.empty())
            {
                return std::string();
            }
            const std::string osExpandedContents =
                ExpandFormat(osContents.c_str());
            if (osExpandedContents.empty())
            {
                return std::string();
            }

            if (osDest.size() + osExpandedContents.size() > 1024 * 1024)
            {
                return std::string();
            }

            osDest += osExpandedContents;

            iSrc += osContents.size() + 2;
        }

        // This is a repeated subclause.
        else if ((iSrc == 0 || pszSrc[iSrc - 1] == ',') &&
                 isdigit(static_cast<unsigned char>(pszSrc[iSrc])))
        {
            const int nRepeat = atoi(pszSrc + iSrc);
            // 100: arbitrary number. Higher values might cause performance
            // problems in the below loop
            if (nRepeat < 0 || nRepeat > 100)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too large repeat count: %d", nRepeat);
                return std::string();
            }

            // Skip over repeat count.
            const char *pszNext = pszSrc + iSrc;  // Used after for.
            for (; isdigit(static_cast<unsigned char>(*pszNext)); pszNext++)
                iSrc++;

            const std::string osContents = ExtractSubstring(pszNext);
            if (osContents.empty())
            {
                return std::string();
            }
            const std::string osExpandedContents =
                ExpandFormat(osContents.c_str());
            if (osExpandedContents.empty())
            {
                return std::string();
            }

            const size_t nExpandedContentsLen = osExpandedContents.size();
            if (osDest.size() + nExpandedContentsLen * nRepeat > 1024 * 1024)
            {
                return std::string();
            }

            for (int i = 0; i < nRepeat; i++)
            {
                if (i > 0)
                    osDest += ',';
                osDest += osExpandedContents;
            }

            if (pszNext[0] == '(')
                iSrc += osContents.size() + 2;
            else
                iSrc += osContents.size();
        }
        else
        {
            osDest += pszSrc[iSrc++];
        }
    }

    return osDest;
}

/************************************************************************/
/*                            ApplyFormats()                            */
/*                                                                      */
/*      This method parses the format string partially, and then        */
/*      applies a subfield format string to each subfield object.       */
/*      It in turn does final parsing of the subfield formats.          */
/************************************************************************/

int DDFFieldDefn::ApplyFormats()

{
    /* -------------------------------------------------------------------- */
    /*      Verify that the format string is contained within brackets.     */
    /* -------------------------------------------------------------------- */
    if (_formatControls.size() < 2 || _formatControls[0] != '(' ||
        _formatControls.back() != ')')
    {
        CPLError(CE_Warning, static_cast<CPLErrorNum>(CPLE_DiscardedFormat),
                 "Format controls for `%s' field missing brackets:%s",
                 osTag.c_str(), _formatControls.c_str());

        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Duplicate the string, and strip off the brackets.               */
    /* -------------------------------------------------------------------- */

    const std::string osFormatList = ExpandFormat(_formatControls.c_str());
    if (osFormatList.empty())
    {
        CPLError(CE_Warning, static_cast<CPLErrorNum>(CPLE_DiscardedFormat),
                 "Invalid format controls for `%s': %s", osTag.c_str(),
                 _formatControls.c_str());
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Tokenize based on commas.                                       */
    /* -------------------------------------------------------------------- */
    const CPLStringList aosFormatItems(
        CSLTokenizeStringComplex(osFormatList.c_str(), ",", FALSE, FALSE));

    /* -------------------------------------------------------------------- */
    /*      Apply the format items to subfields.                            */
    /* -------------------------------------------------------------------- */
    int iFormatItem = 0;  // Used after for.

    for (; iFormatItem < aosFormatItems.size(); iFormatItem++)
    {
        const char *pszPastPrefix = aosFormatItems[iFormatItem];
        while (*pszPastPrefix >= '0' && *pszPastPrefix <= '9')
            pszPastPrefix++;

        ///////////////////////////////////////////////////////////////
        // Did we get too many formats for the subfields created
        // by names?  This may be legal by the 8211 specification, but
        // isn't encountered in any formats we care about so we just
        // blow.

        if (iFormatItem >= GetSubfieldCount())
        {
            CPLError(CE_Warning, static_cast<CPLErrorNum>(CPLE_DiscardedFormat),
                     "Got more formats than subfields for field `%s'.",
                     osTag.c_str());
            break;
        }

        if (!apoSubfields[iFormatItem]->SetFormat(pszPastPrefix))
        {
            return FALSE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Verify that we got enough formats, cleanup and return.          */
    /* -------------------------------------------------------------------- */

    if (iFormatItem < GetSubfieldCount())
    {
        CPLError(CE_Warning, static_cast<CPLErrorNum>(CPLE_DiscardedFormat),
                 "Got less formats than subfields for field `%s'.",
                 osTag.c_str());
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      If all the fields are fixed width, then we are fixed width      */
    /*      too.  This is important for repeating fields.                   */
    /* -------------------------------------------------------------------- */
    nFixedWidth = 0;
    for (auto &poSubfield : apoSubfields)
    {
        if (poSubfield->GetWidth() == 0)
        {
            nFixedWidth = 0;
            break;
        }
        else
        {
            if (nFixedWidth > INT_MAX - poSubfield->GetWidth())
            {
                CPLError(CE_Warning,
                         static_cast<CPLErrorNum>(CPLE_DiscardedFormat),
                         "Invalid format controls for `%s': %s", osTag.c_str(),
                         _formatControls.c_str());
                return FALSE;
            }
            nFixedWidth += poSubfield->GetWidth();
        }
    }

    return TRUE;
}

/************************************************************************/
/*                          FindSubfieldDefn()                          */
/************************************************************************/

/**
 * Find a subfield definition by its mnemonic tag.
 *
 * @param pszMnemonic The name of the field.
 *
 * @return The subfield pointer, or NULL if there isn't any such subfield.
 */

const DDFSubfieldDefn *
DDFFieldDefn::FindSubfieldDefn(const char *pszMnemonic) const

{
    for (const auto &poSubfield : apoSubfields)
    {
        if (EQUAL(poSubfield->GetName(), pszMnemonic))
            return poSubfield.get();
    }

    return nullptr;
}

/************************************************************************/
/*                          GetDefaultValue()                           */
/************************************************************************/

/**
 * Return default data for field instance.
 */

char *DDFFieldDefn::GetDefaultValue(int *pnSize) const

{
    if (!apoFieldParts.empty())
    {
        std::string osData;
        for (auto &poPartFieldDefn : apoFieldParts)
        {
            if (poPartFieldDefn->IsRepeating())  // only last part
                break;
            int nPartSize = 0;
            char *pszVal = poPartFieldDefn->GetDefaultValue(&nPartSize);
            if (!pszVal)
                return nullptr;
            osData.append(pszVal, nPartSize);
            CPLFree(pszVal);
        }
        if (pnSize)
            *pnSize = static_cast<int>(osData.size());
        char *pabyRet = static_cast<char *>(CPLMalloc(osData.size()));
        memcpy(pabyRet, osData.data(), osData.size());
        return pabyRet;
    }

    /* -------------------------------------------------------------------- */
    /*      Loop once collecting the sum of the subfield lengths.           */
    /* -------------------------------------------------------------------- */
    int nTotalSize = 0;
    for (auto &poSubfield : apoSubfields)
    {
        int nSubfieldSize = 0;

        if (!poSubfield->GetDefaultValue(nullptr, 0, &nSubfieldSize))
            return nullptr;
        nTotalSize += nSubfieldSize;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate buffer.                                                */
    /* -------------------------------------------------------------------- */
    char *pachData = static_cast<char *>(CPLMalloc(nTotalSize));

    if (pnSize != nullptr)
        *pnSize = nTotalSize;

    /* -------------------------------------------------------------------- */
    /*      Loop again, collecting actual default values.                   */
    /* -------------------------------------------------------------------- */
    int nOffset = 0;
    for (auto &poSubfield : apoSubfields)
    {
        int nSubfieldSize;

        if (!poSubfield->GetDefaultValue(pachData + nOffset,
                                         nTotalSize - nOffset, &nSubfieldSize))
        {
            CPLAssert(false);
            return nullptr;
        }

        nOffset += nSubfieldSize;
    }

    CPLAssert(nOffset == nTotalSize);

    return pachData;
}
