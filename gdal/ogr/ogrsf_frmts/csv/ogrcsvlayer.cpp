/******************************************************************************
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVLayer class.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_csv.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

#define DIGIT_ZERO '0'

CPL_CVSID("$Id$")

/************************************************************************/
/*                            CSVSplitLine()                            */
/*                                                                      */
/*      Tokenize a CSV line into fields in the form of a string         */
/*      list.  This is used instead of the CPLTokenizeString()          */
/*      because it provides correct CSV escaping and quoting            */
/*      semantics.                                                      */
/************************************************************************/

static char **CSVSplitLine( const char *pszString, char chDelimiter,
                            bool bKeepLeadingAndClosingQuotes,
                            bool bMergeDelimiter )

{
    CPLStringList aosRetList;

    char *pszToken = static_cast<char *>(CPLCalloc(10, 1));
    int nTokenMax = 10;

    while( pszString != nullptr && *pszString != '\0' )
    {
        bool bInString = false;

        int nTokenLen = 0;

        // Try to find the next delimiter, marking end of token.
        for( ; *pszString != '\0'; pszString++ )
        {
            // End if this is a delimiter skip it and break.
            if( !bInString && *pszString == chDelimiter )
            {
                pszString++;
                if( bMergeDelimiter )
                {
                    while( *pszString == chDelimiter )
                        pszString++;
                }
                break;
            }

            if( *pszString == '"' )
            {
                if( !bInString || pszString[1] != '"' )
                {
                    bInString = !bInString;
                    if( !bKeepLeadingAndClosingQuotes )
                        continue;
                }
                else  // Doubled quotes in string resolve to one quote.
                {
                    pszString++;
                }
            }

            if( nTokenLen >= nTokenMax - 2 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = static_cast<char *>(CPLRealloc(pszToken, nTokenMax));
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';
        aosRetList.AddString(pszToken);

        // If the last token is an empty token, then we have to catch
        // it now, otherwise we won't reenter the loop and it will be lost.
        if( *pszString == '\0' && *(pszString - 1) == chDelimiter )
        {
            aosRetList.AddString("");
        }
    }

    CPLFree(pszToken);

    if( aosRetList.Count() == 0 )
        return static_cast<char **>(CPLCalloc(sizeof(char *), 1));
    else
        return aosRetList.StealList();
}

/************************************************************************/
/*                      OGRCSVReadParseLineL()                          */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/************************************************************************/

char **OGRCSVReadParseLineL( VSILFILE *fp, char chDelimiter,
                             bool bDontHonourStrings,
                             bool bKeepLeadingAndClosingQuotes,
                             bool bMergeDelimiter )

{
    const char *pszLine = CPLReadLineL(fp);
    if( pszLine == nullptr )
        return nullptr;

    // Skip BOM.
    const GByte *pabyData = reinterpret_cast<const GByte *>(pszLine);
    if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
        pszLine += 3;

    // Special fix to read NdfcFacilities.xls with un-balanced double quotes.
    if( chDelimiter == '\t' && bDontHonourStrings )
    {
        return CSLTokenizeStringComplex(pszLine, "\t", FALSE, TRUE);
    }

    // If there are no quotes, then this is the simple case.
    // Parse, and return tokens.
    if( strchr(pszLine,'\"') == nullptr )
        return CSVSplitLine(pszLine, chDelimiter, bKeepLeadingAndClosingQuotes,
                            bMergeDelimiter);

    // We must now count the quotes in our working string, and as
    // long as it is odd, keep adding new lines.
    char *pszWorkLine = CPLStrdup(pszLine);

    int i = 0;
    int nCount = 0;
    size_t nWorkLineLength = strlen(pszWorkLine);

    while( true )
    {
        for( ; pszWorkLine[i] != '\0'; i++ )
        {
            if( pszWorkLine[i] == '\"' )
                nCount++;
        }

        if( nCount % 2 == 0 )
            break;

        pszLine = CPLReadLineL(fp);
        if( pszLine == nullptr )
            break;

        const size_t nLineLen = strlen(pszLine);

        char *pszWorkLineTmp = static_cast<char *>(
            VSI_REALLOC_VERBOSE(pszWorkLine, nWorkLineLength + nLineLen + 2));
        if( pszWorkLineTmp == nullptr )
            break;
        pszWorkLine = pszWorkLineTmp;

        // The '\n' gets lost in CPLReadLine().
        strcat(pszWorkLine + nWorkLineLength, "\n");
        strcat(pszWorkLine + nWorkLineLength, pszLine);

        nWorkLineLength += nLineLen + 1;
    }

    char **papszReturn =
        CSVSplitLine(pszWorkLine, chDelimiter, bKeepLeadingAndClosingQuotes,
                     bMergeDelimiter);

    CPLFree(pszWorkLine);

    return papszReturn;
}

/************************************************************************/
/*                            OGRCSVLayer()                             */
/*                                                                      */
/*      Note that the OGRCSVLayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/

OGRCSVLayer::OGRCSVLayer( const char *pszLayerNameIn,
                          VSILFILE  *fp, const char *pszFilenameIn,
                          int bNewIn, int bInWriteModeIn,
                          char chDelimiterIn ) :
    poFeatureDefn(nullptr),
    fpCSV(fp),
    nNextFID(1),
    bHasFieldNames(false),
    bNew(CPL_TO_BOOL(bNewIn)),
    bInWriteMode(CPL_TO_BOOL(bInWriteModeIn)),
    bUseCRLF(false),
    bNeedRewindBeforeRead(false),
    eGeometryFormat(OGR_CSV_GEOM_NONE),
    pszFilename(CPLStrdup(pszFilenameIn)),
    bCreateCSVT(false),
    bWriteBOM(false),
    chDelimiter(chDelimiterIn),
    nCSVFieldCount(0),
    panGeomFieldIndex(nullptr),
    bFirstFeatureAppendedDuringSession(true),
    bHiddenWKTColumn(false),
    iNfdcLongitudeS(-1),
    iNfdcLatitudeS(-1),
    bDontHonourStrings(false),
    iLongitudeField(-1),
    iLatitudeField(-1),
    iZField(-1),
    bIsEurostatTSV(false),
    nEurostatDims(0),
    nTotalFeatures(bNewIn ? 0 : -1),
    bWarningBadTypeOrWidth(false),
    bKeepSourceColumns(false),
    bKeepGeomColumns(true),
    bMergeDelimiter(false),
    bEmptyStringNull(false)
{
    poFeatureDefn = new OGRFeatureDefn(pszLayerNameIn);
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
}

/************************************************************************/
/*                             Matches()                                */
/************************************************************************/

bool OGRCSVLayer::Matches( const char *pszFieldName, char **papszPossibleNames )
{
    if( papszPossibleNames == nullptr )
        return false;
    for( char **papszIter = papszPossibleNames; *papszIter; papszIter++ )
    {
        const char *pszPattern = *papszIter;
        const char *pszStar = strstr(pszPattern, "*");
        if( pszStar == nullptr )
        {
            if( EQUAL(pszFieldName, pszPattern) )
                return true;
        }
        else
        {
            if( pszStar == pszPattern )
            {
                if( strlen(pszPattern) >= 3 &&
                    pszPattern[strlen(pszPattern) - 1] == '*' )
                {
                    // *pattern*
                    CPLString oPattern(pszPattern + 1);
                    oPattern.resize(oPattern.size() - 1);
                    if( CPLString(pszFieldName).ifind(oPattern) !=
                        std::string::npos )
                        return true;
                }
                else
                {
                    // *pattern
                    if( strlen(pszFieldName) >= strlen(pszPattern) - 1 &&
                        EQUAL(pszFieldName + strlen(pszFieldName) -
                                  (strlen(pszPattern) - 1),
                              pszPattern + 1) )
                    {
                        return true;
                    }
                }
            }
            else if( pszPattern[strlen(pszPattern) - 1] == '*' )
            {
                // pattern*
                if( EQUALN(pszFieldName, pszPattern, strlen(pszPattern) - 1) )
                    return true;
            }
        }
    }
    return false;
}

/************************************************************************/
/*                      BuildFeatureDefn()                              */
/************************************************************************/

void OGRCSVLayer::BuildFeatureDefn( const char *pszNfdcGeomField,
                                    const char *pszGeonamesGeomFieldPrefix,
                                    char **papszOpenOptions )
{
    bMergeDelimiter = CPLFetchBool(papszOpenOptions, "MERGE_SEPARATOR", false);
    bEmptyStringNull =
        CPLFetchBool(papszOpenOptions, "EMPTY_STRING_AS_NULL", false);

    // If this is not a new file, read ahead to establish if it is
    // already in CRLF (DOS) mode, or just a normal unix CR mode.
    if( !bNew && bInWriteMode )
    {
        int nBytesRead = 0;
        char chNewByte = '\0';

        while( nBytesRead < 10000 && VSIFReadL(&chNewByte, 1, 1, fpCSV) == 1 )
        {
            if( chNewByte == 13 )
            {
                bUseCRLF = true;
                break;
            }
            nBytesRead++;
        }
        VSIRewindL(fpCSV);
    }

    // Check if the first record seems to be field definitions or
    // not.  We assume it is field definitions if the HEADERS option
    // not supplied and none of the values are strictly numeric.
    char **papszTokens = nullptr;
    int nFieldCount = 0;

    if( !bNew )
    {
        const char *pszLine = CPLReadLineL(fpCSV);
        if( pszLine != nullptr )
        {
            // Detect and remove UTF-8 BOM marker if found (#4623).
            if( reinterpret_cast<const unsigned char *>(pszLine)[0] == 0xEF &&
                reinterpret_cast<const unsigned char *>(pszLine)[1] == 0xBB &&
                reinterpret_cast<const unsigned char *>(pszLine)[2] == 0xBF )
            {
                pszLine += 3;
            }

            // Tokenize the strings and preserve quotes, so we can separate
            // string from numeric this is only used in the test for
            // bHasFieldNames (bug #4361).
            const char szDelimiter[2] = { chDelimiter, '\0' };
            papszTokens =
                CSLTokenizeString2(pszLine, szDelimiter,
                                   (CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS |
                                    CSLT_PRESERVEQUOTES));
            nFieldCount = CSLCount(papszTokens);

            if( nFieldCount > 0 && papszTokens[0][0] == '"' )
                m_eStringQuoting = StringQuoting::ALWAYS;

            const char *pszCSVHeaders =
                CSLFetchNameValueDef(papszOpenOptions, "HEADERS", "AUTO");

            if( EQUAL(pszCSVHeaders, "YES") )
            {
                bHasFieldNames = true;
            }
            else if( EQUAL(pszCSVHeaders, "NO") )
            {
                bHasFieldNames = false;
            }
            else
            {
                // Detect via checking for the presence of numeric values.
                bHasFieldNames = true;
                for( int iField = 0;
                     iField < nFieldCount && bHasFieldNames;
                     iField++ )
                {
                    const CPLValueType eType =
                        CPLGetValueType(papszTokens[iField]);
                    if( eType == CPL_VALUE_INTEGER ||
                        eType == CPL_VALUE_REAL )
                    {
                        // We have a numeric field, therefore do not consider
                        // the first line as field names.
                        bHasFieldNames = false;
                    }
                }

                const CPLString osExt =
                    OGRCSVDataSource::GetRealExtension(pszFilename);

                // Eurostat .tsv files.
                if( EQUAL(osExt, "tsv") && nFieldCount > 1 &&
                    strchr(papszTokens[0], ',') != nullptr &&
                    strchr(papszTokens[0], '\\') != nullptr )
                {
                    bHasFieldNames = true;
                    bIsEurostatTSV = true;
                }
            }

            // Tokenize without quotes to get the actual values.
            CSLDestroy(papszTokens);
            int l_nFlags = CSLT_HONOURSTRINGS;
            if( !bMergeDelimiter )
                l_nFlags |= CSLT_ALLOWEMPTYTOKENS;
            papszTokens = CSLTokenizeString2(pszLine, szDelimiter, l_nFlags);
            nFieldCount = CSLCount(papszTokens);
        }
    }
    else
    {
        bHasFieldNames = false;
    }

    if( !bNew )
        ResetReading();

    nCSVFieldCount = nFieldCount;

    panGeomFieldIndex = static_cast<int *>(CPLCalloc(nFieldCount, sizeof(int)));
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        panGeomFieldIndex[iField] = -1;
    }

    // Check for geonames.org tables.
    if( !bHasFieldNames && nFieldCount == 19 )
    {
        if( CPLGetValueType(papszTokens[0]) == CPL_VALUE_INTEGER &&
            CPLGetValueType(papszTokens[4]) == CPL_VALUE_REAL &&
            CPLGetValueType(papszTokens[5]) == CPL_VALUE_REAL &&
            CPLAtof(papszTokens[4]) >= -90 && CPLAtof(papszTokens[4]) <= 90 &&
            CPLAtof(papszTokens[5]) >= -180 && CPLAtof(papszTokens[4]) <= 180 )
        {
            CSLDestroy(papszTokens);
            papszTokens = nullptr;

            static const struct {
                const char *pszName;
                OGRFieldType eType;
            }
            asGeonamesFieldDesc[] =
            {
                { "GEONAMEID", OFTString },
                { "NAME", OFTString },
                { "ASCIINAME", OFTString },
                { "ALTNAMES", OFTString },
                { "LATITUDE", OFTReal },
                { "LONGITUDE", OFTReal },
                { "FEATCLASS", OFTString },
                { "FEATCODE", OFTString },
                { "COUNTRY", OFTString },
                { "CC2", OFTString },
                { "ADMIN1", OFTString },
                { "ADMIN2", OFTString },
                { "ADMIN3", OFTString },
                { "ADMIN4", OFTString },
                { "POPULATION", OFTReal },
                { "ELEVATION", OFTInteger },
                { "GTOPO30", OFTInteger },
                { "TIMEZONE", OFTString },
                { "MODDATE", OFTString }
            };
            for( int iField = 0; iField < nFieldCount; iField++)
            {
                OGRFieldDefn oFieldDefn(asGeonamesFieldDesc[iField].pszName,
                                        asGeonamesFieldDesc[iField].eType);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }

            iLatitudeField = 4;
            iLongitudeField = 5;

            nFieldCount = 0;

            bDontHonourStrings = true;
        }
    }

    // Search a csvt file for types.
    char **papszFieldTypes = nullptr;
    if( !bNew )
    {
        // Only try to read .csvt from files that have an extension
        const char* pszExt = CPLGetExtension(pszFilename);
        if( pszExt[0] )
        {
            char *dname = CPLStrdup(CPLGetDirname(pszFilename));
            char *fname = CPLStrdup(CPLGetBasename(pszFilename));
            VSILFILE *fpCSVT =
                VSIFOpenL(CPLFormFilename(dname, fname, ".csvt"), "r");
            CPLFree(dname);
            CPLFree(fname);
            if( fpCSVT != nullptr )
            {
                VSIRewindL(fpCSVT);
                papszFieldTypes = OGRCSVReadParseLineL(fpCSVT, ',', false, false);
                VSIFCloseL(fpCSVT);
            }
        }
    }

    // Optionally auto-detect types.
    if( !bNew && papszFieldTypes == nullptr &&
        CPLTestBool(
            CSLFetchNameValueDef(papszOpenOptions, "AUTODETECT_TYPE", "NO")) )
    {
        papszFieldTypes = AutodetectFieldTypes(papszOpenOptions, nFieldCount);
        if( papszFieldTypes != nullptr )
        {
            bKeepSourceColumns = CPLTestBool(CSLFetchNameValueDef(
                papszOpenOptions, "KEEP_SOURCE_COLUMNS", "NO"));
        }
    }

    char **papszGeomPossibleNames = CSLTokenizeString2(
        CSLFetchNameValue(papszOpenOptions, "GEOM_POSSIBLE_NAMES"), ",", 0);
    char **papszXPossibleNames = CSLTokenizeString2(
        CSLFetchNameValue(papszOpenOptions, "X_POSSIBLE_NAMES"), ",", 0);
    char **papszYPossibleNames = CSLTokenizeString2(
        CSLFetchNameValue(papszOpenOptions, "Y_POSSIBLE_NAMES"), ",", 0);
    char **papszZPossibleNames = CSLTokenizeString2(
        CSLFetchNameValue(papszOpenOptions, "Z_POSSIBLE_NAMES"), ",", 0);
    bKeepGeomColumns = CPLTestBool(
        CSLFetchNameValueDef(papszOpenOptions, "KEEP_GEOM_COLUMNS", "YES"));

    // Build field definitions.
    poFeatureDefn->ReserveSpaceForFields(nFieldCount);

    constexpr int knMAX_GEOM_COLUMNS = 100;
    bool bWarnedMaxGeomFields = false;

    for( int iField = 0; !bIsEurostatTSV && iField < nFieldCount; iField++ )
    {
        char *pszFieldName = nullptr;
        char szFieldNameBuffer[100];

        if( bHasFieldNames )
        {
            pszFieldName = papszTokens[iField];

            // Trim white space.
            while( *pszFieldName == ' ' )
                pszFieldName++;

            while( pszFieldName[0] != '\0' &&
                   pszFieldName[strlen(pszFieldName) - 1] == ' ' )
                pszFieldName[strlen(pszFieldName) - 1] = '\0';

            if( *pszFieldName == '\0' )
                pszFieldName = nullptr;
        }

        if( pszFieldName == nullptr )
        {
            // Re-read single column CSV files that have a trailing comma
            // in the header line.
            if( iField == 1 && nFieldCount == 2 && papszTokens[1][0] == '\0' )
            {
                nCSVFieldCount = 1;
                nFieldCount = 1;
                break;
            }
            pszFieldName = szFieldNameBuffer;
            snprintf(szFieldNameBuffer, sizeof(szFieldNameBuffer),
                     "field_%d", iField + 1);
        }

        OGRFieldDefn oField(pszFieldName, OFTString);
        if( papszFieldTypes != nullptr && iField < CSLCount(papszFieldTypes) )
        {
            if( EQUAL(papszFieldTypes[iField], "WKT") )
            {
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn(&oField);

                if( poFeatureDefn->GetGeomFieldCount() == knMAX_GEOM_COLUMNS )
                {
                    if( !bWarnedMaxGeomFields )
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                            "A maximum number of %d geometry fields is supported. "
                            "Only the first ones are taken into account.",
                            knMAX_GEOM_COLUMNS);
                        bWarnedMaxGeomFields = true;
                    }
                    continue;
                }

                eGeometryFormat = OGR_CSV_GEOM_AS_WKT;
                panGeomFieldIndex[iField] = poFeatureDefn->GetGeomFieldCount();
                OGRGeomFieldDefn oGeomFieldDefn(oField.GetNameRef(),
                                                wkbUnknown);
                poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
                continue;
            }
            else if( EQUAL(papszFieldTypes[iField], "CoordX") ||
                     EQUAL(papszFieldTypes[iField], "Point(X)") )
            {
                oField.SetType(OFTReal);
                iLongitudeField = iField;
                osXField = oField.GetNameRef();
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn(&oField);
                continue;
            }
            else if( EQUAL(papszFieldTypes[iField], "CoordY") ||
                     EQUAL(papszFieldTypes[iField], "Point(Y)") )
            {
                oField.SetType(OFTReal);
                iLatitudeField = iField;
                osYField = oField.GetNameRef();
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn(&oField);
                continue;
            }
            else if( EQUAL(papszFieldTypes[iField], "CoordZ") ||
                     EQUAL(papszFieldTypes[iField], "Point(Z)") )
            {
                oField.SetType(OFTReal);
                iZField = iField;
                osZField = oField.GetNameRef();
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn(&oField);
                continue;
            }
            else if( EQUAL(papszFieldTypes[iField], "Integer(Boolean)") )
            {
                oField.SetType(OFTInteger);
                oField.SetSubType(OFSTBoolean);
                oField.SetWidth(1);
            }
            else if( EQUAL(papszFieldTypes[iField], "Integer(Int16)") )
            {
                oField.SetType(OFTInteger);
                oField.SetSubType(OFSTInt16);
            }
            else if( EQUAL(papszFieldTypes[iField], "Real(Float32)") )
            {
                oField.SetType(OFTReal);
                oField.SetSubType(OFSTFloat32);
            }
            else
            {
                char *pszLeftParenthesis = strchr(papszFieldTypes[iField], '(');
                if( pszLeftParenthesis &&
                    pszLeftParenthesis != papszFieldTypes[iField] &&
                    pszLeftParenthesis[1] >= '0' &&
                    pszLeftParenthesis[1] <= '9' )
                {
                    char *pszDot = strchr(pszLeftParenthesis, '.');
                    if( pszDot )
                        *pszDot = 0;
                    *pszLeftParenthesis = 0;

                    if( pszLeftParenthesis[-1] == ' ' )
                        pszLeftParenthesis[-1] = 0;

                    const int nWidth = atoi(pszLeftParenthesis + 1);
                    const int nPrecision = pszDot ? atoi(pszDot + 1) : 0;

                    oField.SetWidth(nWidth);
                    oField.SetPrecision(nPrecision);
                }

                if( EQUAL(papszFieldTypes[iField], "Integer") )
                    oField.SetType(OFTInteger);
                else if (EQUAL(papszFieldTypes[iField], "Integer64") )
                    oField.SetType(OFTInteger64);
                else if( EQUAL(papszFieldTypes[iField], "Real") )
                    oField.SetType(OFTReal);
                else if( EQUAL(papszFieldTypes[iField], "String") )
                    oField.SetType(OFTString);
                else if( EQUAL(papszFieldTypes[iField], "Date") )
                    oField.SetType(OFTDate);
                else if( EQUAL(papszFieldTypes[iField], "Time") )
                    oField.SetType(OFTTime);
                else if( EQUAL(papszFieldTypes[iField], "DateTime") )
                    oField.SetType(OFTDateTime);
                else if( EQUAL(papszFieldTypes[iField], "JSonStringList") )
                    oField.SetType(OFTStringList);
                else if( EQUAL(papszFieldTypes[iField], "JSonIntegerList") )
                    oField.SetType(OFTIntegerList);
                else if( EQUAL(papszFieldTypes[iField], "JSonInteger64List") )
                    oField.SetType(OFTInteger64List);
                else if( EQUAL(papszFieldTypes[iField], "JSonRealList") )
                    oField.SetType(OFTRealList);
                else
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Unknown type : %s", papszFieldTypes[iField]);
            }
        }

        if( Matches(oField.GetNameRef(), papszZPossibleNames) )
        {
            oField.SetType(OFTReal);
            iZField = iField;
            osZField = oField.GetNameRef();
            if( !bKeepGeomColumns )
                continue;
        }
        else if( (iNfdcLatitudeS != -1 && iNfdcLongitudeS != -1) ||
                 (iLatitudeField != -1 && iLongitudeField != -1) )
        {
            // Do nothing.
        }
        else if( (EQUAL(oField.GetNameRef(), "WKT") ||
                  STARTS_WITH_CI(oField.GetNameRef(), "_WKT")) &&
                 oField.GetType() == OFTString )
        {
            if( poFeatureDefn->GetGeomFieldCount() == knMAX_GEOM_COLUMNS )
            {
                if( !bWarnedMaxGeomFields )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                        "A maximum number of %d geometry fields is supported. "
                        "Only the first ones are taken into account.",
                        knMAX_GEOM_COLUMNS);
                    bWarnedMaxGeomFields = true;
                }
            }
            else
            {
                eGeometryFormat = OGR_CSV_GEOM_AS_WKT;

                panGeomFieldIndex[iField] = poFeatureDefn->GetGeomFieldCount();
                OGRGeomFieldDefn oGeomFieldDefn(
                    EQUAL(pszFieldName, "WKT") ? "" : CPLSPrintf("geom_%s",
                                                                pszFieldName),
                    wkbUnknown );

                // Useful hack for RFC 41 testing.
                const char *pszEPSG = strstr(pszFieldName, "_EPSG_");
                if( pszEPSG != nullptr )
                {
                    const int nEPSGCode = atoi(pszEPSG + strlen("_EPSG_"));
                    OGRSpatialReference *poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    poSRS->importFromEPSG(nEPSGCode);
                    oGeomFieldDefn.SetSpatialRef(poSRS);
                    poSRS->Release();
                }

                if( strstr(pszFieldName, "_POINT") )
                    oGeomFieldDefn.SetType(wkbPoint);
                else if( strstr(pszFieldName, "_LINESTRING") )
                    oGeomFieldDefn.SetType(wkbLineString);
                else if( strstr(pszFieldName, "_POLYGON") )
                    oGeomFieldDefn.SetType(wkbPolygon);
                else if( strstr(pszFieldName, "_MULTIPOINT") )
                    oGeomFieldDefn.SetType(wkbMultiPoint);
                else if( strstr(pszFieldName, "_MULTILINESTRING") )
                    oGeomFieldDefn.SetType(wkbMultiLineString);
                else if( strstr(pszFieldName, "_MULTIPOLYGON") )
                    oGeomFieldDefn.SetType(wkbMultiPolygon);
                else if( strstr(pszFieldName, "_CIRCULARSTRING") )
                    oGeomFieldDefn.SetType(wkbCircularString);
                else if( strstr(pszFieldName, "_COMPOUNDCURVE") )
                    oGeomFieldDefn.SetType(wkbCompoundCurve);
                else if( strstr(pszFieldName, "_CURVEPOLYGON") )
                    oGeomFieldDefn.SetType(wkbCurvePolygon);
                else if( strstr(pszFieldName, "_CURVE") )
                    oGeomFieldDefn.SetType(wkbCurve);
                else if( strstr(pszFieldName, "_SURFACE") )
                    oGeomFieldDefn.SetType(wkbSurface);
                else if( strstr(pszFieldName, "_MULTICURVE") )
                    oGeomFieldDefn.SetType(wkbMultiCurve);
                else if( strstr(pszFieldName, "_MULTISURFACE") )
                    oGeomFieldDefn.SetType(wkbMultiSurface);
                else if( strstr(pszFieldName, "_POLYHEDRALSURFACE") )
                    oGeomFieldDefn.SetType(wkbPolyhedralSurface);
                else if( strstr(pszFieldName, "_TIN") )
                    oGeomFieldDefn.SetType(wkbTIN);
                else if( strstr(pszFieldName, "_TRIANGLE") )
                    oGeomFieldDefn.SetType(wkbTriangle);

                poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
                if( !bKeepGeomColumns )
                    continue;
            }
        }
        else if( Matches(oField.GetNameRef(), papszGeomPossibleNames) )
        {
            eGeometryFormat = OGR_CSV_GEOM_AS_SOME_GEOM_FORMAT;
            panGeomFieldIndex[iField] = poFeatureDefn->GetGeomFieldCount();
            OGRGeomFieldDefn oGeomFieldDefn(oField.GetNameRef(), wkbUnknown);
            poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
            if( !bKeepGeomColumns )
                continue;
        }
        else if( Matches(oField.GetNameRef(), papszXPossibleNames) &&
                 poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            oField.SetType(OFTReal);
            iLongitudeField = iField;
            osXField = oField.GetNameRef();
            if( !bKeepGeomColumns )
                continue;
        }
        else if( Matches(oField.GetNameRef(), papszYPossibleNames) &&
                 poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            oField.SetType(OFTReal);
            iLatitudeField = iField;
            osYField = oField.GetNameRef();
            if( !bKeepGeomColumns )
                continue;
        }

        // TODO(schwehr): URL broken.
        // http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm
        // specific
        else if( pszNfdcGeomField != nullptr &&
                 EQUALN(oField.GetNameRef(), pszNfdcGeomField,
                        strlen(pszNfdcGeomField)) &&
                 EQUAL(oField.GetNameRef() + strlen(pszNfdcGeomField),
                       "LatitudeS") &&
                 poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            iNfdcLatitudeS = iField;
            if( !bKeepGeomColumns )
                continue;
        }
        else if( pszNfdcGeomField != nullptr &&
                 EQUALN(oField.GetNameRef(), pszNfdcGeomField,
                        strlen(pszNfdcGeomField)) &&
                 EQUAL(oField.GetNameRef() + strlen(pszNfdcGeomField),
                       "LongitudeS") &&
                 poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            iNfdcLongitudeS = iField;
            if( !bKeepGeomColumns )
                continue;
        }
        // GNIS specific.
        else if( pszGeonamesGeomFieldPrefix != nullptr &&
                 EQUALN(oField.GetNameRef(), pszGeonamesGeomFieldPrefix,
                        strlen(pszGeonamesGeomFieldPrefix)) &&
                 (EQUAL(oField.GetNameRef() +
                            strlen(pszGeonamesGeomFieldPrefix),
                        "_LAT_DEC") ||
                  EQUAL(oField.GetNameRef() +
                            strlen(pszGeonamesGeomFieldPrefix),
                        "_LATITUDE_DEC") ||
                  EQUAL(oField.GetNameRef() +
                            strlen(pszGeonamesGeomFieldPrefix),
                        "_LATITUDE")) &&
                 poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            oField.SetType(OFTReal);
            iLatitudeField = iField;
            osYField = oField.GetNameRef();
            if( !bKeepGeomColumns )
                continue;
        }
        else if( pszGeonamesGeomFieldPrefix != nullptr &&
                 EQUALN(oField.GetNameRef(), pszGeonamesGeomFieldPrefix,
                        strlen(pszGeonamesGeomFieldPrefix)) &&
                 (EQUAL(oField.GetNameRef() +
                            strlen(pszGeonamesGeomFieldPrefix),
                        "_LONG_DEC") ||
                  EQUAL(oField.GetNameRef() +
                            strlen(pszGeonamesGeomFieldPrefix),
                        "_LONGITUDE_DEC") ||
                  EQUAL(oField.GetNameRef() +
                            strlen(pszGeonamesGeomFieldPrefix),
                        "_LONGITUDE")) &&
                 poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            oField.SetType(OFTReal);
            iLongitudeField = iField;
            osXField = oField.GetNameRef();
            if( !bKeepGeomColumns )
                continue;
        }

        poFeatureDefn->AddFieldDefn(&oField);

        if( bKeepSourceColumns && oField.GetType() != OFTString )
        {
            OGRFieldDefn oFieldOriginal(
                CPLSPrintf("%s_original", oField.GetNameRef()), OFTString);
            poFeatureDefn->AddFieldDefn(&oFieldOriginal);
        }
    }

    if( iNfdcLatitudeS != -1 && iNfdcLongitudeS != -1 )
    {
        bDontHonourStrings = true;
        if( poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            poFeatureDefn->SetGeomType(wkbPoint);
        }
        else
        {
            iNfdcLatitudeS = -1;
            iNfdcLongitudeS = -1;
            iLatitudeField = -1;
            iLongitudeField = -1;
        }
    }
    else if( iLatitudeField != -1 && iLongitudeField != -1 )
    {
        if( poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            poFeatureDefn->SetGeomType(iZField >= 0 ? wkbPoint25D : wkbPoint);
        }
        else
        {
            iNfdcLatitudeS = -1;
            iNfdcLongitudeS = -1;
            iLatitudeField = -1;
            iLongitudeField = -1;
        }
    }

    if( poFeatureDefn->GetGeomFieldCount() > 0 &&
        poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef() == nullptr )
    {
        VSILFILE *fpPRJ =
            VSIFOpenL(CPLResetExtension(pszFilename, "prj"), "rb");
        if( fpPRJ != nullptr )
        {
            GByte *pabyRet = nullptr;
            if( VSIIngestFile(fpPRJ, nullptr, &pabyRet, nullptr, 1000000) )
            {
                OGRSpatialReference *poSRS = new OGRSpatialReference();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if( poSRS->SetFromUserInput((const char *)pabyRet) ==
                    OGRERR_NONE )
                {
                    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
                }
                poSRS->Release();
            }
            CPLFree(pabyRet);
            VSIFCloseL(fpPRJ);
        }
    }

    CSLDestroy(papszGeomPossibleNames);
    CSLDestroy(papszXPossibleNames);
    CSLDestroy(papszYPossibleNames);
    CSLDestroy(papszZPossibleNames);

    // Build field definitions for Eurostat TSV files.

    CPLString osSeqDim;
    for( int iField = 0; bIsEurostatTSV && iField < nFieldCount; iField++ )
    {
        if( iField == 0 )
        {
            char **papszDims = CSLTokenizeString2(papszTokens[0], ",\\", 0);
            nEurostatDims = CSLCount(papszDims) - 1;
            for(int iSubField = 0; iSubField < nEurostatDims; iSubField++)
            {
                OGRFieldDefn oField(papszDims[iSubField], OFTString);
                poFeatureDefn->AddFieldDefn(&oField);
            }

            if( nEurostatDims >= 0 )
                osSeqDim = papszDims[nEurostatDims];
            else
                CPLError(CE_Warning, CPLE_AppDefined, "Invalid nEurostatDims");

            CSLDestroy(papszDims);
        }
        else
        {
            if( papszTokens[iField][0] != '\0' &&
                papszTokens[iField][strlen(papszTokens[iField]) - 1] == ' ' )
                papszTokens[iField][strlen(papszTokens[iField]) - 1] = '\0';

            OGRFieldDefn oField(
                CPLSPrintf("%s_%s", osSeqDim.c_str(), papszTokens[iField]),
                OFTReal);
            poFeatureDefn->AddFieldDefn(&oField);

            OGRFieldDefn oField2(
                CPLSPrintf("%s_%s_flag", osSeqDim.c_str(), papszTokens[iField]),
                OFTString);
            poFeatureDefn->AddFieldDefn(&oField2);
        }
    }

    CSLDestroy(papszTokens);
    CSLDestroy(papszFieldTypes);
}

