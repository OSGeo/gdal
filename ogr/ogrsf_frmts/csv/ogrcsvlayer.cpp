/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVLayer class.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

static char **CSVSplitLine( const char *pszString, char chDelimiter )

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
                break;
            }

            if( *pszString == '"' )
            {
                if( !bInString || pszString[1] != '"' )
                {
                    bInString = !bInString;
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

char **OGRCSVReadParseLineL( VSILFILE * fp, char chDelimiter, int bDontHonourStrings )

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
        return CSVSplitLine( pszLine, chDelimiter );

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
            if( pszWorkLine[i] == '\"'
                && (i == 0 || pszWorkLine[i-1] != '\\') )
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

    papszReturn = CSVSplitLine( pszWorkLine, chDelimiter );

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
                          VSILFILE * fp, const char *pszFilename, int bNew, int bInWriteMode,
                          char chDelimiter, const char* pszNfdcGeomField,
                          const char* pszGeonamesGeomFieldPrefix)

{
    fpCSV = fp;

    iWktGeomReadField = -1;
    iNfdcLatitudeS = iNfdcLongitudeS = -1;
    iLatitudeField = iLongitudeField = -1;
    this->bInWriteMode = bInWriteMode;
    this->bNew = bNew;
    this->pszFilename = CPLStrdup(pszFilename);
    this->chDelimiter = chDelimiter;

    bFirstFeatureAppendedDuringSession = TRUE;
    bUseCRLF = FALSE;
    bNeedRewindBeforeRead = FALSE;
    eGeometryFormat = OGR_CSV_GEOM_NONE;

    nNextFID = 1;

    poFeatureDefn = new OGRFeatureDefn( pszLayerNameIn );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    bCreateCSVT = FALSE;
    bDontHonourStrings = FALSE;

    nTotalFeatures = -1;

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
        }
        VSIRewindL( fpCSV );
    }

/* -------------------------------------------------------------------- */
/*      Check if the first record seems to be field definitions or      */
/*      not.  We assume it is field definitions if none of the          */
/*      values are strictly numeric.                                    */
/* -------------------------------------------------------------------- */
    char **papszTokens = NULL;
    char **papszTokensQuotes = NULL;
    int nFieldCount=0, iField;
    CPLValueType eType;

    if( !bNew )
    {
        const char *pszLine = NULL;
        char szDelimiter[2];
        szDelimiter[0] = chDelimiter; szDelimiter[1] = '\0';
        pszLine = CPLReadLineL( fpCSV );
        if ( pszLine != NULL )
        {
            /* tokenize without quotes to get the actual values */
            papszTokens = CSLTokenizeString2( pszLine, szDelimiter, 
                                              CSLT_HONOURSTRINGS);
            /* tokenize the strings and preserve quotes, so we can separate string from numeric */
            /* this is only used in the test for bHasFeldNames (bug #4361) */
            papszTokensQuotes = CSLTokenizeString2( pszLine, szDelimiter, 
                                                    CSLT_HONOURSTRINGS | CSLT_PRESERVEQUOTES );
        }
        // papszTokens = OGRCSVReadParseLineL( fpCSV, chDelimiter, FALSE );    
        nFieldCount = CSLCount( papszTokens );
        bHasFieldNames = TRUE;

        for( iField = 0; iField < nFieldCount && bHasFieldNames; iField++ )
        {
            eType = CPLGetValueType(papszTokensQuotes[iField]);
            if ( (eType == CPL_VALUE_INTEGER ||
                  eType == CPL_VALUE_REAL) ) {
                /* we have a numeric field, therefore do not consider the first line as field names */
                bHasFieldNames = FALSE;
            }
        }
    }
    else
        bHasFieldNames = FALSE;

    if( !bNew && !bHasFieldNames )
        VSIRewindL( fpCSV );

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
        char* dname = strdup(CPLGetDirname(pszFilename));
        char* fname = strdup(CPLGetBasename(pszFilename));
        VSILFILE* fpCSVT = VSIFOpenL(CPLFormFilename(dname, fname, ".csvt"), "r");
        free(dname);
        free(fname);
        if (fpCSVT!=NULL) {
            VSIRewindL(fpCSVT);
            papszFieldTypes = OGRCSVReadParseLineL(fpCSVT, ',', FALSE);
            VSIFCloseL(fpCSVT);
        }
    }
    

