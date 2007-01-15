/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Various utility functions that apply to all SDTS profiles.
 *           SDTSModId, and SDTSFeature methods. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.14  2006/04/10 16:34:19  fwarmerdam
 * updated contact info
 *
 * Revision 1.13  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.12  2001/01/19 21:20:29  warmerda
 * expanded tabs
 *
 * Revision 1.11  2000/04/14 15:00:53  warmerda
 * Ensure that SDTSModId extraction works for variable sized MODN
 *
 * Revision 1.10  1999/11/04 22:52:52  warmerda
 * added dynamic ATID support
 *
 * Revision 1.9  1999/09/21 17:26:31  warmerda
 * generalized SADR reading
 *
 * Revision 1.8  1999/09/12 23:42:23  warmerda
 * treat coords as signed
 *
 * Revision 1.7  1999/09/02 03:40:03  warmerda
 * added indexed readers
 *
 * Revision 1.6  1999/08/16 20:59:50  warmerda
 * added szOBRP support for SDTSModId
 *
 * Revision 1.5  1999/08/10 02:52:13  warmerda
 * introduce use of SDTSApplyModIdList to capture multi-attributes
 *
 * Revision 1.4  1999/07/30 19:15:56  warmerda
 * added module reference counting
 *
 * Revision 1.3  1999/05/11 12:55:54  warmerda
 * added GetName() method to SDTSModId
 *
 * Revision 1.2  1999/05/07 13:45:01  warmerda
 * major upgrade to use iso8211lib
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            SDTSFeature()                             */
/************************************************************************/

SDTSFeature::SDTSFeature()

{
    nAttributes = 0;
    paoATID = NULL;
}

/************************************************************************/
/*                       SDTSFeature::ApplyATID()                       */
/************************************************************************/

void SDTSFeature::ApplyATID( DDFField * poField )

{
    int         nRepeatCount = poField->GetRepeatCount();
    int         bUsualFormat;
    DDFSubfieldDefn *poMODN;

    poMODN = poField->GetFieldDefn()->FindSubfieldDefn( "MODN" );
    if( poMODN == NULL )
    {
        CPLAssert( FALSE );
        return;
    }
    
    bUsualFormat = poMODN->GetWidth() == 4;
    for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
    {
        paoATID = (SDTSModId *) CPLRealloc(paoATID,
                                           sizeof(SDTSModId)*(nAttributes+1));

        const char * pabyData;
        SDTSModId *poModId = paoATID + nAttributes;

        if( bUsualFormat )
        {
            pabyData = poField->GetSubfieldData( poMODN, NULL, iRepeat );
            
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
        memcpy( szModule, pachData, 4 );
        szModule[4] = '\0';

        nRecord = atoi( pachData + 4 );
    }
    else
    {
        DDFSubfieldDefn *poSF;
        int             nBytesRemaining;
        const char  *pachData;

        szModule[4] = '\0';
        poSF = poField->GetFieldDefn()->FindSubfieldDefn( "MODN" );
        pachData = poField->GetSubfieldData(poSF, &nBytesRemaining);
        strncpy( szModule,
                 poSF->ExtractStringData( pachData, nBytesRemaining, NULL),
                 sizeof(szModule) );
            
        poSF = poField->GetFieldDefn()->FindSubfieldDefn( "RCID" );
        pachData = poField->GetSubfieldData(poSF, &nBytesRemaining);
        nRecord = poSF->ExtractIntData( pachData, nBytesRemaining, NULL);
    }

    if( poDefn->GetSubfieldCount() == 3 )
    {
        DDFSubfieldDefn         *poSF;

        poSF = poField->GetFieldDefn()->FindSubfieldDefn( "OBRP" );
        if( poSF != NULL )
        {
            int         nBytesRemaining;
            const char  *pachData;

            pachData = poField->GetSubfieldData(poSF, &nBytesRemaining);
            strncpy( szOBRP, 
                     poSF->ExtractStringData( pachData, nBytesRemaining, NULL),
                     sizeof(szOBRP) );
            
            szOBRP[sizeof(szOBRP)-1] = '\0';
        }
    }

    return FALSE;
}

/************************************************************************/
/*                         SDTSModId::GetName()                         */
/************************************************************************/

const char * SDTSModId::GetName()

{
    static char         szName[20];

    sprintf( szName, "%s:%ld", szModule, nRecord );

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
    DDFFieldDefn        *poIDField;
    DDFSubfieldDefn     *poMODN;

    poIDField = poModule->FindFieldDefn( pszFName );

    if( poIDField == NULL )
        return NULL;

    poMODN = poIDField->FindSubfieldDefn( "MODN" );
    if( poMODN == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Scan the file.                                                  */
/* -------------------------------------------------------------------- */
    DDFRecord   *poRecord;
    char        **papszModnList = NULL;
    
    poModule->Rewind();
    while( (poRecord = poModule->ReadRecord()) != NULL )
    {
        int     iField;
        
        for( iField = 0; iField < poRecord->GetFieldCount(); iField++ )
        {
            DDFField    *poField = poRecord->GetField( iField );

            if( poField->GetFieldDefn() == poIDField )
            {
                const char      *pszModName;
                int             i;

                for( i = 0; i < poField->GetRepeatCount(); i++ )
                {
                    char        szName[5];
                    
                    pszModName = poField->GetSubfieldData(poMODN,NULL,i);

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