/************************************************************************/
/*                             OGRCSVIsTrue()                           */
/************************************************************************/

static bool OGRCSVIsTrue(const char *pszStr)
{
    return EQUAL(pszStr, "t") || EQUAL(pszStr, "true") || EQUAL(pszStr, "y") ||
           EQUAL(pszStr, "yes") || EQUAL(pszStr, "on");
}

/************************************************************************/
/*                            OGRCSVIsFalse()                           */
/************************************************************************/

static bool OGRCSVIsFalse(const char *pszStr)
{
    return EQUAL(pszStr, "f") || EQUAL(pszStr, "false") || EQUAL(pszStr, "n") ||
           EQUAL(pszStr, "no") || EQUAL(pszStr, "off");
}

/************************************************************************/
/*                        AutodetectFieldTypes()                        */
/************************************************************************/

char **OGRCSVLayer::AutodetectFieldTypes(char **papszOpenOptions,
                                         int nFieldCount)
{
    // Use 1000000 as default maximum distance to be compatible with /vsistdin/
    // caching.
    int nBytes = atoi(CSLFetchNameValueDef(papszOpenOptions,
                                           "AUTODETECT_SIZE_LIMIT", "1000000"));
    if( nBytes == 0 )
    {
        const vsi_l_offset nCurPos = VSIFTellL(fpCSV);
        CPL_IGNORE_RET_VAL(VSIFSeekL(fpCSV, 0, SEEK_END));
        const vsi_l_offset nFileSize = VSIFTellL(fpCSV);
        CPL_IGNORE_RET_VAL(VSIFSeekL(fpCSV, nCurPos, SEEK_SET));
        nBytes = std::min(static_cast<int>(nFileSize),
                          std::numeric_limits<int>::max());
    }
    else if( nBytes < 0 ||
             static_cast<vsi_l_offset>(nBytes) < VSIFTellL(fpCSV) )
    {
        nBytes = 1000000;
    }

    const char *pszAutodetectWidth =
        CSLFetchNameValueDef(papszOpenOptions, "AUTODETECT_WIDTH", "NO");

    const bool bAutodetectWidthForIntOrReal = EQUAL(pszAutodetectWidth, "YES");
    const bool bAutodetectWidth = bAutodetectWidthForIntOrReal ||
                                  EQUAL(pszAutodetectWidth, "STRING_ONLY");

    const bool bQuotedFieldAsString = CPLTestBool(CSLFetchNameValueDef(
        papszOpenOptions, "QUOTED_FIELDS_AS_STRING", "NO"));

    // This will be returned as the result.
    char **papszFieldTypes = nullptr;

    char *pszData = static_cast<char *>(VSI_MALLOC_VERBOSE(nBytes));
    if( pszData != nullptr &&
        static_cast<vsi_l_offset>(nBytes) > VSIFTellL(fpCSV) )
    {
        const int nRequested = nBytes - 1 - static_cast<int>(VSIFTellL(fpCSV));
        const int nRead =
            static_cast<int>(VSIFReadL(pszData, 1, nRequested, fpCSV));
        pszData[nRead] = 0;

        CPLString osTmpMemFile(CPLSPrintf("/vsimem/tmp%p", this));
        VSILFILE *fpMem = VSIFileFromMemBuffer(
            osTmpMemFile, reinterpret_cast<GByte *>(pszData), nRead, FALSE);

        std::vector<OGRFieldType> aeFieldType(nFieldCount);
        std::vector<int> abFieldBoolean(nFieldCount);
        std::vector<int> abFieldSet(nFieldCount);
        std::vector<int> anFieldWidth(nFieldCount);
        std::vector<int> anFieldPrecision(nFieldCount);
        int nStringFieldCount = 0;

        while( !VSIFEofL(fpMem) )
        {
            char **papszTokens =
                OGRCSVReadParseLineL(fpMem, chDelimiter, false,
                                     bQuotedFieldAsString, bMergeDelimiter);
            // Can happen if we just reach EOF while trying to read new bytes.
            if( papszTokens == nullptr )
                break;

            // Ignore last line if it is truncated.
            if( VSIFEofL(fpMem) && nRead == nRequested &&
                pszData[nRead - 1] != 13 && pszData[nRead - 1] != 10 )
            {
                CSLDestroy(papszTokens);
                break;
            }

            for( int iField = 0;
                 iField < nFieldCount && papszTokens[iField] != nullptr; iField++ )
            {
                if( papszTokens[iField][0] == 0 )
                    continue;
                if( chDelimiter == ';' )
                {
                    char *chComma = strchr(papszTokens[iField], ',');
                    if( chComma )
                        *chComma = '.';
                }
                CPLValueType eType = CPLGetValueType(papszTokens[iField]);

                int nFieldWidth = 0;
                int nFieldPrecision = 0;

                if( bAutodetectWidth )
                {
                    nFieldWidth = static_cast<int>(strlen(papszTokens[iField]));
                    if( papszTokens[iField][0] == '"' &&
                        papszTokens[iField][nFieldWidth - 1] == '"' )
                    {
                        nFieldWidth -= 2;
                    }
                    if( eType == CPL_VALUE_REAL &&
                        bAutodetectWidthForIntOrReal )
                    {
                        const char *pszDot = strchr(papszTokens[iField], '.');
                        if( pszDot != nullptr )
                            nFieldPrecision =
                                static_cast<int>(strlen(pszDot + 1));
                    }
                }

                OGRFieldType eOGRFieldType;
                bool bIsBoolean = false;
                if( eType == CPL_VALUE_INTEGER )
                {
                    GIntBig nVal = CPLAtoGIntBig(papszTokens[iField]);
                    if( !CPL_INT64_FITS_ON_INT32(nVal) )
                        eOGRFieldType = OFTInteger64;
                    else
                        eOGRFieldType = OFTInteger;
                }
                else if( eType == CPL_VALUE_REAL )
                {
                    eOGRFieldType = OFTReal;
                }
                else if( abFieldSet[iField] &&
                         aeFieldType[iField] == OFTString )
                {
                    eOGRFieldType = OFTString;
                    if( abFieldBoolean[iField] )
                    {
                        abFieldBoolean[iField] =
                            OGRCSVIsTrue(papszTokens[iField]) ||
                            OGRCSVIsFalse(papszTokens[iField]);
                    }
                }
                else
                {
                    OGRField sWrkField;
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    const bool bSuccess = CPL_TO_BOOL(
                        OGRParseDate(papszTokens[iField], &sWrkField, 0));
                    CPLPopErrorHandler();
                    CPLErrorReset();
                    if( bSuccess )
                    {
                        const bool bHasDate =
                            strchr(papszTokens[iField], '/') != nullptr ||
                            strchr(papszTokens[iField], '-') != nullptr;
                        const bool bHasTime =
                            strchr(papszTokens[iField], ':') != nullptr;
                        if( bHasDate && bHasTime )
                            eOGRFieldType = OFTDateTime;
                        else if( bHasDate )
                            eOGRFieldType = OFTDate;
                        else
                            eOGRFieldType = OFTTime;
                    }
                    else
                    {
                        eOGRFieldType = OFTString;
                        bIsBoolean = OGRCSVIsTrue(papszTokens[iField]) ||
                                     OGRCSVIsFalse(papszTokens[iField]);
                    }
                }

                if( !abFieldSet[iField] )
                {
                    aeFieldType[iField] = eOGRFieldType;
                    abFieldSet[iField] = TRUE;
                    abFieldBoolean[iField] = bIsBoolean;
                    if( eOGRFieldType == OFTString && !bIsBoolean )
                        nStringFieldCount++;
                }
                else if( aeFieldType[iField] != eOGRFieldType )
                {
                    // Promotion rules.
                    if( aeFieldType[iField] == OFTInteger )
                    {
                        if( eOGRFieldType == OFTInteger64 ||
                            eOGRFieldType == OFTReal )
                            aeFieldType[iField] = eOGRFieldType;
                        else
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTInteger64 )
                    {
                        if( eOGRFieldType == OFTReal )
                            aeFieldType[iField] = eOGRFieldType;
                        else if( eOGRFieldType != OFTInteger )
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTReal )
                    {
                        if( eOGRFieldType != OFTInteger &&
                            eOGRFieldType != OFTInteger64 &&
                            eOGRFieldType != OFTReal )
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTDate )
                    {
                        if( eOGRFieldType == OFTDateTime )
                            aeFieldType[iField] = OFTDateTime;
                        else
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTDateTime )
                    {
                        if( eOGRFieldType != OFTDate &&
                            eOGRFieldType != OFTDateTime )
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTTime )
                    {
                        aeFieldType[iField] = OFTString;
                        nStringFieldCount++;
                    }
                }

                if( nFieldWidth > anFieldWidth[iField] )
                    anFieldWidth[iField] = nFieldWidth;
                if( nFieldPrecision > anFieldPrecision[iField] )
                    anFieldPrecision[iField] = nFieldPrecision;
            }

            CSLDestroy(papszTokens);

            // If all fields are String and we don't need to compute width,
            // just stop auto-detection now.
            if( nStringFieldCount == nFieldCount && bAutodetectWidth )
                break;
        }

        papszFieldTypes =
            static_cast<char **>(CPLCalloc(nFieldCount + 1, sizeof(char *)));
        for( int iField = 0; iField < nFieldCount; iField++ )
        {
            CPLString osFieldType;
            if( !abFieldSet[iField] )
                osFieldType = "String";
            else if( aeFieldType[iField] == OFTInteger )
                osFieldType = "Integer";
            else if( aeFieldType[iField] == OFTInteger64 )
                osFieldType = "Integer64";
            else if( aeFieldType[iField] == OFTReal )
                osFieldType = "Real";
            else if( aeFieldType[iField] == OFTDateTime  )
                osFieldType = "DateTime";
            else if( aeFieldType[iField] == OFTDate  )
                osFieldType = "Date";
            else if( aeFieldType[iField] == OFTTime  )
                osFieldType = "Time";
            else if( aeFieldType[iField] == OFTStringList  )
                osFieldType = "JSonStringList";
            else if( aeFieldType[iField] == OFTIntegerList  )
                osFieldType = "JSonIntegerList";
            else if( aeFieldType[iField] == OFTInteger64List  )
                osFieldType = "JSonInteger64List";
            else if( aeFieldType[iField] == OFTRealList  )
                osFieldType = "JSonRealList";
            else if( abFieldBoolean[iField] )
                osFieldType = "Integer(Boolean)";
            else
                osFieldType = "String";

            if( !abFieldBoolean[iField] )
            {
                if( anFieldWidth[iField] > 0 &&
                    (aeFieldType[iField] == OFTString ||
                    (bAutodetectWidthForIntOrReal &&
                     (aeFieldType[iField] == OFTInteger ||
                      aeFieldType[iField] == OFTInteger64))) )
                {
                    osFieldType += CPLSPrintf(" (%d)", anFieldWidth[iField]);
                }
                else if( anFieldWidth[iField] > 0 &&
                         bAutodetectWidthForIntOrReal &&
                         aeFieldType[iField] == OFTReal )
                {
                    osFieldType += CPLSPrintf(" (%d.%d)", anFieldWidth[iField],
                                              anFieldPrecision[iField]);
                }
            }

            papszFieldTypes[iField] = CPLStrdup(osFieldType);
        }

        VSIFCloseL(fpMem);
        VSIUnlink(osTmpMemFile);
    }
    VSIFree(pszData);

    ResetReading();

    return papszFieldTypes;
}

