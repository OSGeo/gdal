/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVLayer class.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_csv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");



/************************************************************************/
/*                            CSVSplitLine()                            */
/*                                                                      */
/*      Tokenize a CSV line into fields in the form of a string         */
/*      list.  This is used instead of the CPLTokenizeString()          */
/*      because it provides correct CSV escaping and quoting            */
/*      semantics.                                                      */
/************************************************************************/

static char **CSVSplitLine( const char *pszString, char chDelimiter,
                            int bKeepLeadingAndClosingQuotes,
                            int bMergeDelimiter )

{
    char        **papszRetList = NULL;
    char        *pszToken;
    int         nTokenMax, nTokenLen;

    pszToken = (char *) CPLCalloc(10,1);
    nTokenMax = 10;

    while( pszString != NULL && *pszString != '\0' )
    {
        int     bInString = FALSE;

        nTokenLen = 0;

        /* Try to find the next delimeter, marking end of token */
        for( ; *pszString != '\0'; pszString++ )
        {

            /* End if this is a delimeter skip it and break. */
            if( !bInString && *pszString == chDelimiter )
            {
                pszString++;
                if( bMergeDelimiter )
                {
                    while( *pszString == chDelimiter )
                        pszString ++;
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
                else  /* doubled quotes in string resolve to one quote */
                {
                    pszString++;
                }
            }

            if( nTokenLen >= nTokenMax-2 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = (char *) CPLRealloc( pszToken, nTokenMax );
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';
        papszRetList = CSLAddString( papszRetList, pszToken );

        /* If the last token is an empty token, then we have to catch
         * it now, otherwise we won't reenter the loop and it will be lost.
         */
        if ( *pszString == '\0' && *(pszString-1) == chDelimiter )
        {
            papszRetList = CSLAddString( papszRetList, "" );
        }
    }

    if( papszRetList == NULL )
        papszRetList = (char **) CPLCalloc(sizeof(char *),1);

    CPLFree( pszToken );

    return papszRetList;
}

/************************************************************************/
/*                      OGRCSVReadParseLineL()                          */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/************************************************************************/

char **OGRCSVReadParseLineL( VSILFILE * fp, char chDelimiter,
                             int bDontHonourStrings,
                             int bKeepLeadingAndClosingQuotes,
                             int bMergeDelimiter )

{
    const char  *pszLine;
    char        *pszWorkLine;
    char        **papszReturn;

    pszLine = CPLReadLineL( fp );
    if( pszLine == NULL )
        return( NULL );

    /* Skip BOM */
    GByte* pabyData = (GByte*) pszLine;
    if (pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF)
        pszLine += 3;

    /* Special fix to read NdfcFacilities.xls that has non-balanced double quotes */
    if (chDelimiter == '\t' && bDontHonourStrings)
    {
        return CSLTokenizeStringComplex(pszLine, "\t", FALSE, TRUE);
    }

/* -------------------------------------------------------------------- */
/*      If there are no quotes, then this is the simple case.           */
/*      Parse, and return tokens.                                       */
/* -------------------------------------------------------------------- */
    if( strchr(pszLine,'\"') == NULL )
        return CSVSplitLine( pszLine, chDelimiter, bKeepLeadingAndClosingQuotes,
                             bMergeDelimiter );

/* -------------------------------------------------------------------- */
/*      We must now count the quotes in our working string, and as      */
/*      long as it is odd, keep adding new lines.                       */
/* -------------------------------------------------------------------- */
    pszWorkLine = CPLStrdup( pszLine );

    int i = 0, nCount = 0;
    int nWorkLineLength = strlen(pszWorkLine);

    while( TRUE )
    {
        for( ; pszWorkLine[i] != '\0'; i++ )
        {
            if( pszWorkLine[i] == '\"' )
                nCount++;
        }

        if( nCount % 2 == 0 )
            break;

        pszLine = CPLReadLineL( fp );
        if( pszLine == NULL )
            break;

        int nLineLen = strlen(pszLine);

        char* pszWorkLineTmp = (char *)
            VSIRealloc(pszWorkLine,
                       nWorkLineLength + nLineLen + 2);
        if (pszWorkLineTmp == NULL)
            break;
        pszWorkLine = pszWorkLineTmp;
        strcat( pszWorkLine + nWorkLineLength, "\n" ); // This gets lost in CPLReadLine().
        strcat( pszWorkLine + nWorkLineLength, pszLine );

        nWorkLineLength += nLineLen + 1;
    }

    papszReturn = CSVSplitLine( pszWorkLine, chDelimiter,
                                bKeepLeadingAndClosingQuotes, bMergeDelimiter );

    CPLFree( pszWorkLine );

    return papszReturn;
}

/************************************************************************/
/*                            OGRCSVLayer()                             */
/*                                                                      */
/*      Note that the OGRCSVLayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/

OGRCSVLayer::OGRCSVLayer( const char *pszLayerNameIn, 
                          VSILFILE * fp, const char *pszFilename,
                          int bNew, int bInWriteMode,
                          char chDelimiter )

{
    fpCSV = fp;

    nCSVFieldCount = 0;
    panGeomFieldIndex = NULL;
    iNfdcLatitudeS = iNfdcLongitudeS = -1;
    iLatitudeField = iLongitudeField = -1;
    iZField = -1;
    bHasFieldNames = FALSE;
    this->bInWriteMode = bInWriteMode;
    this->bNew = bNew;
    this->pszFilename = CPLStrdup(pszFilename);
    this->chDelimiter = chDelimiter;

    bFirstFeatureAppendedDuringSession = TRUE;
    bHiddenWKTColumn = FALSE;
    bUseCRLF = FALSE;
    bNeedRewindBeforeRead = FALSE;
    eGeometryFormat = OGR_CSV_GEOM_NONE;

    nNextFID = 1;

    poFeatureDefn = new OGRFeatureDefn( pszLayerNameIn );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    bCreateCSVT = FALSE;
    bDontHonourStrings = FALSE;
    bWriteBOM = FALSE;

    bIsEurostatTSV = FALSE;
    nEurostatDims = 0;

    nTotalFeatures = -1;
    bWarningBadTypeOrWidth = FALSE;
    bKeepSourceColumns = FALSE;
    bKeepGeomColumns = TRUE;
    
    bMergeDelimiter = FALSE;
}

/************************************************************************/
/*                             Matches()                                */
/************************************************************************/

int OGRCSVLayer::Matches(const char* pszFieldName, char** papszPossibleNames)
{
    if( papszPossibleNames == NULL )
        return FALSE;
    for(char** papszIter = papszPossibleNames; *papszIter; papszIter++)
    {
        const char* pszPattern = *papszIter;
        const char* pszStar = strstr(pszPattern, "*");
        if( pszStar == NULL )
        {
            if( EQUAL(pszFieldName, pszPattern) )
                return TRUE;
        }
        else
        {
            if( pszStar == pszPattern )
            {
                if( strlen(pszPattern) >= 3 &&
                    pszPattern[strlen(pszPattern)-1] == '*' )
                {
                    // *pattern*
                    CPLString oPattern(pszPattern+1);
                    oPattern.resize(oPattern.size()-1);
                    if( CPLString(pszFieldName).ifind(oPattern) != std::string::npos )
                        return TRUE;
                }
                else
                {
                    // *pattern
                    if( strlen(pszFieldName) >= strlen(pszPattern)-1 &&
                        EQUAL(pszFieldName + strlen(pszFieldName) - (strlen(pszPattern)-1), pszPattern+1) )
                    {
                        return TRUE;
                    }
                }
            }
            else if( pszPattern[strlen(pszPattern)-1] == '*' )
            {
                // pattern*
                if( EQUALN(pszFieldName, pszPattern, strlen(pszPattern)-1) )
                    return TRUE;
            }
        }
    }
    return FALSE;
}

/************************************************************************/
/*                      BuildFeatureDefn()                              */
/************************************************************************/

void OGRCSVLayer::BuildFeatureDefn( const char* pszNfdcGeomField,
                                    const char* pszGeonamesGeomFieldPrefix,
                                    char** papszOpenOptions )
{

    bMergeDelimiter = CSLFetchBoolean(papszOpenOptions, "MERGE_SEPARATOR", FALSE);

/* -------------------------------------------------------------------- */
/*      If this is not a new file, read ahead to establish if it is     */
/*      already in CRLF (DOS) mode, or just a normal unix CR mode.      */
/* -------------------------------------------------------------------- */
    if( !bNew && bInWriteMode )
    {
        int nBytesRead = 0;
        char chNewByte;

        while( nBytesRead < 10000 && VSIFReadL( &chNewByte, 1, 1, fpCSV ) == 1 )
        {
            if( chNewByte == 13 )
            {
                bUseCRLF = TRUE;
                break;
            }
            nBytesRead ++;
        }
        VSIRewindL( fpCSV );
    }

/* -------------------------------------------------------------------- */
/*      Check if the first record seems to be field definitions or      */
/*      not.  We assume it is field definitions if the HEADERS option   */
/*      not supplied and none of the values are strictly numeric.       */
/* -------------------------------------------------------------------- */
    char **papszTokens = NULL;
    int nFieldCount=0, iField;

    if( !bNew )
    {
        const char *pszLine = NULL;
        char szDelimiter[2];
        szDelimiter[0] = chDelimiter; szDelimiter[1] = '\0';

        pszLine = CPLReadLineL( fpCSV );
        if ( pszLine != NULL )
        {
            /* Detect and remove UTF-8 BOM marker if found (#4623) */
            if (pszLine[0] == (char)0xEF &&
                pszLine[1] == (char)0xBB &&
                pszLine[2] == (char)0xBF)
            {
                pszLine += 3;
            }

            /* tokenize the strings and preserve quotes, so we can separate string from numeric */
            /* this is only used in the test for bHasFieldNames (bug #4361) */
            papszTokens = CSLTokenizeString2( pszLine, szDelimiter, 
                                              (CSLT_HONOURSTRINGS |
                                               CSLT_ALLOWEMPTYTOKENS |
                                               CSLT_PRESERVEQUOTES) );
            nFieldCount = CSLCount( papszTokens );

            const char* pszCSVHeaders = CSLFetchNameValueDef(papszOpenOptions, "HEADERS", "AUTO");
            if (EQUAL(pszCSVHeaders, "YES"))
                bHasFieldNames = TRUE;
            else if (EQUAL(pszCSVHeaders, "NO"))
                bHasFieldNames = FALSE;
            else {
                // detect via checking for the presence of numeric values.
                bHasFieldNames = TRUE;
                for( iField = 0; iField < nFieldCount && bHasFieldNames; iField++ )
                {
                    CPLValueType eType = CPLGetValueType(papszTokens[iField]);
                    if ( (eType == CPL_VALUE_INTEGER ||
                          eType == CPL_VALUE_REAL) ) {
                        /* we have a numeric field, therefore do not consider the first line as field names */
                        bHasFieldNames = FALSE;
                    }
                }

                CPLString osExt = OGRCSVDataSource::GetRealExtension(pszFilename);

                /* Eurostat .tsv files */
                if( EQUAL(osExt, "tsv") && nFieldCount > 1 &&
                    strchr(papszTokens[0], ',') != NULL && strchr(papszTokens[0], '\\') != NULL )
                {
                    bHasFieldNames = TRUE;
                    bIsEurostatTSV = TRUE;
                }
            }

            /* tokenize without quotes to get the actual values */
            CSLDestroy( papszTokens );
            // papszTokens = OGRCSVReadParseLineL( fpCSV, chDelimiter, FALSE );   
            int nFlags = CSLT_HONOURSTRINGS;
            if( !bMergeDelimiter )
                nFlags |= CSLT_ALLOWEMPTYTOKENS;
            papszTokens = CSLTokenizeString2( pszLine, szDelimiter,  nFlags );
            nFieldCount = CSLCount( papszTokens );
        }
    }
    else
        bHasFieldNames = FALSE;

    if( !bNew )
        ResetReading();
 
    nCSVFieldCount = nFieldCount;
    
    panGeomFieldIndex = (int*) CPLCalloc(nFieldCount, sizeof(int));
    for( iField = 0; iField < nFieldCount; iField++ )
    {
        panGeomFieldIndex[iField] = -1;
    }

/* -------------------------------------------------------------------- */
/*      Check for geonames.org tables                                   */
/* -------------------------------------------------------------------- */
    if( !bHasFieldNames && nFieldCount == 19 )
    {
        if (CPLGetValueType(papszTokens[0]) == CPL_VALUE_INTEGER &&
            CPLGetValueType(papszTokens[4]) == CPL_VALUE_REAL &&
            CPLGetValueType(papszTokens[5]) == CPL_VALUE_REAL &&
            CPLAtof(papszTokens[4]) >= -90 && CPLAtof(papszTokens[4]) <= 90 &&
            CPLAtof(papszTokens[5]) >= -180 && CPLAtof(papszTokens[4]) <= 180)
        {
            bHasFieldNames = TRUE;
            CSLDestroy(papszTokens);
            papszTokens = NULL;

            static const struct {
                const char* pszName;
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
            for(iField = 0; iField < nFieldCount; iField++)
            {
                OGRFieldDefn oFieldDefn(asGeonamesFieldDesc[iField].pszName,
                                        asGeonamesFieldDesc[iField].eType);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }

            iLatitudeField = 4;
            iLongitudeField = 5;

            nFieldCount = 0;
        }
    }


/* -------------------------------------------------------------------- */
/*      Search a csvt file for types                                */
/* -------------------------------------------------------------------- */
    char** papszFieldTypes = NULL;
    if (!bNew) {
        char* dname = CPLStrdup(CPLGetDirname(pszFilename));
        char* fname = CPLStrdup(CPLGetBasename(pszFilename));
        VSILFILE* fpCSVT = VSIFOpenL(CPLFormFilename(dname, fname, ".csvt"), "r");
        CPLFree(dname);
        CPLFree(fname);
        if (fpCSVT!=NULL) {
            VSIRewindL(fpCSVT);
            papszFieldTypes = OGRCSVReadParseLineL(fpCSVT, ',', FALSE,FALSE);
            VSIFCloseL(fpCSVT);
        }
    }

/* -------------------------------------------------------------------- */
/*      Optionaly auto-detect types                                     */
/* -------------------------------------------------------------------- */
    if( !bNew && papszFieldTypes == NULL &&
        CSLTestBoolean(CSLFetchNameValueDef(papszOpenOptions,
                                                     "AUTODETECT_TYPE", "NO")) )
    {
        papszFieldTypes = AutodetectFieldTypes(papszOpenOptions, nFieldCount);
        if( papszFieldTypes != NULL )
        {
            bKeepSourceColumns = CSLTestBoolean(CSLFetchNameValueDef(papszOpenOptions,
                                                     "KEEP_SOURCE_COLUMNS", "NO"));
        }
    }
    
    char** papszGeomPossibleNames =
         CSLTokenizeString2(CSLFetchNameValue(papszOpenOptions, "GEOM_POSSIBLE_NAMES"), ",", 0);
    char** papszXPossibleNames =
         CSLTokenizeString2(CSLFetchNameValue(papszOpenOptions, "X_POSSIBLE_NAMES"), ",", 0);
    char** papszYPossibleNames =
         CSLTokenizeString2(CSLFetchNameValue(papszOpenOptions, "Y_POSSIBLE_NAMES"), ",", 0);
    char** papszZPossibleNames =
         CSLTokenizeString2(CSLFetchNameValue(papszOpenOptions, "Z_POSSIBLE_NAMES"), ",", 0);
    bKeepGeomColumns = CSLTestBoolean(CSLFetchNameValueDef(papszOpenOptions,
                                                     "KEEP_GEOM_COLUMNS", "YES"));

/* -------------------------------------------------------------------- */
/*      Build field definitions.                                        */
/* -------------------------------------------------------------------- */
    for( iField = 0; !bIsEurostatTSV && iField < nFieldCount; iField++ )
    {
        char *pszFieldName = NULL;
        char szFieldNameBuffer[100];

        if( bHasFieldNames )
        {
            pszFieldName = papszTokens[iField];

            // trim white space. 
            while( *pszFieldName == ' ' )
                pszFieldName++;

            while( pszFieldName[0] != '\0' 
                && pszFieldName[strlen(pszFieldName)-1] == ' ' )
                pszFieldName[strlen(pszFieldName)-1] = '\0';

            if (*pszFieldName == '\0')
                pszFieldName = NULL;
        }

        if (pszFieldName == NULL)
        {
            /* Re-read single column CSV files that have a trailing comma */
            /* in the header line */
            if( iField == 1 && nFieldCount == 2 && papszTokens[1][0] == '\0' )
            {
                nCSVFieldCount = nFieldCount = 1;
                break;
            }
            pszFieldName = szFieldNameBuffer;
            sprintf( szFieldNameBuffer, "field_%d", iField+1 );
        }

        OGRFieldDefn oField(pszFieldName, OFTString);
        if (papszFieldTypes!=NULL && iField<CSLCount(papszFieldTypes)) {
            if( EQUAL(papszFieldTypes[iField], "WKT") )
            {
                eGeometryFormat = OGR_CSV_GEOM_AS_WKT;
                const char* pszFieldName = oField.GetNameRef();
                panGeomFieldIndex[iField] = poFeatureDefn->GetGeomFieldCount();
                OGRGeomFieldDefn oGeomFieldDefn(pszFieldName, wkbUnknown );
                poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn( &oField );
                continue;
            }
            else if( EQUAL(papszFieldTypes[iField], "CoordX") || EQUAL(papszFieldTypes[iField], "Point(X)") )
            {
                oField.SetType(OFTReal);
                iLongitudeField = iField;
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn( &oField );
                continue;
            }
            else if( EQUAL(papszFieldTypes[iField], "CoordY") || EQUAL(papszFieldTypes[iField], "Point(Y)") )
            {
                oField.SetType(OFTReal);
                iLatitudeField = iField;
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn( &oField );
                continue;
            }
            else if( EQUAL(papszFieldTypes[iField], "CoordZ") || EQUAL(papszFieldTypes[iField], "Point(Z)") )
            {
                oField.SetType(OFTReal);
                iZField = iField;
                if( bKeepGeomColumns )
                    poFeatureDefn->AddFieldDefn( &oField );
                continue;
            }
            else if (EQUAL(papszFieldTypes[iField], "Integer(Boolean)"))
            {
                oField.SetType(OFTInteger);
                oField.SetSubType(OFSTBoolean);
                oField.SetWidth(1);
            }
            else if (EQUAL(papszFieldTypes[iField], "Integer(Int16)"))
            {
                oField.SetType(OFTInteger);
                oField.SetSubType(OFSTInt16);
            }
            else if (EQUAL(papszFieldTypes[iField], "Real(Float32)"))
            {
                oField.SetType(OFTReal);
                oField.SetSubType(OFSTFloat32);
            }
            else
            {
                char* pszLeftParenthesis = strchr(papszFieldTypes[iField], '(');
                if (pszLeftParenthesis && pszLeftParenthesis != papszFieldTypes[iField] &&
                    pszLeftParenthesis[1] >= '0' && pszLeftParenthesis[1] <= '9')
                {
                    int nWidth = 0;
                    int nPrecision = 0;

                    char* pszDot = strchr(pszLeftParenthesis, '.');
                    if (pszDot) *pszDot = 0;
                    *pszLeftParenthesis = 0;

                    if (pszLeftParenthesis[-1] == ' ')
                        pszLeftParenthesis[-1] = 0;

                    nWidth = atoi(pszLeftParenthesis+1);
                    if (pszDot)
                        nPrecision = atoi(pszDot+1);

                    oField.SetWidth(nWidth);
                    oField.SetPrecision(nPrecision);
                }

                if (EQUAL(papszFieldTypes[iField], "Integer"))
                    oField.SetType(OFTInteger);
                else if (EQUAL(papszFieldTypes[iField], "Integer64"))
                    oField.SetType(OFTInteger64);
                else if (EQUAL(papszFieldTypes[iField], "Real"))
                    oField.SetType(OFTReal);
                else if (EQUAL(papszFieldTypes[iField], "String"))
                    oField.SetType(OFTString);
                else if (EQUAL(papszFieldTypes[iField], "Date"))
                    oField.SetType(OFTDate); 
                else if (EQUAL(papszFieldTypes[iField], "Time"))
                    oField.SetType(OFTTime);
                else if (EQUAL(papszFieldTypes[iField], "DateTime"))
                    oField.SetType(OFTDateTime);
                else
                    CPLError(CE_Warning, CPLE_NotSupported, "Unknown type : %s", papszFieldTypes[iField]);
            }
        }

        if( (EQUAL(oField.GetNameRef(),"WKT") ||
             EQUALN(oField.GetNameRef(),"_WKT", 4) )
            && oField.GetType() == OFTString )
        {
            eGeometryFormat = OGR_CSV_GEOM_AS_WKT;

            const char* pszFieldName = oField.GetNameRef();
            panGeomFieldIndex[iField] = poFeatureDefn->GetGeomFieldCount();
            OGRGeomFieldDefn oGeomFieldDefn(
                EQUAL(pszFieldName,"WKT") ? "" : CPLSPrintf("geom_%s", pszFieldName),
                wkbUnknown );

            /* Useful hack for RFC 41 testing */
            const char* pszEPSG = strstr(pszFieldName, "_EPSG_");
            if( pszEPSG != NULL )
            {
                int nEPSGCode = atoi(pszEPSG + strlen("_EPSG_"));
                OGRSpatialReference* poSRS = new OGRSpatialReference();
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

            poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
            if( !bKeepGeomColumns )
                continue;
        }
        else if( Matches(oField.GetNameRef(),papszGeomPossibleNames) )
        {
            eGeometryFormat = OGR_CSV_GEOM_AS_SOME_GEOM_FORMAT;
            const char* pszFieldName = oField.GetNameRef();
            panGeomFieldIndex[iField] = poFeatureDefn->GetGeomFieldCount();
            OGRGeomFieldDefn oGeomFieldDefn(pszFieldName, wkbUnknown );
            poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
            if( !bKeepGeomColumns )
                continue;
        }
        else if( Matches(oField.GetNameRef(),papszXPossibleNames) )
        {
            oField.SetType(OFTReal);
            iLongitudeField = iField;
            if( !bKeepGeomColumns )
                continue;
        }
        else if( Matches(oField.GetNameRef(),papszYPossibleNames) )
        {
            oField.SetType(OFTReal);
            iLatitudeField = iField;
            if( !bKeepGeomColumns )
                continue;
        }
        else if( Matches(oField.GetNameRef(),papszZPossibleNames) )
        {
            oField.SetType(OFTReal);
            iZField = iField;
            if( !bKeepGeomColumns )
                continue;
        }

        /*http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm specific */
        if ( pszNfdcGeomField != NULL &&
                  EQUALN(oField.GetNameRef(), pszNfdcGeomField, strlen(pszNfdcGeomField)) &&
                  EQUAL(oField.GetNameRef() + strlen(pszNfdcGeomField), "LatitudeS") )
        {
            iNfdcLatitudeS = iField;
            if( !bKeepGeomColumns )
                continue;
        }
        else if ( pszNfdcGeomField != NULL &&
                  EQUALN(oField.GetNameRef(), pszNfdcGeomField, strlen(pszNfdcGeomField)) &&
                  EQUAL(oField.GetNameRef() + strlen(pszNfdcGeomField), "LongitudeS") )
        {
            iNfdcLongitudeS = iField;
            if( !bKeepGeomColumns )
                continue;
        }

        /* GNIS specific */
        else if ( pszGeonamesGeomFieldPrefix != NULL &&
                  EQUALN(oField.GetNameRef(), pszGeonamesGeomFieldPrefix, strlen(pszGeonamesGeomFieldPrefix)) &&
                  (EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LAT_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LATITUDE_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LATITUDE")) )
        {
            oField.SetType(OFTReal);
            iLatitudeField = iField;
            if( !bKeepGeomColumns )
                continue;
        }
        else if ( pszGeonamesGeomFieldPrefix != NULL &&
                  EQUALN(oField.GetNameRef(), pszGeonamesGeomFieldPrefix, strlen(pszGeonamesGeomFieldPrefix)) &&
                  (EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LONG_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LONGITUDE_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LONGITUDE")) )
        {
            oField.SetType(OFTReal);
            iLongitudeField = iField;
            if( !bKeepGeomColumns )
                continue;
        }

        poFeatureDefn->AddFieldDefn( &oField );

        if( bKeepSourceColumns && oField.GetType() != OFTString )
        {
            OGRFieldDefn oFieldOriginal( CPLSPrintf("%s_original", oField.GetNameRef()), OFTString);
            poFeatureDefn->AddFieldDefn( &oFieldOriginal );
        }
    }

    if ( iNfdcLatitudeS != -1 && iNfdcLongitudeS != -1 )
    {
        bDontHonourStrings = TRUE;
        if( poFeatureDefn->GetGeomFieldCount() == 0 )
            poFeatureDefn->SetGeomType( wkbPoint );
        else
            iNfdcLatitudeS = iNfdcLongitudeS = -1;
    }
    else if ( iLatitudeField != -1 && iLongitudeField != -1 )
    {
        if( poFeatureDefn->GetGeomFieldCount() == 0 )
            poFeatureDefn->SetGeomType( (iZField) ? wkbPoint25D : wkbPoint );
        else
            iLatitudeField = iLongitudeField = -1;
    }
    
    if( poFeatureDefn->GetGeomFieldCount() > 0 &&
        poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef() == NULL )
    {
        VSILFILE* fpPRJ = VSIFOpenL(CPLResetExtension(pszFilename, "prj"), "rb");
        if( fpPRJ != NULL )
        {
            GByte* pabyRet = NULL;
            if( VSIIngestFile( fpPRJ, NULL, &pabyRet, NULL, 1000000 ) )
            {
                OGRSpatialReference* poSRS = new OGRSpatialReference();
                if( poSRS->SetFromUserInput((const char*)pabyRet) == OGRERR_NONE )
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

/* -------------------------------------------------------------------- */
/*      Build field definitions for Eurostat TSV files.                 */
/* -------------------------------------------------------------------- */

    CPLString osSeqDim;
    for( iField = 0; bIsEurostatTSV && iField < nFieldCount; iField++ )
    {
        if( iField == 0 )
        {
            char** papszDims = CSLTokenizeString2( papszTokens[0], ",\\", 0 );
            nEurostatDims = CSLCount(papszDims) - 1;
            for(int iSubField = 0; iSubField < nEurostatDims; iSubField++)
            {
                OGRFieldDefn oField(papszDims[iSubField], OFTString);
                poFeatureDefn->AddFieldDefn( &oField );
            }

            osSeqDim = papszDims[nEurostatDims];
            CSLDestroy(papszDims);
        }
        else
        {
            if( papszTokens[iField][0] != '\0' 
                && papszTokens[iField][strlen(papszTokens[iField])-1] == ' ' )
                papszTokens[iField][strlen(papszTokens[iField])-1] = '\0';

            OGRFieldDefn oField(CPLSPrintf("%s_%s", osSeqDim.c_str(), papszTokens[iField]), OFTReal);
            poFeatureDefn->AddFieldDefn( &oField );

            OGRFieldDefn oField2(CPLSPrintf("%s_%s_flag", osSeqDim.c_str(), papszTokens[iField]), OFTString);
            poFeatureDefn->AddFieldDefn( &oField2 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */

    CSLDestroy( papszTokens );
    CSLDestroy( papszFieldTypes );
}

/************************************************************************/
/*                             OGRCSVIsTrue()                           */
/************************************************************************/

static int OGRCSVIsTrue(const char* pszStr)
{
    return EQUAL(pszStr, "t") || EQUAL(pszStr, "true") || EQUAL(pszStr, "y") ||
           EQUAL(pszStr, "yes") || EQUAL(pszStr, "on");
}

/************************************************************************/
/*                            OGRCSVIsFalse()                           */
/************************************************************************/

static int OGRCSVIsFalse(const char* pszStr)
{
    return EQUAL(pszStr, "f") || EQUAL(pszStr, "false") || EQUAL(pszStr, "n") ||
           EQUAL(pszStr, "no") || EQUAL(pszStr, "off");
}

/************************************************************************/
/*                        AutodetectFieldTypes()                        */
/************************************************************************/

char** OGRCSVLayer::AutodetectFieldTypes(char** papszOpenOptions, int nFieldCount)
{
    int iField;
    char** papszFieldTypes = NULL;

    /* Use 1000000 as default maximum distance to be compatible with /vsistdin/ */
    /* caching */
    int nBytes = atoi(CSLFetchNameValueDef(papszOpenOptions,
                                            "AUTODETECT_SIZE_LIMIT", "1000000"));
    if( nBytes == 0 )
    {
        vsi_l_offset nCurPos = VSIFTellL(fpCSV);
        VSIFSeekL(fpCSV, 0, SEEK_END);
        vsi_l_offset nFileSize = VSIFTellL(fpCSV);
        VSIFSeekL(fpCSV, nCurPos, SEEK_SET);
        if( nFileSize < INT_MAX )
            nBytes = (int)nFileSize;
        else
            nBytes = INT_MAX;
    }
    else if( nBytes < 0 || (vsi_l_offset)nBytes < VSIFTellL(fpCSV) )
        nBytes = 1000000;

    const char* pszAutodetectWidth = CSLFetchNameValueDef(papszOpenOptions,
                                                          "AUTODETECT_WIDTH", "NO");
    int bAutodetectWidth = FALSE;
    int bAutodetectWidthForIntOrReal = FALSE;
    if( EQUAL(pszAutodetectWidth, "YES") )
        bAutodetectWidth = bAutodetectWidthForIntOrReal = TRUE;
    else if( EQUAL(pszAutodetectWidth, "STRING_ONLY") )
        bAutodetectWidth = TRUE;

    int bQuotedFieldAsString = CSLTestBoolean(CSLFetchNameValueDef(papszOpenOptions,
                                                      "QUOTED_FIELDS_AS_STRING", "NO"));

    char* pszData = (char*) VSIMalloc( nBytes );
    if( pszData != NULL && (vsi_l_offset)nBytes > VSIFTellL(fpCSV) )
    {
        int nRequested = nBytes - 1 - (int)VSIFTellL(fpCSV);
        int nRead = VSIFReadL(pszData, 1, nRequested, fpCSV);
        pszData[nRead] = 0;

        CPLString osTmpMemFile(CPLSPrintf("/vsimem/tmp%p", this));
        VSILFILE* fpMem = VSIFileFromMemBuffer( osTmpMemFile,
                                                (GByte*)pszData,
                                                nRead,
                                                FALSE );

        std::vector<OGRFieldType> aeFieldType;
        std::vector<int> abFieldBoolean;
        std::vector<int> abFieldSet;
        std::vector<int> anFieldWidth;
        std::vector<int> anFieldPrecision;
        aeFieldType.resize(nFieldCount);
        abFieldBoolean.resize(nFieldCount);
        abFieldSet.resize(nFieldCount);
        anFieldWidth.resize(nFieldCount);
        anFieldPrecision.resize(nFieldCount);
        int nStringFieldCount = 0;

        while( !VSIFEofL(fpMem) )
        {
            char** papszTokens = OGRCSVReadParseLineL( fpMem, chDelimiter, FALSE,
                                                       bQuotedFieldAsString,
                                                       bMergeDelimiter );
            /* Can happen if we just reach EOF while trying to read new bytes */
            if( papszTokens == NULL )
                break;

            /* Ignore last line if it is truncated */
            if( VSIFEofL(fpMem) && nRead == nRequested &&
                pszData[nRead-1] != 13 && pszData[nRead-1] != 10 )
            {
                CSLDestroy(papszTokens);
                break;
            }

            for( iField = 0; papszTokens[iField] != NULL &&
                             iField < nFieldCount; iField++ )
            {
                int nFieldWidth = 0, nFieldPrecision = 0;

                if( papszTokens[iField][0] == 0 )
                    continue;
                if (chDelimiter == ';')
                {
                    char* chComma = strchr(papszTokens[iField], ',');
                    if (chComma)
                        *chComma = '.';
                }
                CPLValueType eType = CPLGetValueType(papszTokens[iField]);

                if( bAutodetectWidth )
                {
                    nFieldWidth = strlen(papszTokens[iField]);
                    if( papszTokens[iField][0] == '"' && 
                        papszTokens[iField][nFieldWidth-1] == '"' )
                    {
                        nFieldWidth -= 2;
                    }
                    if( eType == CPL_VALUE_REAL && bAutodetectWidthForIntOrReal )
                    {
                        const char* pszDot = strchr(papszTokens[iField], '.');
                        if( pszDot != NULL )
                            nFieldPrecision = strlen(pszDot + 1);
                    }
                }

                OGRFieldType eOGRFieldType;
                int bIsBoolean = FALSE;
                if( eType == CPL_VALUE_INTEGER )
                {
                    GIntBig nVal = CPLAtoGIntBig(papszTokens[iField]);
                    if( (GIntBig)(int)nVal != nVal )
                        eOGRFieldType = OFTInteger64;
                    else
                        eOGRFieldType = OFTInteger;
                }
                else if( eType == CPL_VALUE_REAL )
                {
                    eOGRFieldType = OFTReal;
                }
                else if( abFieldSet[iField] && aeFieldType[iField] == OFTString )
                {
                    eOGRFieldType = OFTString;
                    if( abFieldBoolean[iField] )
                    {
                        abFieldBoolean[iField] = OGRCSVIsTrue(papszTokens[iField]) ||
                                                 OGRCSVIsFalse(papszTokens[iField]);
                    }
                }
                else
                {
                    OGRField sWrkField;
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    int bSuccess = OGRParseDate( papszTokens[iField], &sWrkField, 0 );
                    CPLPopErrorHandler();
                    CPLErrorReset();
                    if( bSuccess )
                    {
                        int bHasDate = strchr( papszTokens[iField], '/' ) != NULL ||
                                       strchr( papszTokens[iField], '-' ) != NULL;
                        int bHasTime = strchr( papszTokens[iField], ':' ) != NULL;
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
                        nStringFieldCount ++;
                }
                else if( aeFieldType[iField] != eOGRFieldType )
                {
                    /* Promotion rules */
                    if( aeFieldType[iField] == OFTInteger )
                    {
                        if( eOGRFieldType == OFTInteger64 ||
                            eOGRFieldType == OFTReal )
                            aeFieldType[iField] = eOGRFieldType;
                        else
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount ++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTInteger64 )
                    {
                        if( eOGRFieldType == OFTReal )
                            aeFieldType[iField] = eOGRFieldType;
                        else if( eOGRFieldType != OFTInteger )
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount ++;
                        }
                    } 
                    else if ( aeFieldType[iField] == OFTReal )
                    {
                        if( eOGRFieldType != OFTInteger &&
                            eOGRFieldType != OFTReal )
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount ++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTDate )
                    {
                        if( eOGRFieldType == OFTDateTime )
                            aeFieldType[iField] = OFTDateTime;
                        else
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount ++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTDateTime )
                    {
                        if( eOGRFieldType != OFTDate &&
                            eOGRFieldType != OFTDateTime )
                        {
                            aeFieldType[iField] = OFTString;
                            nStringFieldCount ++;
                        }
                    }
                    else if( aeFieldType[iField] == OFTTime )
                    {
                        aeFieldType[iField] = OFTString;
                        nStringFieldCount ++;
                    }
                }

                if( nFieldWidth > anFieldWidth[iField] )
                    anFieldWidth[iField] = nFieldWidth;
                if( nFieldPrecision > anFieldPrecision[iField] )
                    anFieldPrecision[iField] = nFieldPrecision;
            }

            CSLDestroy(papszTokens);

            /* If all fields are String and we don't need to compute width, */
            /* just stop auto-detection now */
            if( nStringFieldCount == nFieldCount && bAutodetectWidth )
                break;
        }

        papszFieldTypes = (char**) CPLCalloc( nFieldCount + 1, sizeof(char*) );
        for(iField = 0; iField < nFieldCount; iField ++ )
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
                else if ( anFieldWidth[iField] > 0 &&
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
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "CSV", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    // Make sure the header file is written even if no features are written.
    if (bNew && bInWriteMode)
        WriteHeader();

    CPLFree( panGeomFieldIndex );

    poFeatureDefn->Release();
    CPLFree(pszFilename);

    if (fpCSV)
        VSIFCloseL( fpCSV );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCSVLayer::ResetReading()

{
    if (fpCSV)
        VSIRewindL( fpCSV );

    if( bHasFieldNames )
        CSLDestroy( OGRCSVReadParseLineL( fpCSV, chDelimiter, bDontHonourStrings ) );

    bNeedRewindBeforeRead = FALSE;

    nNextFID = 1;
}

/************************************************************************/
/*                        GetNextLineTokens()                           */
/************************************************************************/

char** OGRCSVLayer::GetNextLineTokens()
{
/* -------------------------------------------------------------------- */
/*      Read the CSV record.                                            */
/* -------------------------------------------------------------------- */
    char **papszTokens;

    while(TRUE)
    {
        papszTokens = OGRCSVReadParseLineL( fpCSV, chDelimiter, bDontHonourStrings,
                                            FALSE, bMergeDelimiter );
        if( papszTokens == NULL )
            return NULL;

        if( papszTokens[0] != NULL )
            break;

        CSLDestroy(papszTokens);
    }
    return papszTokens;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature* OGRCSVLayer::GetFeature(GIntBig nFID)
{
    if( nFID < 1 || fpCSV == NULL )
        return NULL;
    if( nFID < nNextFID || bNeedRewindBeforeRead )
        ResetReading();
    while( nNextFID < nFID )
    {
        char **papszTokens = GetNextLineTokens();
        if( papszTokens == NULL )
            return NULL;
        CSLDestroy(papszTokens);
        nNextFID ++;
    }
    return GetNextUnfilteredFeature();
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature * OGRCSVLayer::GetNextUnfilteredFeature()

{
    if (fpCSV == NULL)
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the CSV record.                                            */
/* -------------------------------------------------------------------- */
    char **papszTokens = GetNextLineTokens();
    if( papszTokens == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the OGR feature.                                         */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = new OGRFeature( poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Set attributes for any indicated attribute records.             */
/* -------------------------------------------------------------------- */
    int         iAttr;
    int         iOGRField = 0;
    int         nAttrCount = MIN(CSLCount(papszTokens), nCSVFieldCount );
    CPLValueType eType;
    
    for( iAttr = 0; !bIsEurostatTSV && iAttr < nAttrCount; iAttr++)
    {
        if( (iAttr == iLongitudeField || iAttr == iLatitudeField || iAttr == iZField ) &&
            !bKeepGeomColumns )
        {
            continue;
        }
        int iGeom = panGeomFieldIndex[iAttr];
        if( iGeom >= 0 && papszTokens[iAttr][0] != '\0'&&
            !(poFeatureDefn->GetGeomFieldDefn(iGeom)->IsIgnored()) )
        {
            const char* pszStr = papszTokens[iAttr];
            while( *pszStr == ' ' )
                pszStr ++;
            char *pszWKT = (char*)pszStr;
            OGRGeometry *poGeom = NULL;

            CPLPushErrorHandler(CPLQuietErrorHandler);
            if( OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeom )
                == OGRERR_NONE )
            {
                poGeom->assignSpatialReference(
                    poFeatureDefn->GetGeomFieldDefn(iGeom)->GetSpatialRef());
                poFeature->SetGeomFieldDirectly( iGeom, poGeom );
            }
            else if( *pszStr == '{' &&
                (poGeom = (OGRGeometry*)OGR_G_CreateGeometryFromJson(pszStr)) != NULL )
            {
                poFeature->SetGeomFieldDirectly( iGeom, poGeom );
            }
            else if( ((*pszStr >= '0' && *pszStr <= '9') ||
                      (*pszStr >= 'a' && *pszStr <= 'z') ||
                      (*pszStr >= 'A' && *pszStr <= 'Z') ) &&
                     (poGeom = OGRGeometryFromHexEWKB(pszStr, NULL, FALSE)) != NULL )
            {
                poFeature->SetGeomFieldDirectly( iGeom, poGeom );
            }
            CPLPopErrorHandler();
            if( !bKeepGeomColumns )
                continue;
        }

        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(iOGRField);
        OGRFieldType eFieldType = poFieldDefn->GetType();
        OGRFieldSubType eFieldSubType = poFieldDefn->GetSubType();
        if( eFieldType == OFTInteger && eFieldSubType == OFSTBoolean )
        {
            if (papszTokens[iAttr][0] != '\0' && !poFieldDefn->IsIgnored() )
            {
                if( OGRCSVIsTrue(papszTokens[iAttr]) ||
                    strcmp(papszTokens[iAttr], "1") == 0 )
                {
                    poFeature->SetField( iOGRField, 1 );
                }
                else if( OGRCSVIsFalse(papszTokens[iAttr]) ||
                    strcmp(papszTokens[iAttr], "0") == 0 )
                {
                    poFeature->SetField( iOGRField, 0 );
                }
                else if( !bWarningBadTypeOrWidth )
                {
                    bWarningBadTypeOrWidth = TRUE;
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Invalid value type found in record %d for field %s. "
                                "This warning will no longer be emitted",
                                nNextFID, poFieldDefn->GetNameRef());
                }
            }
        }
        else if( eFieldType == OFTReal || eFieldType == OFTInteger ||
                 eFieldType == OFTInteger64 )
        {
            if (papszTokens[iAttr][0] != '\0' && !poFieldDefn->IsIgnored() )
            {
                if (chDelimiter == ';' && eFieldType == OFTReal)
                {
                    char* chComma = strchr(papszTokens[iAttr], ',');
                    if (chComma)
                        *chComma = '.';
                }
                eType = CPLGetValueType(papszTokens[iAttr]);
                if ( eType == CPL_VALUE_INTEGER || eType == CPL_VALUE_REAL )
                {
                    poFeature->SetField( iOGRField, papszTokens[iAttr] );
                    if( !bWarningBadTypeOrWidth &&
                        (eFieldType == OFTInteger || eFieldType == OFTInteger64) && eType == CPL_VALUE_REAL )
                    {
                        bWarningBadTypeOrWidth = TRUE;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Invalid value type found in record %d for field %s. "
                                 "This warning will no longer be emitted",
                                 nNextFID, poFieldDefn->GetNameRef());
                    }
                    else if( !bWarningBadTypeOrWidth && poFieldDefn->GetWidth() > 0 &&
                             (int)strlen(papszTokens[iAttr]) > poFieldDefn->GetWidth() )
                    {
                        bWarningBadTypeOrWidth = TRUE;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Value with a width greater than field width found in record %d for field %s. "
                                 "This warning will no longer be emitted",
                                 nNextFID, poFieldDefn->GetNameRef());
                    }
                    else if( !bWarningBadTypeOrWidth && eType == CPL_VALUE_REAL &&
                             poFieldDefn->GetWidth() > 0)
                    {
                        const char* pszDot = strchr(papszTokens[iAttr], '.');
                        int nPrecision = 0;
                        if( pszDot != NULL )
                            nPrecision = strlen(pszDot + 1);
                        if( nPrecision > poFieldDefn->GetPrecision() )
                        {
                             bWarningBadTypeOrWidth = TRUE;
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Value with a precision greater than field precision found in record %d for field %s. "
                                     "This warning will no longer be emitted",
                                     nNextFID, poFieldDefn->GetNameRef());
                        }
                    }
                }
                else
                {
                    if( !bWarningBadTypeOrWidth )
                    {
                        bWarningBadTypeOrWidth = TRUE;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                    "Invalid value type found in record %d for field %s. "
                                    "This warning will no longer be emitted",
                                    nNextFID, poFieldDefn->GetNameRef());
                    }
                }
            }
        }
        else if (eFieldType != OFTString)
        {
            if (papszTokens[iAttr][0] != '\0' && !poFieldDefn->IsIgnored())
            {
                poFeature->SetField( iOGRField, papszTokens[iAttr] );
                if( !bWarningBadTypeOrWidth && !poFeature->IsFieldSet(iOGRField) )
                {
                    bWarningBadTypeOrWidth = TRUE;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid value type found in record %d for field %s. "
                             "This warning will no longer be emitted",
                             nNextFID, poFieldDefn->GetNameRef());
                }
            }
        }
        else
        {
            if( !poFieldDefn->IsIgnored() )
            {
                poFeature->SetField( iOGRField, papszTokens[iAttr] );
                if( !bWarningBadTypeOrWidth && poFieldDefn->GetWidth() > 0 &&
                    (int)strlen(papszTokens[iAttr]) > poFieldDefn->GetWidth() )
                {
                    bWarningBadTypeOrWidth = TRUE;
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Value with a width greater than field width found in record %d for field %s. "
                                "This warning will no longer be emitted",
                                nNextFID, poFieldDefn->GetNameRef());
                }
            }
        }

        if( bKeepSourceColumns && eFieldType != OFTString )
        {
            iOGRField ++;
            if( papszTokens[iAttr][0] != '\0' &&
                !poFeatureDefn->GetFieldDefn(iOGRField)->IsIgnored() )
            {
                poFeature->SetField( iOGRField, papszTokens[iAttr] );
            }
        }
        
        iOGRField++;
    }

/* -------------------------------------------------------------------- */
/*      Eurostat TSV files.                                             */
/* -------------------------------------------------------------------- */

    for( iAttr = 0; bIsEurostatTSV && iAttr < nAttrCount; iAttr++)
    {
        if( iAttr == 0 )
        {
            char** papszDims = CSLTokenizeString2( papszTokens[0], ",", 0 );
            if( CSLCount(papszDims) != nEurostatDims )
            {
                CSLDestroy(papszDims);
                break;
            }
            for( int iSubAttr = 0; iSubAttr < nEurostatDims; iSubAttr ++ )
            {
                if( !poFeatureDefn->GetFieldDefn(iSubAttr)->IsIgnored() )
                    poFeature->SetField( iSubAttr, papszDims[iSubAttr] );
            }
            CSLDestroy(papszDims);
        }
        else
        {
            char** papszVals = CSLTokenizeString2( papszTokens[iAttr], " ", 0 );
            eType = CPLGetValueType(papszVals[0]);
            if ( (papszVals[0][0] != '\0')  &&
                 ( eType == CPL_VALUE_INTEGER ||
                   eType == CPL_VALUE_REAL ) )
            {
                if( !poFeatureDefn->GetFieldDefn(nEurostatDims + 2 * (iAttr - 1))->IsIgnored() )
                    poFeature->SetField( nEurostatDims + 2 * (iAttr - 1), papszVals[0] );
            }
            if( CSLCount(papszVals) == 2 )
            {
                if( !poFeatureDefn->GetFieldDefn(nEurostatDims + 2 * (iAttr - 1) + 1)->IsIgnored() )
                    poFeature->SetField( nEurostatDims + 2 * (iAttr - 1) + 1, papszVals[1] );
            }
            CSLDestroy(papszVals);
        }
    }

/* -------------------------------------------------------------------- */
/*http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm specific */
/* -------------------------------------------------------------------- */

    if ( iNfdcLatitudeS != -1 &&
         iNfdcLongitudeS != -1 &&
         nAttrCount > iNfdcLatitudeS &&
         nAttrCount > iNfdcLongitudeS  &&
         papszTokens[iNfdcLongitudeS][0] != 0 &&
         papszTokens[iNfdcLatitudeS][0] != 0)
    {
        double dfLon = CPLAtof(papszTokens[iNfdcLongitudeS]) / 3600;
        if (strchr(papszTokens[iNfdcLongitudeS], 'W'))
            dfLon *= -1;
        double dfLat = CPLAtof(papszTokens[iNfdcLatitudeS]) / 3600;
        if (strchr(papszTokens[iNfdcLatitudeS], 'S'))
            dfLat *= -1;
        if( !(poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored()) )
            poFeature->SetGeometryDirectly( new OGRPoint(dfLon, dfLat) );
    }

/* -------------------------------------------------------------------- */
/*      GNIS specific                                                   */
/* -------------------------------------------------------------------- */
    else if ( iLatitudeField != -1 &&
              iLongitudeField != -1 &&
              nAttrCount > iLatitudeField &&
              nAttrCount > iLongitudeField  &&
              papszTokens[iLongitudeField][0] != 0 &&
              papszTokens[iLatitudeField][0] != 0)
    {
        /* Some records have dummy 0,0 value */
        if (papszTokens[iLongitudeField][0] != '0' ||
            papszTokens[iLongitudeField][1] != '\0' ||
            papszTokens[iLatitudeField][0] != '0' ||
            papszTokens[iLatitudeField][1] != '\0')
        {
            double dfLon = CPLAtof(papszTokens[iLongitudeField]);
            double dfLat = CPLAtof(papszTokens[iLatitudeField]);
            if( !(poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored()) )
            {
                if( iZField != -1 && nAttrCount > iZField && papszTokens[iZField][0] != 0 )
                    poFeature->SetGeometryDirectly( new OGRPoint(dfLon, dfLat, CPLAtof(papszTokens[iZField])) );
                else
                    poFeature->SetGeometryDirectly( new OGRPoint(dfLon, dfLat) );
            }
        }
    }

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Translate the record id.                                        */
/* -------------------------------------------------------------------- */
    poFeature->SetFID( nNextFID++ );

    m_nFeaturesRead++;

    return poFeature;
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRCSVLayer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;

    if( bNeedRewindBeforeRead )
        ResetReading();
    
/* -------------------------------------------------------------------- */
/*      Read features till we find one that satisfies our current       */
/*      spatial criteria.                                               */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        poFeature = GetNextUnfilteredFeature();
        if( poFeature == NULL )
            break;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            break;

        delete poFeature;
    }

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return bInWriteMode && !bKeepSourceColumns && bKeepGeomColumns;
    else if( EQUAL(pszCap,OLCCreateField) )
        return bNew && !bHasFieldNames;
    else if( EQUAL(pszCap,OLCCreateGeomField) )
        return bNew && !bHasFieldNames && eGeometryFormat == OGR_CSV_GEOM_AS_WKT;
    else if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE;
    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCSVLayer::CreateField( OGRFieldDefn *poNewField, int bApproxOK )

{
/* -------------------------------------------------------------------- */
/*      If we have already written our field names, then we are not     */
/*      allowed to add new fields.                                      */
/* -------------------------------------------------------------------- */
    if( !TestCapability(OLCCreateField) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create new fields after first feature written.");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Does this duplicate an existing field?                          */
/* -------------------------------------------------------------------- */
    if( poFeatureDefn->GetFieldIndex( poNewField->GetNameRef() ) != -1 )
    {
        if( poFeatureDefn->GetGeomFieldIndex( poNewField->GetNameRef() ) != -1 )
            return OGRERR_NONE;
        if( poFeatureDefn->GetGeomFieldIndex( CPLSPrintf("geom_%s", poNewField->GetNameRef()) ) != -1 )
            return OGRERR_NONE;

        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create field %s, but a field with this name already exists.",
                  poNewField->GetNameRef() );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Is this a legal field type for CSV?  For now we only allow      */
/*      simple integer, real and string fields.                         */
/* -------------------------------------------------------------------- */
    switch( poNewField->GetType() )
    {
      case OFTInteger:
      case OFTInteger64:
      case OFTReal:
      case OFTString:
        // these types are OK.
        break;

      default:
        if( bApproxOK )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Attempt to create field of type %s, but this is not supported\n"
                      "for .csv files.  Just treating as a plain string.",
                      poNewField->GetFieldTypeName( poNewField->GetType() ) );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Attempt to create field of type %s, but this is not supported\n"
                      "for .csv files.",
                      poNewField->GetFieldTypeName( poNewField->GetType() ) );
            return OGRERR_FAILURE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Seems ok, add to field list.                                    */
/* -------------------------------------------------------------------- */
    poFeatureDefn->AddFieldDefn( poNewField );
    nCSVFieldCount ++;

    panGeomFieldIndex = (int*) CPLRealloc(panGeomFieldIndex,
                                sizeof(int) * poFeatureDefn->GetFieldCount());
    panGeomFieldIndex[poFeatureDefn->GetFieldCount() - 1] = -1;

    return OGRERR_NONE;
}


/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRCSVLayer::CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                     CPL_UNUSED int bApproxOK )
{
    if( !TestCapability(OLCCreateGeomField) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create new fields after first feature written.");
        return OGRERR_FAILURE;
    }

    poFeatureDefn->AddGeomFieldDefn( poGeomField );

    const char* pszName = poGeomField->GetNameRef();
    if( EQUALN(pszName, "geom_", strlen("geom_")) )
        pszName += strlen("geom_");
    if( !EQUAL(pszName, "WKT") && !EQUALN(pszName, "_WKT", 4) )
        pszName = CPLSPrintf("_WKT%s", pszName);

    OGRFieldDefn oRegularFieldDefn( pszName, OFTString );
    poFeatureDefn->AddFieldDefn( &oRegularFieldDefn );
    nCSVFieldCount ++;

    panGeomFieldIndex = (int*) CPLRealloc(panGeomFieldIndex,
                                sizeof(int) * poFeatureDefn->GetFieldCount());
    panGeomFieldIndex[poFeatureDefn->GetFieldCount() - 1] =
        poFeatureDefn->GetGeomFieldCount() - 1;

    return OGRERR_FAILURE;
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

/* -------------------------------------------------------------------- */
/*      Write field names if we haven't written them yet.               */
/*      Write .csvt file if needed                                      */
/* -------------------------------------------------------------------- */
    bNew = FALSE;
    bHasFieldNames = TRUE;

    for(int iFile=0;iFile<((bCreateCSVT) ? 2 : 1);iFile++)
    {
        VSILFILE* fpCSVT = NULL;
        if (bCreateCSVT && iFile == 0)
        {
            char* pszDirName = CPLStrdup(CPLGetDirname(pszFilename));
            char* pszBaseName = CPLStrdup(CPLGetBasename(pszFilename));
            fpCSVT = VSIFOpenL(CPLFormFilename(pszDirName, pszBaseName, ".csvt"), "wb");
            CPLFree(pszDirName);
            CPLFree(pszBaseName);
        }
        else
        {
            if( strncmp(pszFilename, "/vsistdout/", 11) == 0 ||
                strncmp(pszFilename, "/vsizip/", 8) == 0 )
                fpCSV = VSIFOpenL( pszFilename, "wb" );
            else
                fpCSV = VSIFOpenL( pszFilename, "w+b" );

            if( fpCSV == NULL )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to create %s:\n%s",
                          pszFilename, VSIStrerror( errno ) );
                return OGRERR_FAILURE;
            }
        }

        if (bWriteBOM && fpCSV)
        {
            VSIFWriteL("\xEF\xBB\xBF", 1, 3, fpCSV);
        }

        if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ)
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "X%cY%cZ", chDelimiter, chDelimiter);
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "CoordX,CoordY,Real");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_XY)
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "X%cY", chDelimiter);
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "CoordX,CoordY");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_YX)
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "Y%cX", chDelimiter);
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "CoordY,CoordX");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }
        }

        if( bHiddenWKTColumn )
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "%s", poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef() );
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "WKT");
        }

        for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            char *pszEscaped;

            if( iField > 0 || bHiddenWKTColumn )
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }

            pszEscaped = 
                CPLEscapeString( poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), 
                                 -1, CPLES_CSV );

            if (fpCSV)
            {
                int bAddDoubleQuote = FALSE;
                if( chDelimiter == ' ' && pszEscaped[0] != '"' && strchr(pszEscaped, ' ') != NULL )
                    bAddDoubleQuote = TRUE;
                if( bAddDoubleQuote )
                    VSIFWriteL( "\"", 1, 1, fpCSV );
                VSIFPrintfL( fpCSV, "%s", pszEscaped );
                if( bAddDoubleQuote )
                    VSIFWriteL( "\"", 1, 1, fpCSV );
            }
            CPLFree( pszEscaped );

            if (fpCSVT)
            {
                int nWidth = poFeatureDefn->GetFieldDefn(iField)->GetWidth();
                int nPrecision = poFeatureDefn->GetFieldDefn(iField)->GetPrecision();

                switch( poFeatureDefn->GetFieldDefn(iField)->GetType() )
                {
                  case OFTInteger:  
                  {
                      if( poFeatureDefn->GetFieldDefn(iField)->GetSubType() == OFSTBoolean )
                      {
                          nWidth = 0;
                          VSIFPrintfL( fpCSVT, "%s", "Integer(Boolean)");
                      }
                      else if( poFeatureDefn->GetFieldDefn(iField)->GetSubType() == OFSTInt16 )
                      {
                          nWidth = 0;
                          VSIFPrintfL( fpCSVT, "%s", "Integer(Int16)");
                      }
                      else
                          VSIFPrintfL( fpCSVT, "%s", "Integer");
                      break;
                  }
                  case OFTInteger64:
                      VSIFPrintfL( fpCSVT, "%s", "Integer64");
                      break;
                  case OFTReal:
                  {
                      if( poFeatureDefn->GetFieldDefn(iField)->GetSubType() == OFSTFloat32 )
                      {
                          nWidth = 0;
                          VSIFPrintfL( fpCSVT, "%s", "Real(Float32)");
                      }
                      else
                          VSIFPrintfL( fpCSVT, "%s", "Real");
                      break;
                  }
                  case OFTDate:     VSIFPrintfL( fpCSVT, "%s", "Date"); break;
                  case OFTTime:     VSIFPrintfL( fpCSVT, "%s", "Time"); break;
                  case OFTDateTime: VSIFPrintfL( fpCSVT, "%s", "DateTime"); break;
                  default:          VSIFPrintfL( fpCSVT, "%s", "String"); break;
                }

                if (nWidth != 0)
                {
                    if (nPrecision != 0)
                        VSIFPrintfL( fpCSVT, "(%d.%d)", nWidth, nPrecision);
                    else
                        VSIFPrintfL( fpCSVT, "(%d)", nWidth);
                }
            }
        }

        /* The CSV driver will not recognize single column tables, so add */
        /* a fake second blank field */
        if( poFeatureDefn->GetFieldCount() == 1 ||
            (poFeatureDefn->GetFieldCount() == 0 && bHiddenWKTColumn) )
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
        }

        if( bUseCRLF )
        {
            if (fpCSV) VSIFPutcL( 13, fpCSV );
            if (fpCSVT) VSIFPutcL( 13, fpCSVT );
        }
        if (fpCSV) VSIFPutcL( '\n', fpCSV );
        if (fpCSVT) VSIFPutcL( '\n', fpCSVT );
        if (fpCSVT) VSIFCloseL(fpCSVT);
    }

    if (fpCSV == NULL) 
        return OGRERR_FAILURE;
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRCSVLayer::ICreateFeature( OGRFeature *poNewFeature )

