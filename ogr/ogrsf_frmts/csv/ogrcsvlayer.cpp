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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2004/07/20 19:18:23  warmerda
 * New
 *
 */

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
                          FILE * fp )

{
    fpCSV = fp;

    nNextFID = 1;

    poFeatureDefn = new OGRFeatureDefn( pszLayerNameIn );
    poFeatureDefn->SetGeomType( wkbNone );

/* -------------------------------------------------------------------- */
/*      Check if the first record seems to be field definitions or      */
/*      not.  We assume it is field definitions if none of the          */
/*      values are strictly numeric.                                    */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSVReadParseLine( fpCSV );
    int nFieldCount = CSLCount(papszTokens);
    int iField;

    bHasFieldNames = TRUE;

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
            
        OGRFieldDefn oField( pszFieldName, OFTString );

        poFeatureDefn->AddFieldDefn( &oField );
    }

    CSLDestroy( papszTokens );
}

/************************************************************************/
/*                           ~OGRCSVLayer()                           */
/************************************************************************/

OGRCSVLayer::~OGRCSVLayer()

{
    delete poFeatureDefn;
    
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

    return poFeature;
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRCSVLayer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;
    
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
    return FALSE;
}