/************************************************************************/
/*                            ~OGRCSVLayer()                            */
/************************************************************************/

OGRCSVLayer::~OGRCSVLayer()

{
    if( m_nFeaturesRead > 0 )
    {
        CPLDebug("CSV", "%d features read on layer '%s'.",
                 static_cast<int>(m_nFeaturesRead),
                 poFeatureDefn->GetName());
    }

    // Make sure the header file is written even if no features are written.
    if( bNew && bInWriteMode )
        WriteHeader();

    CPLFree(panGeomFieldIndex);

    poFeatureDefn->Release();
    CPLFree(pszFilename);

    if( fpCSV )
        VSIFCloseL(fpCSV);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCSVLayer::ResetReading()

{
    if( fpCSV )
        VSIRewindL(fpCSV);

    if( bHasFieldNames )
        CSLDestroy(
            OGRCSVReadParseLineL(fpCSV, chDelimiter, bDontHonourStrings));

    bNeedRewindBeforeRead = false;

    nNextFID = 1;
}

/************************************************************************/
/*                        GetNextLineTokens()                           */
/************************************************************************/

char **OGRCSVLayer::GetNextLineTokens()
{
    while( true )
    {
        // Read the CSV record.
        char **papszTokens = OGRCSVReadParseLineL(
            fpCSV, chDelimiter, bDontHonourStrings, false, bMergeDelimiter);

        if( papszTokens == nullptr )
            return nullptr;

        if( papszTokens[0] != nullptr )
            return papszTokens;

        CSLDestroy(papszTokens);
    }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRCSVLayer::GetFeature(GIntBig nFID)
{
    if( nFID < 1 || fpCSV == nullptr )
        return nullptr;
    if( nFID < nNextFID || bNeedRewindBeforeRead )
        ResetReading();
    while( nNextFID < nFID )
    {
        char **papszTokens = GetNextLineTokens();
        if( papszTokens == nullptr )
            return nullptr;
        CSLDestroy(papszTokens);
        nNextFID++;
    }
    return GetNextUnfilteredFeature();
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRCSVLayer::GetNextUnfilteredFeature()

{
    if( fpCSV == nullptr )
        return nullptr;

    // Read the CSV record.
    char **papszTokens = GetNextLineTokens();
    if( papszTokens == nullptr )
        return nullptr;

    // Create the OGR feature.
    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);

    // Set attributes for any indicated attribute records.
    int iOGRField = 0;
    const int nAttrCount = std::min(
        CSLCount(papszTokens), nCSVFieldCount + (bHiddenWKTColumn ? 1 : 0));
    CPLValueType eType;

    for( int iAttr = 0; !bIsEurostatTSV && iAttr < nAttrCount; iAttr++ )
    {
        if( (iAttr == iLongitudeField || iAttr == iLatitudeField ||
             iAttr == iZField) &&
            !bKeepGeomColumns )
        {
            continue;
        }
        int iGeom = 0;
        if( bHiddenWKTColumn )
        {
            if( iAttr != 0 )
                iGeom = panGeomFieldIndex[iAttr - 1];
        }
        else
        {
            iGeom = panGeomFieldIndex[iAttr];
        }
        if( iGeom >= 0 )
        {
            if( papszTokens[iAttr][0] != '\0' &&
                !(poFeatureDefn->GetGeomFieldDefn(iGeom)->IsIgnored()) )
            {
                const char *pszStr = papszTokens[iAttr];
                while( *pszStr == ' ' )
                    pszStr++;
                OGRGeometry *poGeom = nullptr;

                CPLPushErrorHandler(CPLQuietErrorHandler);
                if( OGRGeometryFactory::createFromWkt(pszStr, nullptr, &poGeom) ==
                    OGRERR_NONE )
                {
                    poGeom->assignSpatialReference(
                        poFeatureDefn->GetGeomFieldDefn(iGeom)
                            ->GetSpatialRef());
                    poFeature->SetGeomFieldDirectly(iGeom, poGeom);
                }
                else if( *pszStr == '{' &&
                         (poGeom = reinterpret_cast<OGRGeometry *>(
                              OGR_G_CreateGeometryFromJson(pszStr))) != nullptr )
                {
                    poFeature->SetGeomFieldDirectly(iGeom, poGeom);
                }
                else if( ((*pszStr >= '0' && *pszStr <= '9') ||
                          (*pszStr >= 'a' && *pszStr <= 'z') ||
                          (*pszStr >= 'A' && *pszStr <= 'Z')) &&
                         (poGeom = OGRGeometryFromHexEWKB(pszStr, nullptr,
                                                          FALSE)) != nullptr )
                {
                    poFeature->SetGeomFieldDirectly(iGeom, poGeom);
                }
                CPLPopErrorHandler();
            }
            if( !bKeepGeomColumns || (iAttr == 0 && bHiddenWKTColumn) )
                continue;
        }

        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iOGRField);
        const OGRFieldType eFieldType = poFieldDefn->GetType();
        const OGRFieldSubType eFieldSubType = poFieldDefn->GetSubType();
        if( eFieldType == OFTInteger && eFieldSubType == OFSTBoolean )
        {
            if( papszTokens[iAttr][0] != '\0' && !poFieldDefn->IsIgnored() )
            {
                if( OGRCSVIsTrue(papszTokens[iAttr]) ||
                    strcmp(papszTokens[iAttr], "1") == 0 )
                {
                    poFeature->SetField(iOGRField, 1);
                }
                else if( OGRCSVIsFalse(papszTokens[iAttr]) ||
                         strcmp(papszTokens[iAttr], "0") == 0 )
                {
                    poFeature->SetField(iOGRField, 0);
                }
                else if( !bWarningBadTypeOrWidth )
                {
                    bWarningBadTypeOrWidth = true;
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Invalid value type found in record %d for field %s. "
                        "This warning will no longer be emitted",
                        nNextFID, poFieldDefn->GetNameRef());
                }
            }
        }
        else if( eFieldType == OFTReal || eFieldType == OFTInteger ||
                 eFieldType == OFTInteger64 )
        {
            if( papszTokens[iAttr][0] != '\0' && !poFieldDefn->IsIgnored() )
            {
                if( chDelimiter == ';' && eFieldType == OFTReal )
                {
                    char *chComma = strchr(papszTokens[iAttr], ',');
                    if( chComma )
                        *chComma = '.';
                }
                eType = CPLGetValueType(papszTokens[iAttr]);
                if( eType == CPL_VALUE_INTEGER || eType == CPL_VALUE_REAL )
                {
                    poFeature->SetField(iOGRField, papszTokens[iAttr]);
                    if( !bWarningBadTypeOrWidth &&
                        (eFieldType == OFTInteger ||
                         eFieldType == OFTInteger64) &&
                        eType == CPL_VALUE_REAL )
                    {
                        bWarningBadTypeOrWidth = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Invalid value type found in record %d for "
                                 "field %s. "
                                 "This warning will no longer be emitted",
                                 nNextFID, poFieldDefn->GetNameRef());
                    }
                    else if( !bWarningBadTypeOrWidth &&
                             poFieldDefn->GetWidth() > 0 &&
                             static_cast<int>(strlen(papszTokens[iAttr])) >
                                 poFieldDefn->GetWidth() )
                    {
                        bWarningBadTypeOrWidth = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Value with a width greater than field width "
                                 "found in record %d for field %s. "
                                 "This warning will no longer be emitted",
                                 nNextFID, poFieldDefn->GetNameRef());
                    }
                    else if( !bWarningBadTypeOrWidth &&
                             eType == CPL_VALUE_REAL &&
                             poFieldDefn->GetWidth() > 0)
                    {
                        const char *pszDot = strchr(papszTokens[iAttr], '.');
                        const int nPrecision =
                            pszDot != nullptr
                                ? static_cast<int>(strlen(pszDot + 1))
                                : 0;
                        if( nPrecision > poFieldDefn->GetPrecision() )
                        {
                            bWarningBadTypeOrWidth = true;
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Value with a precision greater than "
                                     "field precision found in record %d for "
                                     "field %s. "
                                     "This warning will no longer be emitted",
                                     nNextFID, poFieldDefn->GetNameRef());
                        }
                    }
                }
                else
                {
                    if( !bWarningBadTypeOrWidth )
                    {
                        bWarningBadTypeOrWidth = true;
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Invalid value type found in record %d for field "
                            "%s. This warning will no longer be emitted.",
                            nNextFID, poFieldDefn->GetNameRef());
                    }
                }
            }
        }
        else if( eFieldType != OFTString )
        {
            if( papszTokens[iAttr][0] != '\0' && !poFieldDefn->IsIgnored() )
            {
                poFeature->SetField(iOGRField, papszTokens[iAttr]);
                if( !bWarningBadTypeOrWidth &&
                    !poFeature->IsFieldSetAndNotNull(iOGRField) )
                {
                    bWarningBadTypeOrWidth = true;
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Invalid value type found in record %d for field %s. "
                        "This warning will no longer be emitted",
                        nNextFID, poFieldDefn->GetNameRef());
                }
            }
        }
        else if( !poFieldDefn->IsIgnored() )
        {
            if( bEmptyStringNull && papszTokens[iAttr][0] == '\0' )
            {
                poFeature->SetFieldNull(iOGRField);
            }
            else
            {
                poFeature->SetField(iOGRField, papszTokens[iAttr]);
                if( !bWarningBadTypeOrWidth && poFieldDefn->GetWidth() > 0 &&
                    static_cast<int>(strlen(papszTokens[iAttr])) >
                        poFieldDefn->GetWidth() )
                {
                    bWarningBadTypeOrWidth = true;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Value with a width greater than field width "
                             "found in record %d for field %s. "
                             "This warning will no longer be emitted",
                             nNextFID, poFieldDefn->GetNameRef());
                }
            }
        }

        if( bKeepSourceColumns && eFieldType != OFTString )
        {
            iOGRField++;
            if( papszTokens[iAttr][0] != '\0' &&
                !poFeatureDefn->GetFieldDefn(iOGRField)->IsIgnored() )
            {
                poFeature->SetField(iOGRField, papszTokens[iAttr]);
            }
        }

        iOGRField++;
    }

    // Eurostat TSV files.

    for( int iAttr = 0; bIsEurostatTSV && iAttr < nAttrCount; iAttr++ )
    {
        if( iAttr == 0 )
        {
            char **papszDims = CSLTokenizeString2(papszTokens[0], ",", 0);
            if( CSLCount(papszDims) != nEurostatDims )
            {
                CSLDestroy(papszDims);
                break;
            }
            for( int iSubAttr = 0; iSubAttr < nEurostatDims; iSubAttr++ )
            {
                if( !poFeatureDefn->GetFieldDefn(iSubAttr)->IsIgnored() )
                    poFeature->SetField(iSubAttr, papszDims[iSubAttr]);
            }
            CSLDestroy(papszDims);
        }
        else
        {
            char **papszVals = CSLTokenizeString2(papszTokens[iAttr], " ", 0);
            eType = CPLGetValueType(papszVals[0]);
            if( (papszVals[0] && papszVals[0][0] != '\0') &&
                (eType == CPL_VALUE_INTEGER || eType == CPL_VALUE_REAL) )
            {
                if( !poFeatureDefn->GetFieldDefn(nEurostatDims + 2 * (iAttr - 1))
                         ->IsIgnored() )
                    poFeature->SetField(nEurostatDims + 2 * (iAttr - 1),
                                        papszVals[0]);
            }
            if( CSLCount(papszVals) == 2 )
            {
                if( !poFeatureDefn
                         ->GetFieldDefn(nEurostatDims + 2 * (iAttr - 1) + 1)
                         ->IsIgnored() )
                    poFeature->SetField(nEurostatDims + 2 * (iAttr - 1) + 1,
                                        papszVals[1]);
            }
            CSLDestroy(papszVals);
        }
    }

    // http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm
    // specific

    if( iNfdcLatitudeS != -1 &&
        iNfdcLongitudeS != -1 &&
        nAttrCount > iNfdcLatitudeS &&
        nAttrCount > iNfdcLongitudeS  &&
        papszTokens[iNfdcLongitudeS][0] != 0 &&
        papszTokens[iNfdcLatitudeS][0] != 0 )
    {
        const double dfLon =
            CPLAtof(papszTokens[iNfdcLongitudeS]) / 3600.0 *
            (strchr(papszTokens[iNfdcLongitudeS], 'W') ? -1.0 : 1.0);
        const double dfLat =
            CPLAtof(papszTokens[iNfdcLatitudeS]) / 3600.0 *
            (strchr(papszTokens[iNfdcLatitudeS], 'S') ? -1.0 : 1.0);
        if( !poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored() )
            poFeature->SetGeometryDirectly(new OGRPoint(dfLon, dfLat));
    }

    // GNIS specific.
    else if( iLatitudeField != -1 &&
             iLongitudeField != -1 &&
             nAttrCount > iLatitudeField &&
             nAttrCount > iLongitudeField  &&
             papszTokens[iLongitudeField][0] != 0 &&
             papszTokens[iLatitudeField][0] != 0 )
    {
        // Some records have dummy 0,0 value.
        if( papszTokens[iLongitudeField][0] != DIGIT_ZERO ||
            papszTokens[iLongitudeField][1] != '\0' ||
            papszTokens[iLatitudeField][0] != DIGIT_ZERO ||
            papszTokens[iLatitudeField][1] != '\0' )
        {
            const double dfLon = CPLAtof(papszTokens[iLongitudeField]);
            const double dfLat = CPLAtof(papszTokens[iLatitudeField]);
            if( !poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored() )
            {
                if( iZField != -1 && nAttrCount > iZField &&
                    papszTokens[iZField][0] != 0 )
                    poFeature->SetGeometryDirectly(new OGRPoint(
                        dfLon, dfLat, CPLAtof(papszTokens[iZField])));
                else
                    poFeature->SetGeometryDirectly(new OGRPoint(dfLon, dfLat));
            }
        }
    }

    CSLDestroy(papszTokens);

    // Translate the record id.
    poFeature->SetFID(nNextFID++);

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRCSVLayer::GetNextFeature()