{
    int iField;

    if( !bInWriteMode )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "The CreateFeature() operation is not permitted on a read-only CSV." );
        return OGRERR_FAILURE;
    }

    /* If we need rewind, it means that we have just written a feature before */
    /* so there's no point seeking to the end of the file, as we're already */
    /* at the end */
    int bNeedSeekEnd = !bNeedRewindBeforeRead;

    bNeedRewindBeforeRead = TRUE;

/* -------------------------------------------------------------------- */
/*      Write field names if we haven't written them yet.               */
/*      Write .csvt file if needed                                      */
/* -------------------------------------------------------------------- */
    if( bNew )
    {
        OGRErr eErr = WriteHeader();
        if (eErr != OGRERR_NONE)
            return eErr;
        bNeedSeekEnd = FALSE;
    }

    if (fpCSV == NULL)
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Make sure we are at the end of the file.                        */
/* -------------------------------------------------------------------- */
    if (bNeedSeekEnd)
    {
        if (bFirstFeatureAppendedDuringSession)
        {
            /* Add a newline character to the end of the file if necessary */
            bFirstFeatureAppendedDuringSession = FALSE;
            VSIFSeekL( fpCSV, 0, SEEK_END );
            VSIFSeekL( fpCSV, VSIFTellL(fpCSV) - 1, SEEK_SET);
            char chLast;
            VSIFReadL( &chLast, 1, 1, fpCSV );
            VSIFSeekL( fpCSV, 0, SEEK_END );
            if (chLast != '\n')
            {
                if( bUseCRLF )
                    VSIFPutcL( 13, fpCSV );
                VSIFPutcL( '\n', fpCSV );
            }
        }
        else
        {
            VSIFSeekL( fpCSV, 0, SEEK_END );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write out the geometry                                          */
/* -------------------------------------------------------------------- */
    if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ ||
             eGeometryFormat == OGR_CSV_GEOM_AS_XY ||
             eGeometryFormat == OGR_CSV_GEOM_AS_YX)
    {
        OGRGeometry     *poGeom = poNewFeature->GetGeometryRef();
        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        {
            OGRPoint* poPoint = (OGRPoint*) poGeom;
            char szBuffer[75];
            if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ )
                OGRMakeWktCoordinate(szBuffer, poPoint->getX(), poPoint->getY(), poPoint->getZ(), 3);
            else if (eGeometryFormat == OGR_CSV_GEOM_AS_XY )
                OGRMakeWktCoordinate(szBuffer, poPoint->getX(), poPoint->getY(), 0, 2);
            else
                OGRMakeWktCoordinate(szBuffer, poPoint->getY(), poPoint->getX(), 0, 2);
            char* pc = szBuffer;
            while(*pc != '\0')
            {
                if (*pc == ' ')
                    *pc = chDelimiter;
                pc ++;
            }
            VSIFPrintfL( fpCSV, "%s", szBuffer );
        }
        else
        {
            VSIFPrintfL( fpCSV, "%c", chDelimiter );
            if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ)
                VSIFPrintfL( fpCSV, "%c", chDelimiter );
        }
        if (poFeatureDefn->GetFieldCount() > 0)
            VSIFPrintfL( fpCSV, "%c", chDelimiter );
    }

