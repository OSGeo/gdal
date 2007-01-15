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
 ****************************************************************************/

#include "ogr_csv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRCSVLayer()                             */
/*                                                                      */
/*      Note that the OGRCSVLayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/

OGRCSVLayer::OGRCSVLayer( const char *pszLayerNameIn, 
                          FILE * fp, const char *pszFilename, int bNew, int bInWriteMode )

{
    fpCSV = fp;

    this->bInWriteMode = bInWriteMode;
    this->bNew = bNew;

    bUseCRLF = FALSE;
    bNeedRewind = FALSE;

    nNextFID = 1;

    poFeatureDefn = new OGRFeatureDefn( pszLayerNameIn );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

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
        papszTokens = CSVReadParseLine( fpCSV );
        nFieldCount = CSLCount( papszTokens );
        bHasFieldNames = TRUE;
    }
    else
        bHasFieldNames = FALSE;

    for( iField = 0; iField < nFieldCount && bHasFieldNames; iField++ )
    {
        const char *pszToken = papszTokens[iField];
        int bAllNumeric = TRUE;
        
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
        char *pszFieldName;
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
        }
        else
        {
            pszFieldName = szFieldNameBuffer;
            sprintf( szFieldNameBuffer, "field_%d", iField+1 );
        }

        OGRFieldDefn oField(pszFieldName, OFTString);
        if (papszFieldTypes!=NULL && iField<CSLCount(papszFieldTypes)) {
            if (EQUAL(papszFieldTypes[iField], "Integer"))
                oField.SetType(OFTInteger);
            else if (EQUAL(papszFieldTypes[iField], "Real"))
                oField.SetType(OFTReal);
            else if (EQUAL(papszFieldTypes[iField], "String"))
                oField.SetType(OFTString);
        }

        poFeatureDefn->AddFieldDefn( &oField );
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
    
    VSIFClose( fpCSV );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCSVLayer::ResetReading()

{
    VSIRewind( fpCSV );

    if( bHasFieldNames )
        CSLDestroy( CSVReadParseLine( fpCSV ) );

    bNeedRewind = FALSE;

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
    char **papszTokens = CSVReadParseLine( fpCSV );

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

    if( bNeedRewind )
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

        if( m_poAttrQuery == NULL || m_poAttrQuery->Evaluate( poFeature ) )
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

    bNeedRewind = TRUE;

/* -------------------------------------------------------------------- */
/*      Write field names if we haven't written them yet.               */
/* -------------------------------------------------------------------- */
    if( !bHasFieldNames )
    {
        for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            char *pszEscaped;

            if( iField > 0 )
                fprintf( fpCSV, "%s", "," );

            pszEscaped = 
                CPLEscapeString( poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), 
                                 -1, CPLES_CSV );

            VSIFPrintf( fpCSV, "%s", pszEscaped );
            CPLFree( pszEscaped );
        }
        if( bUseCRLF )
            VSIFPutc( 13, fpCSV );
        VSIFPutc( '\n', fpCSV );

        bHasFieldNames = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Make sure we are at the end of the file.                        */
/* -------------------------------------------------------------------- */
    VSIFSeek( fpCSV, 0, SEEK_END );

/* -------------------------------------------------------------------- */
/*      Write out all the field values.                                 */
/* -------------------------------------------------------------------- */
    
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        char *pszEscaped;
        
        if( iField > 0 )
            fprintf( fpCSV, "%s", "," );
        
        pszEscaped = 
            CPLEscapeString( poNewFeature->GetFieldAsString(iField),
                             -1, CPLES_CSV );
        
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