{
    if( bNeedRewindBeforeRead )
        ResetReading();

    // Read features till we find one that satisfies our current
    // spatial criteria.
    while( true )
    {
        OGRFeature *poFeature = GetNextUnfilteredFeature();
        if( poFeature == nullptr )
            return nullptr;

        if( (m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter))) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVLayer::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap, OLCSequentialWrite) )
        return bInWriteMode && !bKeepSourceColumns && bKeepGeomColumns;
    else if( EQUAL(pszCap, OLCCreateField) )
        return bNew && !bHasFieldNames;
    else if( EQUAL(pszCap, OLCCreateGeomField) )
        return bNew && !bHasFieldNames &&
               eGeometryFormat == OGR_CSV_GEOM_AS_WKT;
    else if( EQUAL(pszCap, OLCIgnoreFields) )
        return TRUE;
    else if( EQUAL(pszCap, OLCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap, OLCMeasuredGeometries) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          PreCreateField()                            */
/************************************************************************/

OGRCSVCreateFieldAction
OGRCSVLayer::PreCreateField( OGRFeatureDefn *poFeatureDefn,
                             const std::set<CPLString>& oSetFields,
                             OGRFieldDefn *poNewField, int bApproxOK )
{
    // Does this duplicate an existing field?
    if( oSetFields.find(CPLString(poNewField->GetNameRef()).toupper()) != oSetFields.end() )
    {
        if( poFeatureDefn->GetGeomFieldIndex(poNewField->GetNameRef()) >= 0 ||
            poFeatureDefn->GetGeomFieldIndex(
                CPLSPrintf("geom_%s", poNewField->GetNameRef())) >= 0 )
        {
            return CREATE_FIELD_DO_NOTHING;
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create field %s, "
                 "but a field with this name already exists.",
                 poNewField->GetNameRef());

        return CREATE_FIELD_ERROR;
    }

    // Is this a legal field type for CSV?
    switch( poNewField->GetType() )
    {
    case OFTInteger:
    case OFTInteger64:
    case OFTReal:
    case OFTString:
    case OFTIntegerList:
    case OFTInteger64List:
    case OFTRealList:
    case OFTStringList:
    case OFTTime:
    case OFTDate:
    case OFTDateTime:
        // These types are OK.
        break;

    default:
        if( bApproxOK )
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Attempt to create field of type %s, but this is not supported "
                "for .csv files.  Just treating as a plain string.",
                poNewField->GetFieldTypeName(poNewField->GetType()));
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Attempt to create field of type %s, but this is not supported "
                "for .csv files.",
                poNewField->GetFieldTypeName(poNewField->GetType()));
            return CREATE_FIELD_ERROR;
        }
    }
    return CREATE_FIELD_PROCEED;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCSVLayer::CreateField( OGRFieldDefn *poNewField, int bApproxOK )