/* -------------------------------------------------------------------- */
/*      Special case to deal with hidden "WKT" geometry column          */
/* -------------------------------------------------------------------- */
    int bNonEmptyLine = FALSE;

    if( bHiddenWKTColumn )
    {
        char *pszWKT = NULL;
        OGRGeometry     *poGeom = poNewFeature->GetGeomFieldRef(0);
        if (poGeom && poGeom->exportToWkt(&pszWKT) == OGRERR_NONE)
        {
            bNonEmptyLine = TRUE;
            VSIFWriteL( "\"", 1, 1, fpCSV );
            VSIFWriteL( pszWKT, 1, strlen(pszWKT), fpCSV );
            VSIFWriteL( "\"", 1, 1, fpCSV );
        }
        CPLFree(pszWKT);
    }

/* -------------------------------------------------------------------- */
/*      Write out all the field values.                                 */
/* -------------------------------------------------------------------- */

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        char *pszEscaped;
        
        if( iField > 0 || bHiddenWKTColumn )
            VSIFPrintfL( fpCSV, "%c", chDelimiter );

        if (eGeometryFormat == OGR_CSV_GEOM_AS_WKT &&
            panGeomFieldIndex[iField] >= 0 )
        {
            int iGeom = panGeomFieldIndex[iField];
            OGRGeometry     *poGeom = poNewFeature->GetGeomFieldRef(iGeom);
            if (poGeom && poGeom->exportToWkt(&pszEscaped) == OGRERR_NONE)
            {
                int nLenWKT = (int)strlen(pszEscaped);
                char* pszNew = (char*) CPLMalloc(1 + nLenWKT + 1 + 1);
                pszNew[0] = '"';
                memcpy(pszNew + 1, pszEscaped, nLenWKT);
                pszNew[1 + nLenWKT] = '"';
                pszNew[1 + nLenWKT + 1] = '\0';
                CPLFree(pszEscaped);
                pszEscaped = pszNew;
            }
            else
                pszEscaped = CPLStrdup("");
        }
        else if (poFeatureDefn->GetFieldDefn(iField)->GetType() == OFTReal)
        {
            pszEscaped = CPLStrdup(poNewFeature->GetFieldAsString(iField));
        }
        else
        {
            pszEscaped =
                CPLEscapeString( poNewFeature->GetFieldAsString(iField),
                                -1, CPLES_CSV );
        }

        int nLen = (int)strlen(pszEscaped);
        bNonEmptyLine |= (nLen != 0);
        int bAddDoubleQuote = FALSE;
        if( chDelimiter == ' ' && pszEscaped[0] != '"' && strchr(pszEscaped, ' ') != NULL )
            bAddDoubleQuote = TRUE;
        if( bAddDoubleQuote )
            VSIFWriteL( "\"", 1, 1, fpCSV );
        VSIFWriteL( pszEscaped, 1, nLen, fpCSV );
        if( bAddDoubleQuote )
            VSIFWriteL( "\"", 1, 1, fpCSV );
        CPLFree( pszEscaped );
    }

    if( (poFeatureDefn->GetFieldCount() == 1 ||
         (poFeatureDefn->GetFieldCount() == 0 && bHiddenWKTColumn)) && !bNonEmptyLine )
        VSIFPrintfL( fpCSV, "%c", chDelimiter );

    if( bUseCRLF )
        VSIFPutcL( 13, fpCSV );
    VSIFPutcL( '\n', fpCSV );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetCRLF()                               */
