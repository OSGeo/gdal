/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFAEntry class for reading and relating
 *           one node in the HFA object tree structure. 
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 * hfaentry.cpp
 *
 * Implementation of the HFAEntry class.
 *
 * $Log$
 * Revision 1.3  1999/01/22 17:37:30  warmerda
 * fixed types in GetFieldValue() calls
 *
 * Revision 1.2  1999/01/04 22:52:47  warmerda
 * field access working
 *
 * Revision 1.1  1999/01/04 05:28:12  warmerda
 * New
 *
 */

#include "hfa_p.h"
#include "cpl_conv.h"

/************************************************************************/
/*                              HFAEntry()                              */
/************************************************************************/

HFAEntry::HFAEntry( HFAInfo_t * psHFAIn, GUInt32 nPos,
                    HFAEntry * poParentIn, HFAEntry * poPrevIn )

{
    psHFA = psHFAIn;
    
    nFilePos = nPos;

    poParent = poParentIn;
    poPrev = poPrevIn;

/* -------------------------------------------------------------------- */
/*      Initialize fields to null values in case there is a read        */
/*      error, so the entry will be in a harmless state.                */
/* -------------------------------------------------------------------- */
    poNext = poChild = NULL;

    nDataPos = nDataSize = 0;
    nNextPos = nChildPos = 0;

    szName[0] = szType[0] = '\0';

    pabyData = NULL;

    poType = NULL;

/* -------------------------------------------------------------------- */
/*      Read the entry information from the file.                       */
/* -------------------------------------------------------------------- */
    GInt32	anEntryNums[6];
    int		i;

    if( VSIFSeek( psHFA->fp, nFilePos, SEEK_SET ) == -1
        || VSIFRead( anEntryNums, sizeof(GInt32), 6, psHFA->fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFRead() failed in HFAEntry()." );
        return;
    }

    for( i = 0; i < 6; i++ )
        HFAStandard( 4, anEntryNums + i );

    nNextPos = anEntryNums[0];
    nChildPos = anEntryNums[3];
    nDataPos = anEntryNums[4];
    nDataSize = anEntryNums[5];

/* -------------------------------------------------------------------- */
/*      Read the name, and type.                                        */
/* -------------------------------------------------------------------- */
    if( VSIFRead( szName, 1, 64, psHFA->fp ) < 1
        || VSIFRead( szType, 1, 32, psHFA->fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFRead() failed in HFAEntry()." );
        return;
    }
}

/************************************************************************/
/*                             ~HFAEntry()                              */
/*                                                                      */
/*      Ensure that children are cleaned up when this node is           */
/*      cleaned up.                                                     */
/************************************************************************/

HFAEntry::~HFAEntry()

{
    CPLFree( pabyData );
    
    if( poNext != NULL )
        delete poNext;

    if( poChild != NULL )
        delete poChild;
}

/************************************************************************/
/*                              GetChild()                              */
/************************************************************************/

HFAEntry *HFAEntry::GetChild()

{
/* -------------------------------------------------------------------- */
/*      Do we need to create the child node?                            */
/* -------------------------------------------------------------------- */
    if( poChild == NULL && nChildPos != 0 )
    {
        poChild = new HFAEntry( psHFA, nChildPos, this, NULL );
    }

    return( poChild );
}

/************************************************************************/
/*                              GetNext()                               */
/************************************************************************/

HFAEntry *HFAEntry::GetNext()

{
/* -------------------------------------------------------------------- */
/*      Do we need to create the next node?                             */
/* -------------------------------------------------------------------- */
    if( poNext == NULL && nNextPos != 0 )
    {
        poNext = new HFAEntry( psHFA, nNextPos, poParent, this );
    }

    return( poNext );
}

/************************************************************************/
/*                              LoadData()                              */
/*                                                                      */
/*      Load the data for this entry, and build up the field            */
/*      information for it.                                             */
/************************************************************************/

void HFAEntry::LoadData()

{
    if( pabyData != NULL || nDataSize == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Allocate buffer, and read data.                                 */
/* -------------------------------------------------------------------- */
    pabyData = (GByte *) CPLMalloc(nDataSize);
    if( VSIFSeek( psHFA->fp, nDataPos, SEEK_SET ) < 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFSeek() failed in HFAEntry::LoadData()." );
        return;
    }

    if( VSIFRead( pabyData, 1, nDataSize, psHFA->fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFRead() failed in HFAEntry::LoadData()." );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Get the type corresponding to this entry.                       */
/* -------------------------------------------------------------------- */
    poType = psHFA->poDictionary->FindType( szType );
    if( poType == NULL )
        return;
}

/************************************************************************/
/*                          DumpFieldValues()                           */
/************************************************************************/

void HFAEntry::DumpFieldValues( FILE * fp, const char * pszPrefix )

{
    if( pszPrefix == NULL )
        pszPrefix = "";

    LoadData();

    if( poType == NULL )
        return;

    poType->DumpInstValue( fp,
                           pabyData, nDataPos, nDataSize,
                           pszPrefix );
}

/************************************************************************/
/*                           GetNamedChild()                            */
/************************************************************************/

HFAEntry *HFAEntry::GetNamedChild( const char * pszName )

{
    int		nNameLen;
    HFAEntry	*poEntry;

/* -------------------------------------------------------------------- */
/*      Establish how much of this name path is for the next child.     */
/*      Up to the '.' or end of estring.                                */
/* -------------------------------------------------------------------- */
    for( nNameLen = 0;
         pszName[nNameLen] != '.'
             && pszName[nNameLen] != '\0'
             && pszName[nNameLen] != ':';
         nNameLen++ ) {}

/* -------------------------------------------------------------------- */
/*      Scan children looking for this name.                            */
/* -------------------------------------------------------------------- */
    for( poEntry = GetChild(); poEntry != NULL; poEntry = poEntry->GetNext() )
    {
        if( EQUALN(poEntry->GetName(),pszName,nNameLen)
            && (int) strlen(poEntry->GetName()) == nNameLen )
        {
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Is there a remainder to process?                                */
/* -------------------------------------------------------------------- */
    if( poEntry != NULL && pszName[nNameLen] == '.' )
        return( poEntry->GetNamedChild( pszName+nNameLen+1 ) );
    else
        return( poEntry );
}

/************************************************************************/
/*                           GetFieldValue()                            */
/************************************************************************/
        
void *HFAEntry::GetFieldValue( const char * pszFieldPath,
                               char chReqType )

{
    HFAEntry	*poEntry = this;
    
/* -------------------------------------------------------------------- */
/*      Is there a node path in this string?                            */
/* -------------------------------------------------------------------- */
    if( strchr(pszFieldPath,':') != NULL )
    {
        poEntry = GetNamedChild( pszFieldPath );
        if( poEntry == NULL )
            return NULL;
        
        pszFieldPath = strchr(pszFieldPath,':') + 1;
    }

/* -------------------------------------------------------------------- */
/*      Do we have the data and type for this node?                     */
/* -------------------------------------------------------------------- */
    LoadData();

    if( pabyData == NULL )
        return NULL;
    
    if( poType == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Extract the instance information.                               */
/* -------------------------------------------------------------------- */

    return( poType->ExtractInstValue( pszFieldPath,
                                      pabyData, nDataPos, nDataSize,
                                      chReqType ) );
}

/************************************************************************/
/*                            GetIntField()                             */
/************************************************************************/

int HFAEntry::GetIntField( const char * pszFieldPath, CPLErr *peErr )

{
    void	*pRetData;

    pRetData = GetFieldValue( pszFieldPath, 'i' );
    if( pRetData == NULL )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return 0;
    }
    else
    {
        if( peErr != NULL )
            *peErr = CE_None;

        return *((int *) pRetData);
    }
}

/************************************************************************/
/*                           GetDoubleField()                           */
/************************************************************************/

double HFAEntry::GetDoubleField( const char * pszFieldPath, CPLErr *peErr )

{
    void	*pRetData;

    pRetData = GetFieldValue( pszFieldPath, 'd' );
    if( pRetData == NULL )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return 0.0;
    }
    else
    {
        if( peErr != NULL )
            *peErr = CE_None;

        return *((double *) pRetData);
    }
}

/************************************************************************/
/*                           GetStringField()                           */
/************************************************************************/

const char *HFAEntry::GetStringField( const char * pszFieldPath, CPLErr *peErr)

{
    void	*pRetData;

    pRetData = GetFieldValue( pszFieldPath, 's' );
    if( pRetData == NULL )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return NULL;
    }
    else
    {
        if( peErr != NULL )
            *peErr = CE_None;

        return (char *) pRetData;
    }
}