{
    // If we have already written our field names, then we are not
    // allowed to add new fields.
    if( !TestCapability(OLCCreateField) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create new fields after first feature written.");
        return OGRERR_FAILURE;
    }

    if( nCSVFieldCount >= 10000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Limiting to 10000 fields");
        return OGRERR_FAILURE;
    }

    if( m_oSetFields.empty() )
    {
        for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            m_oSetFields.insert(CPLString(
                poFeatureDefn->GetFieldDefn(i)->GetNameRef()).toupper());
        }
    }

    const OGRCSVCreateFieldAction eAction =
        PreCreateField(poFeatureDefn, m_oSetFields, poNewField, bApproxOK);
    if( eAction == CREATE_FIELD_DO_NOTHING )
        return OGRERR_NONE;
    if( eAction == CREATE_FIELD_ERROR )
        return OGRERR_FAILURE;

    // Seems ok, add to field list.
    poFeatureDefn->AddFieldDefn(poNewField);
    nCSVFieldCount++;
    m_oSetFields.insert(CPLString(poNewField->GetNameRef()).toupper());

    panGeomFieldIndex = static_cast<int *>(CPLRealloc(
        panGeomFieldIndex, sizeof(int) * poFeatureDefn->GetFieldCount()));
    panGeomFieldIndex[poFeatureDefn->GetFieldCount() - 1] = -1;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRCSVLayer::CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                     int /* bApproxOK */ )
{
    if( !TestCapability(OLCCreateGeomField) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create new fields after first feature written.");
        return OGRERR_FAILURE;
    }

    // Does this duplicate an existing field?
    if( poFeatureDefn->GetGeomFieldIndex(poGeomField->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create geom field %s, "
                 "but a field with this name already exists.",
                 poGeomField->GetNameRef());

        return OGRERR_FAILURE;
    }
    OGRGeomFieldDefn oGeomField(poGeomField);
    if( oGeomField.GetSpatialRef() )
    {
        oGeomField.GetSpatialRef()->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    poFeatureDefn->AddGeomFieldDefn(&oGeomField);

    const char *pszName = poGeomField->GetNameRef();
    if( EQUAL(pszName, ""))
    {
        const int nIdx = poFeatureDefn->GetFieldIndex("WKT");
        if( nIdx >= 0 )
        {
            panGeomFieldIndex[nIdx] = poFeatureDefn->GetGeomFieldCount() - 1;
            return OGRERR_NONE;
        }
        pszName = "WKT";
    }
    if( STARTS_WITH_CI(pszName, "geom_") && strlen(pszName) >= strlen("geom_") )
        pszName += strlen("geom_");
    if( !EQUAL(pszName, "WKT") && !STARTS_WITH_CI(pszName, "_WKT") )
        pszName = CPLSPrintf("_WKT%s", pszName);

    OGRFieldDefn oRegularFieldDefn(pszName, OFTString);
    poFeatureDefn->AddFieldDefn(&oRegularFieldDefn);
    nCSVFieldCount++;

    panGeomFieldIndex = static_cast<int *>(CPLRealloc(
        panGeomFieldIndex, sizeof(int) * poFeatureDefn->GetFieldCount()));
    panGeomFieldIndex[poFeatureDefn->GetFieldCount() - 1] =
        poFeatureDefn->GetGeomFieldCount() - 1;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            WriteHeader()                             */
/*                                                                      */
/*      Write the header, and possibly the .csvt file if they           */
/*      haven't already been written.                                   */
/************************************************************************/

OGRErr OGRCSVLayer::WriteHeader()
{
    if( !bNew )
        return OGRERR_NONE;

    // Write field names if we haven't written them yet.
    // Write .csvt file if needed.
    bNew = false;
    bHasFieldNames = true;
    bool bOK = true;

    for( int iFile = 0; iFile < (bCreateCSVT ? 2 : 1); iFile++ )
    {
        VSILFILE *fpCSVT = nullptr;
        if( bCreateCSVT && iFile == 0 )
        {
            char *pszDirName = CPLStrdup(CPLGetDirname(pszFilename));
            char *pszBaseName = CPLStrdup(CPLGetBasename(pszFilename));
            fpCSVT = VSIFOpenL(
                CPLFormFilename(pszDirName, pszBaseName, ".csvt"), "wb");
            CPLFree(pszDirName);
            CPLFree(pszBaseName);
        }
        else
        {
            if( STARTS_WITH(pszFilename, "/vsistdout/") ||
                STARTS_WITH(pszFilename, "/vsizip/") )
                fpCSV = VSIFOpenL(pszFilename, "wb");
            else
                fpCSV = VSIFOpenL(pszFilename, "w+b");

            if( fpCSV == nullptr )
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "Failed to create %s:\n%s",
                         pszFilename, VSIStrerror(errno));
                return OGRERR_FAILURE;
            }
        }

        if( bWriteBOM && fpCSV )
        {
            bOK &= VSIFWriteL("\xEF\xBB\xBF", 1, 3, fpCSV) > 0;
        }

        if( eGeometryFormat == OGR_CSV_GEOM_AS_XYZ )
        {
            if( fpCSV )
                bOK &=
                    VSIFPrintfL(fpCSV, "X%cY%cZ", chDelimiter, chDelimiter) > 0;
            if( fpCSVT )
                bOK &= VSIFPrintfL(fpCSVT, "%s", "CoordX,CoordY,Real") > 0;
            if( poFeatureDefn->GetFieldCount() > 0 )
            {
                if( fpCSV )
                    bOK &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
                if( fpCSVT )
                    bOK &= VSIFPrintfL(fpCSVT, "%s", ",") > 0;
            }
        }
        else if( eGeometryFormat == OGR_CSV_GEOM_AS_XY )
        {
            if( fpCSV )
                bOK &= VSIFPrintfL(fpCSV, "X%cY", chDelimiter) > 0;
            if( fpCSVT )
                bOK &= VSIFPrintfL(fpCSVT, "%s", "CoordX,CoordY") > 0;
            if( poFeatureDefn->GetFieldCount() > 0 )
            {
                if( fpCSV )
                    bOK &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
                if( fpCSVT )
                    bOK &= VSIFPrintfL(fpCSVT, "%s", ",") > 0;
            }
        }
        else if( eGeometryFormat == OGR_CSV_GEOM_AS_YX )
        {
            if( fpCSV )
                bOK &= VSIFPrintfL(fpCSV, "Y%cX", chDelimiter) > 0;
            if( fpCSVT )
                bOK &= VSIFPrintfL(fpCSVT, "%s", "CoordY,CoordX") > 0;
            if( poFeatureDefn->GetFieldCount() > 0 )
            {
                if( fpCSV )
                    bOK &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
                if(fpCSVT)
                    bOK &= VSIFPrintfL(fpCSVT, "%s", ",") > 0;
            }
        }

        if( bHiddenWKTColumn )
        {
            if( fpCSV )
            {
                const char* pszColName =
                    bCreateCSVT ? poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef() : "WKT";
                bOK &=
                    VSIFPrintfL(fpCSV, "%s", pszColName) >= 0;
            }
            if( fpCSVT )
                bOK &= VSIFPrintfL(fpCSVT, "%s", "WKT") > 0;
        }

        for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            if( iField > 0 || bHiddenWKTColumn )
            {
                if( fpCSV )
                    bOK &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
                if( fpCSVT )
                    bOK &= VSIFPrintfL(fpCSVT, "%s", ",") > 0;
            }

            char *pszEscaped = CPLEscapeString(
                poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), -1,
                m_eStringQuoting == StringQuoting::ALWAYS ?
                    CPLES_CSV_FORCE_QUOTING : CPLES_CSV);
            if( pszEscaped == nullptr )
                return OGRERR_FAILURE;

            if( fpCSV )
            {
                bool bAddDoubleQuote = false;
                if( chDelimiter == ' ' && pszEscaped[0] != '"' &&
                    strchr(pszEscaped, ' ') != nullptr )
                    bAddDoubleQuote = true;
                if( bAddDoubleQuote )
                    bOK &= VSIFWriteL("\"", 1, 1, fpCSV) > 0;
                bOK &= VSIFPrintfL(fpCSV, "%s", pszEscaped) >= 0;
                if( bAddDoubleQuote )
                    bOK &= VSIFWriteL("\"", 1, 1, fpCSV) > 0;
            }
            CPLFree(pszEscaped);

            if( fpCSVT )
            {
                int nWidth = poFeatureDefn->GetFieldDefn(iField)->GetWidth();
                const int nPrecision =
                    poFeatureDefn->GetFieldDefn(iField)->GetPrecision();

                switch( poFeatureDefn->GetFieldDefn(iField)->GetType() )
                {
                case OFTInteger:
                {
                    if( poFeatureDefn->GetFieldDefn(iField)->GetSubType() ==
                        OFSTBoolean )
                    {
                        nWidth = 0;
                        bOK &=
                            VSIFPrintfL(fpCSVT, "%s", "Integer(Boolean)") > 0;
                    }
                    else if( poFeatureDefn->GetFieldDefn(iField)->
                                 GetSubType() == OFSTInt16 )
                    {
                        nWidth = 0;
                        bOK &= VSIFPrintfL(fpCSVT, "%s", "Integer(Int16)") > 0;
                    }
                    else
                    {
                        bOK &= VSIFPrintfL(fpCSVT, "%s", "Integer") > 0;
                    }
                    break;
                }
                case OFTInteger64:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "Integer64") > 0;
                    break;
                case OFTReal:
                {
                    if( poFeatureDefn->GetFieldDefn(iField)->GetSubType() ==
                        OFSTFloat32 )
                    {
                        nWidth = 0;
                        bOK &= VSIFPrintfL(fpCSVT, "%s", "Real(Float32)") > 0;
                    }
                    else
                    {
                        bOK &= VSIFPrintfL(fpCSVT, "%s", "Real") > 0;
                    }
                    break;
                }
                case OFTDate:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "Date") > 0;
                    break;
                case OFTTime:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "Time") > 0;
                    break;
                case OFTDateTime:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "DateTime") > 0;
                    break;
                case OFTStringList:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "JSonStringList") > 0;
                    break;
                case OFTIntegerList:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "JSonIntegerList") > 0;
                    break;
                case OFTInteger64List:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "JSonInteger64List") > 0;
                    break;
                case OFTRealList:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "JSonRealList") > 0;
                    break;
                default:
                    bOK &= VSIFPrintfL(fpCSVT, "%s", "String") > 0;
                    break;
                }

                if( nWidth != 0 )
                {
                    if( nPrecision != 0 )
                        bOK &= VSIFPrintfL(fpCSVT, "(%d.%d)", nWidth,
                                           nPrecision) > 0;
                    else
                        bOK &= VSIFPrintfL(fpCSVT, "(%d)", nWidth) > 0;
                }
            }
        }

        // The CSV driver will not recognize single column tables, so add
        // a fake second blank field.
        if( (poFeatureDefn->GetFieldCount() == 1 && !bHiddenWKTColumn) ||
            (poFeatureDefn->GetFieldCount() == 0 && bHiddenWKTColumn) )
        {
            if( fpCSV )
                bOK &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
        }

        if( bUseCRLF )
        {
            if( fpCSV )
                bOK &= VSIFPutcL(13, fpCSV) > 0;
            if(fpCSVT)
                bOK &= VSIFPutcL(13, fpCSVT) > 0;
        }
        if( fpCSV )
            bOK &= VSIFPutcL('\n', fpCSV) > 0;
        if( fpCSVT )
            bOK &= VSIFPutcL('\n', fpCSVT) > 0;
        if( fpCSVT )
            VSIFCloseL(fpCSVT);
    }

    return (!bOK || fpCSV == nullptr) ? OGRERR_FAILURE : OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRCSVLayer::ICreateFeature( OGRFeature *poNewFeature )