/* -------------------------------------------------------------------- */
/*      Build field definitions.                                        */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < nFieldCount; iField++ )
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
            pszFieldName = szFieldNameBuffer;
            sprintf( szFieldNameBuffer, "field_%d", iField+1 );
        }

        OGRFieldDefn oField(pszFieldName, OFTString);
        if (papszFieldTypes!=NULL && iField<CSLCount(papszFieldTypes)) {

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

        if( EQUAL(oField.GetNameRef(),"WKT")
            && oField.GetType() == OFTString 
            && iWktGeomReadField == -1 )
        {
            iWktGeomReadField = iField;
            poFeatureDefn->SetGeomType( wkbUnknown );
        }

        /*http://www.faa.gov/airports/airport_safety/airportdata_5010/menu/index.cfm specific */
        if ( pszNfdcGeomField != NULL &&
                  EQUALN(oField.GetNameRef(), pszNfdcGeomField, strlen(pszNfdcGeomField)) &&
                  EQUAL(oField.GetNameRef() + strlen(pszNfdcGeomField), "LatitudeS") )
            iNfdcLatitudeS = iField;
        else if ( pszNfdcGeomField != NULL &&
                  EQUALN(oField.GetNameRef(), pszNfdcGeomField, strlen(pszNfdcGeomField)) &&
                  EQUAL(oField.GetNameRef() + strlen(pszNfdcGeomField), "LongitudeS") )
            iNfdcLongitudeS = iField;

        /* GNIS specific */
        else if ( pszGeonamesGeomFieldPrefix != NULL &&
                  EQUALN(oField.GetNameRef(), pszGeonamesGeomFieldPrefix, strlen(pszGeonamesGeomFieldPrefix)) &&
                  (EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LAT_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LATITUDE_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LATITUDE")) )
        {
            oField.SetType(OFTReal);
            iLatitudeField = iField;
        }
        else if ( pszGeonamesGeomFieldPrefix != NULL &&
                  EQUALN(oField.GetNameRef(), pszGeonamesGeomFieldPrefix, strlen(pszGeonamesGeomFieldPrefix)) &&
                  (EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LONG_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LONGITUDE_DEC") ||
                   EQUAL(oField.GetNameRef() + strlen(pszGeonamesGeomFieldPrefix), "_LONGITUDE")) )
        {
            oField.SetType(OFTReal);
            iLongitudeField = iField;
        }

        poFeatureDefn->AddFieldDefn( &oField );

    }

    if ( iNfdcLatitudeS != -1 && iNfdcLongitudeS != -1 )
    {
        bDontHonourStrings = TRUE;
        poFeatureDefn->SetGeomType( wkbPoint );
    }
    else if ( iLatitudeField != -1 && iLongitudeField != -1 )
    {
        poFeatureDefn->SetGeomType( wkbPoint );
    }
    
    CSLDestroy( papszTokens );
    CSLDestroy( papszFieldTypes );
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
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature * OGRCSVLayer::GetNextUnfilteredFeature()

{
    if (fpCSV == NULL)
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the CSV record.                                            */
/* -------------------------------------------------------------------- */
    char **papszTokens;

    while(TRUE)
    {
        papszTokens = OGRCSVReadParseLineL( fpCSV, chDelimiter, bDontHonourStrings );
        if( papszTokens == NULL )
            return NULL;

        if( papszTokens[0] != NULL )
            break;

        CSLDestroy(papszTokens);
    }

/* -------------------------------------------------------------------- */
/*      Create the OGR feature.                                         */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = new OGRFeature( poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Set attributes for any indicated attribute records.             */
/* -------------------------------------------------------------------- */
    int         iAttr;
    int         nAttrCount = MIN(CSLCount(papszTokens),
                                 poFeatureDefn->GetFieldCount() );
    CPLValueType eType;
    
    for( iAttr = 0; iAttr < nAttrCount; iAttr++)
    {
        if( iAttr == iWktGeomReadField && papszTokens[iAttr][0] != '\0' )
        {
            char *pszWKT = papszTokens[iAttr];
            OGRGeometry *poGeom = NULL;

            if( OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeom )
                == OGRERR_NONE )
                poFeature->SetGeometryDirectly( poGeom );
        }

        if ( (poFeatureDefn->GetFieldDefn(iAttr)->GetType() == OFTReal) ||
             (poFeatureDefn->GetFieldDefn(iAttr)->GetType() == OFTInteger) )
        {
            eType = CPLGetValueType(papszTokens[iAttr]);
            if ( (papszTokens[iAttr][0] != '\0') &&
                 ( eType == CPL_VALUE_INTEGER ||
                   eType == CPL_VALUE_REAL ) )
                poFeature->SetField( iAttr, CPLAtof(papszTokens[iAttr]) );
        }
        else if (poFeatureDefn->GetFieldDefn(iAttr)->GetType() != OFTString)
        {
            if (papszTokens[iAttr][0] != '\0')
                poFeature->SetField( iAttr, papszTokens[iAttr] );
        }
        else
            poFeature->SetField( iAttr, papszTokens[iAttr] );

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
        double dfLon = atof(papszTokens[iNfdcLongitudeS]) / 3600;
        if (strchr(papszTokens[iNfdcLongitudeS], 'W'))
            dfLon *= -1;
        double dfLat = atof(papszTokens[iNfdcLatitudeS]) / 3600;
        if (strchr(papszTokens[iNfdcLatitudeS], 'S'))
            dfLat *= -1;
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
            double dfLon = atof(papszTokens[iLongitudeField]);
            double dfLat = atof(papszTokens[iLatitudeField]);
            poFeature->SetGeometryDirectly( new OGRPoint(dfLon, dfLat) );
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
            || FilterGeometry( poFeature->GetGeometryRef() ) )
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
        return bInWriteMode;
    else if( EQUAL(pszCap,OLCCreateField) )
        return bNew && !bHasFieldNames;
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
    if( bHasFieldNames || !bNew )
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

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRCSVLayer::CreateFeature( OGRFeature *poNewFeature )

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
    if( !bHasFieldNames )
    {
      bHasFieldNames = TRUE;
      bNeedSeekEnd = FALSE;

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

        if (eGeometryFormat == OGR_CSV_GEOM_AS_WKT)
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "%s", "WKT");
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "String");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ)
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "X%cY%cZ", chDelimiter, chDelimiter);
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "Real,Real,Real");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_XY)
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "X%cY", chDelimiter);
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "Real,Real");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_YX)
        {
            if (fpCSV) VSIFPrintfL( fpCSV, "Y%cX", chDelimiter);
            if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", "Real,Real");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }
        }

        for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            char *pszEscaped;

            if( iField > 0 )
            {
                if (fpCSV) VSIFPrintfL( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintfL( fpCSVT, "%s", ",");
            }

            pszEscaped = 
                CPLEscapeString( poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), 
                                 -1, CPLES_CSV );

            if (fpCSV) VSIFPrintfL( fpCSV, "%s", pszEscaped );
            CPLFree( pszEscaped );

            if (fpCSVT)
            {
                switch( poFeatureDefn->GetFieldDefn(iField)->GetType() )
                {
                    case OFTInteger:  VSIFPrintfL( fpCSVT, "%s", "Integer"); break;
                    case OFTReal:     VSIFPrintfL( fpCSVT, "%s", "Real"); break;
                    case OFTDate:     VSIFPrintfL( fpCSVT, "%s", "Date"); break;
                    case OFTTime:     VSIFPrintfL( fpCSVT, "%s", "Time"); break;
                    case OFTDateTime: VSIFPrintfL( fpCSVT, "%s", "DateTime"); break;
                    default:          VSIFPrintfL( fpCSVT, "%s", "String"); break;
                }

                int nWidth = poFeatureDefn->GetFieldDefn(iField)->GetWidth();
                int nPrecision = poFeatureDefn->GetFieldDefn(iField)->GetPrecision();
                if (nWidth != 0)
                {
                    if (nPrecision != 0)
                        VSIFPrintfL( fpCSVT, "(%d.%d)", nWidth, nPrecision);
                    else
                        VSIFPrintfL( fpCSVT, "(%d)", nWidth);
                }
            }
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
    if (eGeometryFormat == OGR_CSV_GEOM_AS_WKT)
    {
        OGRGeometry     *poGeom = poNewFeature->GetGeometryRef();
        char* pszWKT = NULL;
        if (poGeom && poGeom->exportToWkt(&pszWKT) == OGRERR_NONE)
        {
            VSIFPrintfL( fpCSV, "\"%s\"", pszWKT);
        }
        else
        {
            VSIFPrintfL( fpCSV, "\"\"");
        }
        CPLFree(pszWKT);
        if (poFeatureDefn->GetFieldCount() > 0)
            VSIFPrintfL( fpCSV, "%c", chDelimiter);
    }
    else if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ ||
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
/*      Write out all the field values.                                 */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        char *pszEscaped;
        
        if( iField > 0 )
            VSIFPrintfL( fpCSV, "%c", chDelimiter );
        
        pszEscaped = 
            CPLEscapeString( poNewFeature->GetFieldAsString(iField),
                            -1, CPLES_CSV );

        if (poFeatureDefn->GetFieldDefn(iField)->GetType() == OFTReal)
        {
            /* Use point as decimal separator */
            char* pszComma = strchr(pszEscaped, ',');
            if (pszComma)
                *pszComma = '.';
        }

        VSIFWriteL( pszEscaped, 1, strlen(pszEscaped), fpCSV );
        CPLFree( pszEscaped );
    }
    
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

void OGRCSVLayer::SetWriteGeometry(OGRCSVGeometryFormat eGeometryFormat)
{
    this->eGeometryFormat = eGeometryFormat;
}

/************************************************************************/
/*                          SetCreateCSVT()                             */
/************************************************************************/

void OGRCSVLayer::SetCreateCSVT(int bCreateCSVT)
{
    this->bCreateCSVT = bCreateCSVT;
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

int OGRCSVLayer::GetFeatureCount( int bForce )
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
        papszTokens = OGRCSVReadParseLineL( fpCSV, chDelimiter, bDontHonourStrings );
        if( papszTokens == NULL )
            break;

        if( papszTokens[0] != NULL )
            nTotalFeatures ++;

        CSLDestroy(papszTokens);
    }

    ResetReading();

    return nTotalFeatures;
}
