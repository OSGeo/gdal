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
/*                            OGRCSVLayer()                             */
/*                                                                      */
/*      Note that the OGRCSVLayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/

OGRCSVLayer::OGRCSVLayer( const char *pszLayerNameIn, 
                          FILE * fp, const char *pszFilename, int bNew, int bInWriteMode,
                          char chDelimiter)

{
    fpCSV = fp;

    iWktGeomReadField = -1;
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

/* -------------------------------------------------------------------- */
/*      If this is not a new file, read ahead to establish if it is     */
/*      already in CRLF (DOS) mode, or just a normal unix CR mode.      */
/* -------------------------------------------------------------------- */
    if( !bNew )
    {
        int nBytesRead = 0;
        char chNewByte;

        while( nBytesRead < 10000 && VSIFRead( &chNewByte, 1, 1, fpCSV ) == 1 )
        {
            if( chNewByte == 13 )
            {
                bUseCRLF = TRUE;
                break;
            }
        }
        VSIRewind( fpCSV );
    }

/* -------------------------------------------------------------------- */
/*      Check if the first record seems to be field definitions or      */
/*      not.  We assume it is field definitions if none of the          */
/*      values are strictly numeric.                                    */
/* -------------------------------------------------------------------- */
    char **papszTokens = NULL;
    int nFieldCount=0, iField;

    if( !bNew )
    {
        papszTokens = CSVReadParseLine2( fpCSV, chDelimiter );
        nFieldCount = CSLCount( papszTokens );
        bHasFieldNames = TRUE;
    }
    else
        bHasFieldNames = FALSE;

    for( iField = 0; iField < nFieldCount && bHasFieldNames; iField++ )
    {
        const char *pszToken = papszTokens[iField];
        int bAllNumeric = TRUE;

        if (*pszToken != '\0')
        {
            while( *pszToken != '\0' && bAllNumeric )
            {
                if( *pszToken != '.' && *pszToken != '-'
                    && (*pszToken < '0' || *pszToken > '9') )
                    bAllNumeric = FALSE;
                pszToken++;
            }

            if( bAllNumeric )
                bHasFieldNames = FALSE;
        }
    }

    if( !bHasFieldNames )
        VSIRewind( fpCSV );