{
    if( !bInWriteMode )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The CreateFeature() operation is not permitted on a "
                 "read-only CSV.");
        return OGRERR_FAILURE;
    }

    // If we need rewind, it means that we have just written a feature before
    // so there's no point seeking to the end of the file, as we're already
    // at the end.
    bool bNeedSeekEnd = !bNeedRewindBeforeRead;

    bNeedRewindBeforeRead = true;

    // Write field names if we haven't written them yet.
    // Write .csvt file if needed.
    if( bNew )
    {
        const OGRErr eErr = WriteHeader();
        if( eErr != OGRERR_NONE )
            return eErr;
        bNeedSeekEnd = false;
    }

    if( fpCSV == nullptr )
        return OGRERR_FAILURE;

    bool bRet = true;

    // Make sure we are at the end of the file.
    if( bNeedSeekEnd )
    {
        if( bFirstFeatureAppendedDuringSession )
        {
            // Add a newline character to the end of the file if necessary.
            bFirstFeatureAppendedDuringSession = false;
            bRet &= VSIFSeekL(fpCSV, 0, SEEK_END) >= 0;
            bRet &= VSIFSeekL(fpCSV, VSIFTellL(fpCSV) - 1, SEEK_SET) >= 0;
            char chLast = '\0';
            bRet &= VSIFReadL(&chLast, 1, 1, fpCSV) > 0;
            bRet &= VSIFSeekL(fpCSV, 0, SEEK_END) >= 0;
            if( chLast != '\n' )
            {
                if( bUseCRLF )
                    bRet &= VSIFPutcL(13, fpCSV) != EOF;
                bRet &= VSIFPutcL('\n', fpCSV) != EOF;
            }
        }
        else
        {
            bRet &= VSIFSeekL(fpCSV, 0, SEEK_END) >= 0;
        }
    }

    // Write out the geometry.
    if( eGeometryFormat == OGR_CSV_GEOM_AS_XYZ ||
        eGeometryFormat == OGR_CSV_GEOM_AS_XY ||
        eGeometryFormat == OGR_CSV_GEOM_AS_YX )
    {
        OGRGeometry *poGeom = poNewFeature->GetGeometryRef();
        if( poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            OGRPoint *poPoint = poGeom->toPoint();
            char szBuffer[75] = {};
            if( eGeometryFormat == OGR_CSV_GEOM_AS_XYZ )
                OGRMakeWktCoordinate(szBuffer, poPoint->getX(), poPoint->getY(),
                                     poPoint->getZ(), 3);
            else if( eGeometryFormat == OGR_CSV_GEOM_AS_XY )
                OGRMakeWktCoordinate(szBuffer, poPoint->getX(), poPoint->getY(),
                                     0, 2);
            else
                OGRMakeWktCoordinate(szBuffer, poPoint->getY(), poPoint->getX(),
                                     0, 2);
            char *pc = szBuffer;
            while(*pc != '\0')
            {
                if( *pc == ' ' )
                    *pc = chDelimiter;
                pc++;
            }
            bRet &= VSIFPrintfL(fpCSV, "%s", szBuffer) > 0;
        }
        else
        {
            bRet &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
            if( eGeometryFormat == OGR_CSV_GEOM_AS_XYZ )
                bRet &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
        }
        if( poFeatureDefn->GetFieldCount() > 0 )
            bRet &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;
    }

    // Special case to deal with hidden "WKT" geometry column.
    bool bNonEmptyLine = false;

    if( bHiddenWKTColumn )
    {
        char *pszWKT = nullptr;
        OGRGeometry *poGeom = poNewFeature->GetGeomFieldRef(0);
        if( poGeom &&
            poGeom->exportToWkt(&pszWKT, wkbVariantIso) == OGRERR_NONE )
        {
            bNonEmptyLine = true;
            bRet &= VSIFWriteL("\"", 1, 1, fpCSV) > 0;
            bRet &= VSIFWriteL(pszWKT, strlen(pszWKT), 1, fpCSV) > 0;
            bRet &= VSIFWriteL("\"", 1, 1, fpCSV) > 0;
        }
        CPLFree(pszWKT);
    }

    // Write out all the field values.
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        char *pszEscaped = nullptr;

        if( iField > 0 || bHiddenWKTColumn )
            bRet &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;

        if( eGeometryFormat == OGR_CSV_GEOM_AS_WKT &&
            panGeomFieldIndex[iField] >= 0 )
        {
            const int iGeom = panGeomFieldIndex[iField];
            OGRGeometry *poGeom = poNewFeature->GetGeomFieldRef(iGeom);
            if( poGeom &&
                poGeom->exportToWkt(&pszEscaped, wkbVariantIso) == OGRERR_NONE )
            {
                const int nLenWKT = static_cast<int>(strlen(pszEscaped));
                char *pszNew =
                    static_cast<char *>(CPLMalloc(1 + nLenWKT + 1 + 1));
                pszNew[0] = '"';
                memcpy(pszNew + 1, pszEscaped, nLenWKT);
                pszNew[1 + nLenWKT] = '"';
                pszNew[1 + nLenWKT + 1] = '\0';
                CPLFree(pszEscaped);
                pszEscaped = pszNew;
            }
            else
            {
                CPLFree(pszEscaped);
                pszEscaped = CPLStrdup("");
            }
        }
        else
        {
            const OGRFieldType eType(
                poFeatureDefn->GetFieldDefn(iField)->GetType());
            if( eType == OFTReal )
            {
                if( poFeatureDefn->GetFieldDefn(iField)->GetSubType() ==
                        OFSTFloat32 &&
                    poNewFeature->IsFieldSetAndNotNull(iField) )
                {
                    pszEscaped = CPLStrdup(CPLSPrintf(
                        "%.8g", poNewFeature->GetFieldAsDouble(iField)));
                }
                else
                {
                    pszEscaped =
                        CPLStrdup(poNewFeature->GetFieldAsString(iField));
                }
            }
            else if( eType == OFTStringList || eType == OFTIntegerList ||
                     eType == OFTInteger64List || eType == OFTRealList )
            {
                char *pszJSon = poNewFeature->GetFieldAsSerializedJSon(iField);
                if( pszJSon )
                {
                    pszEscaped = CPLEscapeString(pszJSon, -1,
                        m_eStringQuoting == StringQuoting::ALWAYS ?
                            CPLES_CSV_FORCE_QUOTING : CPLES_CSV);
                }
                else
                {
                    pszEscaped = CPLStrdup("");
                }
                CPLFree(pszJSon);
            }
            else
            {
                const char* pszContent = poNewFeature->GetFieldAsString(iField);
                pszEscaped = CPLEscapeString(
                    pszContent, -1,
                    (m_eStringQuoting == StringQuoting::ALWAYS ||
                     (m_eStringQuoting == StringQuoting::IF_AMBIGUOUS &&
                      CPLGetValueType(pszContent) != CPL_VALUE_STRING)) ?
                            CPLES_CSV_FORCE_QUOTING : CPLES_CSV);
            }
        }
        if( pszEscaped == nullptr )
        {
            return OGRERR_FAILURE;
        }
        const size_t nLen = strlen(pszEscaped);
        bNonEmptyLine |= nLen != 0;
        bool bAddDoubleQuote = false;
        if( chDelimiter == ' ' && pszEscaped[0] != '"' &&
            strchr(pszEscaped, ' ') != nullptr )
            bAddDoubleQuote = true;
        if( bAddDoubleQuote )
            bRet &= VSIFWriteL("\"", 1, 1, fpCSV) > 0;
        if( nLen )
            bRet &= VSIFWriteL(pszEscaped, nLen, 1, fpCSV) > 0;
        if( bAddDoubleQuote )
            bRet &= VSIFWriteL("\"", 1, 1, fpCSV) > 0;
        CPLFree(pszEscaped);
    }

    if( (poFeatureDefn->GetFieldCount() == 1 ||
         (poFeatureDefn->GetFieldCount() == 0 && bHiddenWKTColumn)) &&
        !bNonEmptyLine )
        bRet &= VSIFPrintfL(fpCSV, "%c", chDelimiter) > 0;

    if( bUseCRLF )
        bRet &= VSIFPutcL(13, fpCSV) != EOF;
    bRet &= VSIFPutcL('\n', fpCSV) != EOF;

    if( nTotalFeatures >= 0 )
        nTotalFeatures++;

    return bRet ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                              SetCRLF()                               */
