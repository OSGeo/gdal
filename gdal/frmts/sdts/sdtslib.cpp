/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Various utility functions that apply to all SDTS profiles.
 *           SDTSModId, and SDTSFeature methods.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "sdts_al.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            SDTSFeature()                             */
/************************************************************************/

SDTSFeature::SDTSFeature() :
    nAttributes(0),
    paoATID(NULL)
{}

/************************************************************************/
/*                       SDTSFeature::ApplyATID()                       */
/************************************************************************/

void SDTSFeature::ApplyATID( DDFField * poField )

{
    DDFSubfieldDefn *poMODN
        = poField->GetFieldDefn()->FindSubfieldDefn( "MODN" );
    if( poMODN == NULL )
    {
        // CPLAssert( false );
        return;
    }

    bool bUsualFormat = poMODN->GetWidth() == 4;
    const int nRepeatCount = poField->GetRepeatCount();
    for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
    {
        paoATID = reinterpret_cast<SDTSModId *>(
          CPLRealloc( paoATID, sizeof(SDTSModId)*(nAttributes+1) ) );

        SDTSModId *poModId = paoATID + nAttributes;
        memset(poModId, 0, sizeof(SDTSModId));

        if( bUsualFormat )
        {
            const char * pabyData
                = poField->GetSubfieldData( poMODN, NULL, iRepeat );
            if( pabyData == NULL || strlen(pabyData) < 5 )
                return;

            memcpy( poModId->szModule, pabyData, 4 );
            poModId->szModule[4] = '\0';
            poModId->nRecord = atoi(pabyData + 4);
            poModId->szOBRP[0] = '\0';
        }
        else
        {
            poModId->Set( poField );
        }

        nAttributes++;
    }
}

/************************************************************************/
/*                            ~SDTSFeature()                            */
/************************************************************************/

SDTSFeature::~SDTSFeature()

{
    CPLFree( paoATID );
    paoATID = NULL;
}

/************************************************************************/
/*                           SDTSModId::Set()                           */
/*                                                                      */
/*      Set a module from a field.  We depend on our pre-knowledge      */
/*      of the data layout to fetch more efficiently.                   */
/************************************************************************/

int SDTSModId::Set( DDFField *poField )

{
    const char  *pachData = poField->GetData();
    DDFFieldDefn *poDefn = poField->GetFieldDefn();

    if( poDefn->GetSubfieldCount() >= 2
        && poDefn->GetSubfield(0)->GetWidth() == 4 )
    {
        if( strlen(pachData) < 5 )
            return FALSE;

        memcpy( szModule, pachData, 4 );
        szModule[4] = '\0';

        nRecord = atoi( pachData + 4 );
    }
    else
    {
        DDFSubfieldDefn *poSF
            = poField->GetFieldDefn()->FindSubfieldDefn( "MODN" );
        if( poSF == NULL )
            return FALSE;
        int nBytesRemaining;
        pachData = poField->GetSubfieldData(poSF, &nBytesRemaining);
        if( pachData == NULL )
            return FALSE;
        snprintf( szModule, sizeof(szModule), "%s",
                 poSF->ExtractStringData( pachData, nBytesRemaining, NULL) );

        poSF = poField->GetFieldDefn()->FindSubfieldDefn( "RCID" );
        if( poSF != NULL )
        {
            pachData = poField->GetSubfieldData(poSF, &nBytesRemaining);
            if( pachData != NULL )
                nRecord = poSF->ExtractIntData( pachData, nBytesRemaining, NULL);
        }
    }

    if( poDefn->GetSubfieldCount() == 3 )
    {
        DDFSubfieldDefn *poSF = poField->GetFieldDefn()->FindSubfieldDefn( "OBRP" );
        if( poSF != NULL )
        {
            int nBytesRemaining;
            pachData
                = poField->GetSubfieldData(poSF, &nBytesRemaining);
            if( pachData != NULL )
            {
                snprintf( szOBRP, sizeof(szOBRP), "%s",
                        poSF->ExtractStringData( pachData, nBytesRemaining, NULL) );
            }
        }
    }

    return FALSE;
}

/************************************************************************/
/*                         SDTSModId::GetName()                         */
/************************************************************************/

const char * SDTSModId::GetName()

{
    snprintf( szName, sizeof(szName), "%s:%d", szModule, nRecord );

    return szName;
}

/************************************************************************/
/*                      SDTSScanModuleReferences()                      */
/*                                                                      */
/*      Find all modules references by records in this module based     */
/*      on a particular field name.  That field must be in module       */
/*      reference form (contain MODN/RCID subfields).                   */
/************************************************************************/

char **SDTSScanModuleReferences( DDFModule * poModule, const char * pszFName )

{
/* -------------------------------------------------------------------- */
/*      Identify the field, and subfield we are interested in.          */
/* -------------------------------------------------------------------- */
    DDFFieldDefn *poIDField = poModule->FindFieldDefn( pszFName );

    if( poIDField == NULL )
        return NULL;

    DDFSubfieldDefn *poMODN = poIDField->FindSubfieldDefn( "MODN" );
    if( poMODN == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Scan the file.                                                  */
/* -------------------------------------------------------------------- */
    poModule->Rewind();

    DDFRecord *poRecord = NULL;
    char **papszModnList = NULL;
    while( (poRecord = poModule->ReadRecord()) != NULL )
    {
        for( int iField = 0; iField < poRecord->GetFieldCount(); iField++ )
        {
            DDFField    *poField = poRecord->GetField( iField );

            if( poField->GetFieldDefn() == poIDField )
            {
                for( int i = 0; i < poField->GetRepeatCount(); i++ )
                {
                    const char *pszModName
                        = poField->GetSubfieldData(poMODN, NULL, i);

                    if( pszModName == NULL || strlen(pszModName) < 4 )
                        continue;

                    char szName[5];
                    strncpy( szName, pszModName, 4 );
                    szName[4] = '\0';

                    if( CSLFindString( papszModnList, szName ) == -1 )
                        papszModnList = CSLAddString( papszModnList, szName );
                }
            }
        }
    }

    poModule->Rewind();

    return papszModnList;
}