/************************************************************************/

void OGRCSVLayer::SetCRLF( int bNewValue )

{
    bUseCRLF = bNewValue;
}

/************************************************************************/
/*                       SetWriteGeometry()                             */
/************************************************************************/

void OGRCSVLayer::SetWriteGeometry(OGRwkbGeometryType eGType,
                                   OGRCSVGeometryFormat eGeometryFormat,
                                   const char* pszGeomCol)
{
    this->eGeometryFormat = eGeometryFormat;
    if (eGeometryFormat == OGR_CSV_GEOM_AS_WKT && eGType != wkbNone )
    {
        OGRGeomFieldDefn oGFld(pszGeomCol, eGType);
        bHiddenWKTColumn = TRUE;
        /* We don't use CreateGeomField() since we don't want to generate */
        /* a geometry field in first position, as it confuses applications */
        /* (such as MapServer <= 6.4) that assume that the first regular field */
        /* they add will be at index 0 */
        poFeatureDefn->AddGeomFieldDefn( &oGFld );
    }
    else
        poFeatureDefn->SetGeomType( eGType );
}

/************************************************************************/
/*                          SetCreateCSVT()                             */
/************************************************************************/

void OGRCSVLayer::SetCreateCSVT(int bCreateCSVT)
{
    this->bCreateCSVT = bCreateCSVT;
}

/************************************************************************/
/*                          SetWriteBOM()                               */
/************************************************************************/

void OGRCSVLayer::SetWriteBOM(int bWriteBOM)
{
    this->bWriteBOM = bWriteBOM;
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRCSVLayer::GetFeatureCount( int bForce )
{
    if (bInWriteMode || m_poFilterGeom != NULL || m_poAttrQuery != NULL)
        return OGRLayer::GetFeatureCount(bForce);

    if (nTotalFeatures >= 0)
        return nTotalFeatures;

    if (fpCSV == NULL)
        return 0;

    ResetReading();

    char **papszTokens;
    nTotalFeatures = 0;
    while(TRUE)
    {
        papszTokens = GetNextLineTokens();
        if( papszTokens == NULL )
            break;

        nTotalFeatures ++;

        CSLDestroy(papszTokens);
    }

    ResetReading();

    return nTotalFeatures;
}
