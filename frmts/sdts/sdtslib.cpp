/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Various utility functions that apply to all SDTS profiles.
 * Author:   Frank Warmerdam, warmerda@home.com
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

/************************************************************************/
/*                           SDTSModId::Set()                           */
/*                                                                      */
/*      Set a module from a field.  We depend on our pre-knowledge      */
/*      of the data layout to fetch more efficiently.                   */
/************************************************************************/

int SDTSModId::Set( DDFField *poField )

{
    const char	*pachData = poField->GetData();

    memcpy( szModule, pachData, 4 );
    szModule[4] = '\0';

    nRecord = atoi( pachData + 4 );

    return FALSE;
}

/************************************************************************/
/*                            SDTSGetSADR()                             */
/*                                                                      */
/*      Extract the contents of a Spatial Address field.  Eventually    */
/*      this code should also apply the scaling according to the        */
/*      internal reference file ... it will have to be passed in        */
/*      then.                                                           */
/************************************************************************/

int SDTSGetSADR( SDTS_IREF *poIREF, DDFField * poField, int nVertices,
                 double *pdfX, double * pdfY, double * pdfZ )

{
    double	dfXScale = poIREF->dfXScale;
    double	dfYScale = poIREF->dfYScale;
    
    CPLAssert( poField->GetDataSize() >= nVertices * SDTS_SIZEOF_SADR );

/* -------------------------------------------------------------------- */
/*      For the sake of efficiency we depend on our knowledge that      */
/*      the SADR field is a series of bigendian int32's and decode      */
/*      them directly.                                                  */
/* -------------------------------------------------------------------- */
    GInt32	anXY[2];
    const char	*pachRawData = poField->GetData();

    for( int iVertex = 0; iVertex < nVertices; iVertex++ )
    {
        // we copy to a temp buffer to ensure it is world aligned.
        memcpy( anXY, pachRawData, 8 );
        pachRawData += 8;

        // possibly byte swap, and always apply scale factor
        pdfX[iVertex] = dfXScale * CPL_MSBWORD32( anXY[0] );
        pdfY[iVertex] = dfYScale * CPL_MSBWORD32( anXY[1] );
        pdfZ[iVertex] = 0.0;
    }
    
    return TRUE;
}

/************************************************************************/
/*                         SDTSModId::GetName()                         */
/************************************************************************/

const char * SDTSModId::GetName()

{
    static char		szName[20];

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
    DDFFieldDefn	*poIDField;
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
    DDFRecord	*poRecord;
    char	**papszModnList = NULL;
    
    poModule->Rewind();
    while( (poRecord = poModule->ReadRecord()) != NULL )
    {
        int	iField;
        
        for( iField = 0; iField < poRecord->GetFieldCount(); iField++ )
        {
            DDFField	*poField = poRecord->GetField( iField );

            if( poField->GetFieldDefn() == poIDField )
            {
                const char	*pszModName;
                int		i;

                for( i = 0; i < poField->GetRepeatCount(); i++ )
                {
                    char	szName[5];
                    
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