/************************************************************************/

void OGRCSVLayer::SetCRLF(bool bNewValue) { bUseCRLF = bNewValue; }

/************************************************************************/
/*                       SetWriteGeometry()                             */
/************************************************************************/

void OGRCSVLayer::SetWriteGeometry(OGRwkbGeometryType eGType,
                                   OGRCSVGeometryFormat eGeometryFormatIn,
                                   const char *pszGeomCol)
{
    eGeometryFormat = eGeometryFormatIn;
    if( eGeometryFormat == OGR_CSV_GEOM_AS_WKT && eGType != wkbNone )
    {
        OGRGeomFieldDefn oGFld(pszGeomCol, eGType);
        bHiddenWKTColumn = true;
        // We don't use CreateGeomField() since we don't want to generate
        // a geometry field in first position, as it confuses applications
        // (such as MapServer <= 6.4) that assume that the first regular field
        // they add will be at index 0.
        poFeatureDefn->AddGeomFieldDefn(&oGFld);
    }
    else
    {
        poFeatureDefn->SetGeomType(eGType);
    }
}

/************************************************************************/
/*                          SetCreateCSVT()                             */
/************************************************************************/

void OGRCSVLayer::SetCreateCSVT( bool bCreateCSVTIn )
{
    bCreateCSVT = bCreateCSVTIn;
}