/* -------------------------------------------------------------------- */
/*      Search a csvt file for types                                */
/* -------------------------------------------------------------------- */
    char** papszFieldTypes = NULL;
    if (!bNew) {
        char* dname = strdup(CPLGetDirname(pszFilename));
        char* fname = strdup(CPLGetBasename(pszFilename));
        FILE* fpCSVT = fopen(CPLFormFilename(dname, fname, ".csvt"), "r");
        free(dname);
        free(fname);
        if (fpCSVT!=NULL) {
            VSIRewind(fpCSVT);
            papszFieldTypes = CSVReadParseLine(fpCSVT);
            fclose(fpCSVT);
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

        poFeatureDefn->AddFieldDefn( &oField );

        if( EQUAL(oField.GetNameRef(),"WKT")
            && oField.GetType() == OFTString 
            && iWktGeomReadField == -1 )
        {
            iWktGeomReadField = iField;
            poFeatureDefn->SetGeomType( wkbUnknown );
        }
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
    
    VSIFClose( fpCSV );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCSVLayer::ResetReading()

{
    VSIRewind( fpCSV );

    if( bHasFieldNames )
        CSLDestroy( CSVReadParseLine2( fpCSV, chDelimiter ) );

    bNeedRewindBeforeRead = FALSE;

    nNextFID = 1;
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature * OGRCSVLayer::GetNextUnfilteredFeature()

{
/* -------------------------------------------------------------------- */
/*      Read the CSV record.                                            */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSVReadParseLine2( fpCSV, chDelimiter );

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
    int         nAttrCount = MIN(CSLCount(papszTokens),
                                 poFeatureDefn->GetFieldCount() );
    
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

        if (poFeatureDefn->GetFieldDefn(iAttr)->GetType() == OFTReal)
        {
            if (papszTokens[iAttr][0] != '\0')
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
        FILE* fpCSVT = NULL;
        if (bCreateCSVT)
        {
            char* pszDirName = CPLStrdup(CPLGetDirname(pszFilename));
            char* pszBaseName = CPLStrdup(CPLGetBasename(pszFilename));
            fpCSVT = fopen(CPLFormFilename(pszDirName, pszBaseName, ".csvt"), "wt");
            CPLFree(pszDirName);
            CPLFree(pszBaseName);
        }

        if (eGeometryFormat == OGR_CSV_GEOM_AS_WKT)
        {
            VSIFPrintf( fpCSV, "%s", "WKT");
            if (fpCSVT) VSIFPrintf( fpCSVT, "%s", "String");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                VSIFPrintf( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintf( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ)
        {
            VSIFPrintf( fpCSV, "X%cY%cZ", chDelimiter, chDelimiter);
            if (fpCSVT) VSIFPrintf( fpCSVT, "%s", "Real,Real,Real");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                VSIFPrintf( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintf( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_XY)
        {
            VSIFPrintf( fpCSV, "X%cY", chDelimiter);
            if (fpCSVT) VSIFPrintf( fpCSVT, "%s", "Real,Real");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                VSIFPrintf( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintf( fpCSVT, "%s", ",");
            }
        }
        else if (eGeometryFormat == OGR_CSV_GEOM_AS_YX)
        {
            VSIFPrintf( fpCSV, "Y%cX", chDelimiter);
            if (fpCSVT) VSIFPrintf( fpCSVT, "%s", "Real,Real");
            if (poFeatureDefn->GetFieldCount() > 0)
            {
                VSIFPrintf( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintf( fpCSVT, "%s", ",");
            }
        }

        for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            char *pszEscaped;

            if( iField > 0 )
            {
                VSIFPrintf( fpCSV, "%c", chDelimiter );
                if (fpCSVT) VSIFPrintf( fpCSVT, "%s", ",");
            }

            pszEscaped = 
                CPLEscapeString( poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), 
                                 -1, CPLES_CSV );

            VSIFPrintf( fpCSV, "%s", pszEscaped );
            CPLFree( pszEscaped );

            if (fpCSVT)
            {
                switch( poFeatureDefn->GetFieldDefn(iField)->GetType() )
                {
                    case OFTInteger:  VSIFPrintf( fpCSVT, "%s", "Integer"); break;
                    case OFTReal:     VSIFPrintf( fpCSVT, "%s", "Real"); break;
                    case OFTDate:     VSIFPrintf( fpCSVT, "%s", "Date"); break;
                    case OFTTime:     VSIFPrintf( fpCSVT, "%s", "Time"); break;
                    case OFTDateTime: VSIFPrintf( fpCSVT, "%s", "DateTime"); break;
                    default:          VSIFPrintf( fpCSVT, "%s", "String"); break;
                }

                int nWidth = poFeatureDefn->GetFieldDefn(iField)->GetWidth();
                int nPrecision = poFeatureDefn->GetFieldDefn(iField)->GetPrecision();
                if (nWidth != 0)
                {
                    if (nPrecision != 0)
                        VSIFPrintf( fpCSVT, "(%d.%d)", nWidth, nPrecision);
                    else
                        VSIFPrintf( fpCSVT, "(%d)", nWidth);
                }
            }
        }
        if( bUseCRLF )
        {
            VSIFPutc( 13, fpCSV );
            if (fpCSVT) VSIFPutc( 13, fpCSVT );
        }
        VSIFPutc( '\n', fpCSV );
        if (fpCSVT) VSIFPutc( '\n', fpCSVT );
        if (fpCSVT) VSIFClose(fpCSVT);

        bHasFieldNames = TRUE;
        bNeedSeekEnd = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Make sure we are at the end of the file.                        */
/* -------------------------------------------------------------------- */
    if (bNeedSeekEnd)
    {
        if (bFirstFeatureAppendedDuringSession)
        {
            /* Add a newline character to the end of the file if necessary */
            bFirstFeatureAppendedDuringSession = FALSE;
            VSIFSeek( fpCSV, 0, SEEK_END );
            VSIFSeek( fpCSV, VSIFTell(fpCSV) - 1, SEEK_SET);
            char chLast = VSIFGetc( fpCSV );
            VSIFSeek( fpCSV, 0, SEEK_END );
            if (chLast != '\n')
            {
                if( bUseCRLF )
                    VSIFPutc( 13, fpCSV );
                VSIFPutc( '\n', fpCSV );
            }
        }
        else
        {
            VSIFSeek( fpCSV, 0, SEEK_END );
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
            VSIFPrintf( fpCSV, "\"%s\"", pszWKT);
        }
        else
        {
            VSIFPrintf( fpCSV, "\"\"");
        }
        CPLFree(pszWKT);
        if (poFeatureDefn->GetFieldCount() > 0)
            VSIFPrintf( fpCSV, "%c", chDelimiter);
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
            VSIFPrintf( fpCSV, "%s", szBuffer );
        }
        else
        {
            VSIFPrintf( fpCSV, "%c", chDelimiter );
            if (eGeometryFormat == OGR_CSV_GEOM_AS_XYZ)
                VSIFPrintf( fpCSV, "%c", chDelimiter );
        }
        if (poFeatureDefn->GetFieldCount() > 0)
            VSIFPrintf( fpCSV, "%c", chDelimiter );
    }

/* -------------------------------------------------------------------- */
/*      Write out all the field values.                                 */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        char *pszEscaped;
        
        if( iField > 0 )
            fprintf( fpCSV, "%c", chDelimiter );
        
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

        VSIFWrite( pszEscaped, 1, strlen(pszEscaped), fpCSV );
        CPLFree( pszEscaped );
    }
    
    if( bUseCRLF )
        VSIFPutc( 13, fpCSV );
    VSIFPutc( '\n', fpCSV );

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