/************************************************************************/
/*                          SetWriteBOM()                               */
/************************************************************************/

void OGRCSVLayer::SetWriteBOM( bool bWriteBOMIn ) { bWriteBOM = bWriteBOMIn; }

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRCSVLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != nullptr || m_poAttrQuery != nullptr )
    {
        GIntBig nRet = OGRLayer::GetFeatureCount(bForce);
        if( nRet >= 0 )
        {
            nTotalFeatures = nNextFID - 1;
        }
        return nRet;
    }

    if( nTotalFeatures >= 0 )
        return nTotalFeatures;

    if( fpCSV == nullptr )
        return 0;

    ResetReading();

    if( chDelimiter == '\t' && bDontHonourStrings )
    {
        const int nBufSize = 4096;
        char szBuffer[nBufSize + 1] = {};

        nTotalFeatures = 0;
        bool bLastWasNewLine = false;
        while( true )
        {
            const int nRead =
                static_cast<int>(VSIFReadL(szBuffer, 1, nBufSize, fpCSV));
            szBuffer[nRead] = 0;
            if( nTotalFeatures == 0 && szBuffer[0] != 13 && szBuffer[0] != 10 )
                nTotalFeatures = 1;
            for( int i = 0; i < nRead; i++ )
            {
                if( szBuffer[i] == 13 || szBuffer[i] == 10 )
                {
                    bLastWasNewLine = true;
                }
                else if( bLastWasNewLine )
                {
                    nTotalFeatures++;
                    bLastWasNewLine = false;
                }
            }

            if( nRead < nBufSize )
                break;
        }
    }
    else
    {
        nTotalFeatures = 0;
        while( true )
        {
            char **papszTokens = GetNextLineTokens();
            if( papszTokens == nullptr )
                break;

            nTotalFeatures++;

            CSLDestroy(papszTokens);
        }
    }

    ResetReading();

    return nTotalFeatures;
}

/************************************************************************/
/*                          SyncToDisk()                                */
/************************************************************************/

OGRErr OGRCSVLayer::SyncToDisk()
{
    if( bInWriteMode && fpCSV != nullptr )
    {
        if( VSIFFlushL(fpCSV) != 0 )
            return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}
