/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFAEntry class for reading and relating
 *           one node in the HFA object tree structure. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 */

#include "hfa_p.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                              HFAEntry()                              */
/*                                                                      */
/*      Construct an HFAEntry from the source file.                     */
/************************************************************************/

HFAEntry::HFAEntry( HFAInfo_t * psHFAIn, GUInt32 nPos,
                    HFAEntry * poParentIn, HFAEntry * poPrevIn )

{
    psHFA = psHFAIn;
    
    nFilePos = nPos;
    bDirty = FALSE;

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

    if( VSIFSeekL( psHFA->fp, nFilePos, SEEK_SET ) == -1
        || VSIFReadL( anEntryNums, sizeof(GInt32), 6, psHFA->fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFReadL() failed in HFAEntry()." );
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
    if( VSIFReadL( szName, 1, 64, psHFA->fp ) < 1
        || VSIFReadL( szType, 1, 32, psHFA->fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFReadL() failed in HFAEntry()." );
        return;
    }
}

/************************************************************************/
/*                              HFAEntry()                              */
/*                                                                      */
/*      Construct an HFAEntry in memory, with the intention that it     */
/*      would be written to disk later.                                 */
/************************************************************************/

HFAEntry::HFAEntry( HFAInfo_t * psHFAIn, 
                    const char * pszNodeName, 
                    const char * pszTypeName,
                    HFAEntry * poParentIn )

{
/* -------------------------------------------------------------------- */
/*      Initialize Entry                                                */
/* -------------------------------------------------------------------- */
    psHFA = psHFAIn;
    
    nFilePos = 0;

    poParent = poParentIn;
    poPrev = poNext = poChild = NULL;

    nDataPos = nDataSize = 0;
    nNextPos = nChildPos = 0;

    SetName( pszNodeName );
    memset( szType, 0, 32 );
    strncpy( szType, pszTypeName, 32 );

    pabyData = NULL;
    poType = NULL;

/* -------------------------------------------------------------------- */
/*      Update the previous or parent node to refer to this one.        */
/* -------------------------------------------------------------------- */
    if( poParent == NULL )
    {
        /* do nothing */
    }
    else if( poParent->poChild == NULL )
    {
        poParent->poChild = this;
        poParent->MarkDirty();
    }
    else
    {
        poPrev = poParent->poChild;
        while( poPrev->poNext != NULL )
            poPrev = poPrev->poNext;

        poPrev->poNext = this;
        poPrev->MarkDirty();
    }

    MarkDirty();
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
/*                              SetName()                               */
/*                                                                      */
/*    Changes the name assigned to this node                            */
/************************************************************************/

void HFAEntry::SetName( const char *pszNodeName )
{
  memset( szName, 0, 64 );
  strncpy( szName, pszNodeName, 64 );

  MarkDirty();
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
    if( VSIFSeekL( psHFA->fp, nDataPos, SEEK_SET ) < 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFSeekL() failed in HFAEntry::LoadData()." );
        return;
    }

    if( VSIFReadL( pabyData, 1, nDataSize, psHFA->fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "VSIFReadL() failed in HFAEntry::LoadData()." );
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
/*                              MakeData()                              */
/*                                                                      */
/*      Create a data block on the this HFAEntry in memory.  By         */
/*      default it will create the data the correct size for fixed      */
/*      sized types, or do nothing for variable length types.           */
/*      However, the caller can supply a desired size for variable      */
/*      sized fields.                                                   */
/************************************************************************/

GByte *HFAEntry::MakeData( int nSize )

{
    if( poType == NULL )
    {
        poType = psHFA->poDictionary->FindType( szType );
        if( poType == NULL )
            return NULL;
    }

    if( nSize == 0 && poType->nBytes > 0 )
        nSize = poType->nBytes;

    if( (int) nDataSize < nSize && nSize > 0 )
    {
        pabyData = (GByte *) CPLRealloc(pabyData, nSize);
        memset( pabyData + nDataSize, 0, nSize - nDataSize );
        nDataSize = nSize;

        MarkDirty();
    }

    return pabyData;
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
            if( pszName[nNameLen] == '.' )
            {
                HFAEntry *poResult;

                poResult = poEntry->GetNamedChild( pszName+nNameLen+1 );
                if( poResult != NULL )
                    return poResult;
            }
            else
                return poEntry;
        }
    }

    return NULL;
}

/************************************************************************/
/*                           GetFieldValue()                            */
/************************************************************************/
        
int HFAEntry::GetFieldValue( const char * pszFieldPath,
                             char chReqType, void *pReqReturn )

{
    HFAEntry	*poEntry = this;
    
/* -------------------------------------------------------------------- */
/*      Is there a node path in this string?                            */
/* -------------------------------------------------------------------- */
    if( strchr(pszFieldPath,':') != NULL )
    {
        poEntry = GetNamedChild( pszFieldPath );
        if( poEntry == NULL )
            return FALSE;
        
        pszFieldPath = strchr(pszFieldPath,':') + 1;
    }

/* -------------------------------------------------------------------- */
/*      Do we have the data and type for this node?                     */
/* -------------------------------------------------------------------- */
    LoadData();

    if( pabyData == NULL )
        return FALSE;
    
    if( poType == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Extract the instance information.                               */
/* -------------------------------------------------------------------- */


    return( poType->ExtractInstValue( pszFieldPath,
                                      pabyData, nDataPos, nDataSize,
                                      chReqType, pReqReturn ) );
}

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

int HFAEntry::GetFieldCount( const char * pszFieldPath, CPLErr *peErr )

{
    HFAEntry	*poEntry = this;
    
/* -------------------------------------------------------------------- */
/*      Is there a node path in this string?                            */
/* -------------------------------------------------------------------- */
    if( strchr(pszFieldPath,':') != NULL )
    {
        poEntry = GetNamedChild( pszFieldPath );
        if( poEntry == NULL )
            return -1;
        
        pszFieldPath = strchr(pszFieldPath,':') + 1;
    }

/* -------------------------------------------------------------------- */
/*      Do we have the data and type for this node?                     */
/* -------------------------------------------------------------------- */
    LoadData();

    if( pabyData == NULL )
        return -1;
    
    if( poType == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Extract the instance information.                               */
/* -------------------------------------------------------------------- */

    return( poType->GetInstCount( pszFieldPath,
                                  pabyData, nDataPos, nDataSize ) );
}

/************************************************************************/
/*                            GetIntField()                             */
/************************************************************************/

GInt32 HFAEntry::GetIntField( const char * pszFieldPath, CPLErr *peErr )

{
    GInt32	nIntValue;

    if( !GetFieldValue( pszFieldPath, 'i', &nIntValue ) )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return 0;
    }
    else
    {
        if( peErr != NULL )
            *peErr = CE_None;

        return nIntValue;
    }
}

/************************************************************************/
/*                           GetBigIntField()                           */
/*                                                                      */
/*      This is just a helper method that reads two ULONG array         */
/*      entries as a GBigInt.  The passed name should be the name of    */
/*      the array with no array index.  Array indexes 0 and 1 will      */
/*      be concatenated.                                                */
/************************************************************************/

GIntBig HFAEntry::GetBigIntField( const char *pszFieldPath, CPLErr *peErr )

{
    GUInt32 nLower, nUpper;
    char szFullFieldPath[1024];

    sprintf( szFullFieldPath, "%s[0]", pszFieldPath );
    nLower = GetIntField( szFullFieldPath, peErr );
    if( peErr != NULL && *peErr != CE_None )
        return 0;

    sprintf( szFullFieldPath, "%s[1]", pszFieldPath );
    nUpper = GetIntField( szFullFieldPath, peErr );
    if( peErr != NULL && *peErr != CE_None )
        return 0;

    return nLower + (((GIntBig) nUpper) << 32);
}

/************************************************************************/
/*                           GetDoubleField()                           */
/************************************************************************/

double HFAEntry::GetDoubleField( const char * pszFieldPath, CPLErr *peErr )

{
    double dfDoubleValue;

    if( !GetFieldValue( pszFieldPath, 'd', &dfDoubleValue ) )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return 0.0;
    }
    else
    {
        if( peErr != NULL )
            *peErr = CE_None;

        return dfDoubleValue;
    }
}

/************************************************************************/
/*                           GetStringField()                           */
/************************************************************************/

const char *HFAEntry::GetStringField( const char * pszFieldPath, CPLErr *peErr)

{
    char *pszResult = NULL;

    if( !GetFieldValue( pszFieldPath, 's', &pszResult ) )
    {
        if( peErr != NULL )
            *peErr = CE_Failure;

        return NULL;
    }
    else
    {
        if( peErr != NULL )
            *peErr = CE_None;

        return pszResult;
    }
}

/************************************************************************/
/*                           SetFieldValue()                            */
/************************************************************************/
        
CPLErr HFAEntry::SetFieldValue( const char * pszFieldPath,
                                char chReqType, void *pValue )

{
    HFAEntry	*poEntry = this;
    
/* -------------------------------------------------------------------- */
/*      Is there a node path in this string?                            */
/* -------------------------------------------------------------------- */
    if( strchr(pszFieldPath,':') != NULL )
    {
        poEntry = GetNamedChild( pszFieldPath );
        if( poEntry == NULL )
            return CE_Failure;
        
        pszFieldPath = strchr(pszFieldPath,':') + 1;
    }

/* -------------------------------------------------------------------- */
/*      Do we have the data and type for this node?  Try loading        */
/*      from a file, or instantiating a new node.                       */
/* -------------------------------------------------------------------- */
    LoadData();
    if( MakeData() == NULL 
        || pabyData == NULL
        || poType == NULL )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Extract the instance information.                               */
/* -------------------------------------------------------------------- */
    MarkDirty();

    return( poType->SetInstValue( pszFieldPath,
                                  pabyData, nDataPos, nDataSize,
                                  chReqType, pValue ) );
}

/************************************************************************/
/*                           SetStringField()                           */
/************************************************************************/

CPLErr HFAEntry::SetStringField( const char * pszFieldPath, 
                                 const char * pszValue )

{
    return SetFieldValue( pszFieldPath, 's', (void *) pszValue );
}

/************************************************************************/
/*                            SetIntField()                             */
/************************************************************************/

CPLErr HFAEntry::SetIntField( const char * pszFieldPath, int nValue )

{
    return SetFieldValue( pszFieldPath, 'i', &nValue );
}

/************************************************************************/
/*                           SetDoubleField()                           */
/************************************************************************/

CPLErr HFAEntry::SetDoubleField( const char * pszFieldPath,
                                 double dfValue )

{
    return SetFieldValue( pszFieldPath, 'd', &dfValue );
}

/************************************************************************/
/*                            SetPosition()                             */
/*                                                                      */
/*      Set the disk position for this entry, and recursively apply     */
/*      to any children of this node.  The parent will take care of     */
/*      our siblings.                                                   */
/************************************************************************/

void HFAEntry::SetPosition()

{
/* -------------------------------------------------------------------- */
/*      Establish the location of this entry, and it's data.            */
/* -------------------------------------------------------------------- */
    if( nFilePos == 0 )
    {
        nFilePos = HFAAllocateSpace( psHFA, 
                                     psHFA->nEntryHeaderLength 
                                     + nDataSize );

        if( nDataSize > 0 )
            nDataPos = nFilePos + psHFA->nEntryHeaderLength;
    }

/* -------------------------------------------------------------------- */
/*      Force all children to set their position.                       */
/* -------------------------------------------------------------------- */
    for( HFAEntry *poThisChild = poChild; 
         poThisChild != NULL;
         poThisChild = poThisChild->poNext )
    {
        poThisChild->SetPosition();
    }
}

/************************************************************************/
/*                            FlushToDisk()                             */
/*                                                                      */
/*      Write this entry, and it's data to disk if the entries          */
/*      information is dirty.  Also force children to do the same.      */
/************************************************************************/

CPLErr HFAEntry::FlushToDisk()

{
    CPLErr	eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      If we are the root node, call SetPosition() on the whole        */
/*      tree to ensure that all entries have an allocated position.     */
/* -------------------------------------------------------------------- */
    if( poParent == NULL )
        SetPosition();

/* ==================================================================== */
/*      Only write this node out if it is dirty.                        */
/* ==================================================================== */
    if( bDirty )
    {
/* -------------------------------------------------------------------- */
/*      Ensure we know where the relative entries are located.          */
/* -------------------------------------------------------------------- */
        if( poNext != NULL )
            nNextPos = poNext->nFilePos;
        if( poChild != NULL )
            nChildPos = poChild->nFilePos;

/* -------------------------------------------------------------------- */
/*      Write the Ehfa_Entry fields.                                    */
/* -------------------------------------------------------------------- */
        GUInt32		nLong;

        VSIFFlushL( psHFA->fp );
        if( VSIFSeekL( psHFA->fp, nFilePos, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Failed to seek to %d for writing, out of disk space?",
                      nFilePos );
            return CE_Failure;
        }

        nLong = nNextPos;
        HFAStandard( 4, &nLong );
        VSIFWriteL( &nLong, 4, 1, psHFA->fp );

        if( poPrev != NULL )
            nLong = poPrev->nFilePos;
        else
            nLong = 0;
        HFAStandard( 4, &nLong );
        VSIFWriteL( &nLong, 4, 1, psHFA->fp );

        if( poParent != NULL )
            nLong = poParent->nFilePos;
        else
            nLong = 0;
        HFAStandard( 4, &nLong );
        VSIFWriteL( &nLong, 4, 1, psHFA->fp );

        nLong = nChildPos;
        HFAStandard( 4, &nLong );
        VSIFWriteL( &nLong, 4, 1, psHFA->fp );

        
        nLong = nDataPos;
        HFAStandard( 4, &nLong );
        VSIFWriteL( &nLong, 4, 1, psHFA->fp );

        nLong = nDataSize;
        HFAStandard( 4, &nLong );
        VSIFWriteL( &nLong, 4, 1, psHFA->fp );

        VSIFWriteL( szName, 1, 64, psHFA->fp );
        VSIFWriteL( szType, 1, 32, psHFA->fp );

        nLong = 0; /* Should we keep the time, or set it more reasonably? */
        if( VSIFWriteL( &nLong, 4, 1, psHFA->fp ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Failed to write HFAEntry %s(%s), out of disk space?",
                      szName, szType );
            return CE_Failure;
        }

/* -------------------------------------------------------------------- */
/*      Write out the data.                                             */
/* -------------------------------------------------------------------- */
        VSIFFlushL( psHFA->fp );
        if( nDataSize > 0 && pabyData != NULL )
        {
            if( VSIFSeekL( psHFA->fp, nDataPos, SEEK_SET ) != 0 
                || VSIFWriteL( pabyData, nDataSize, 1, psHFA->fp ) != 1 )
            {
                CPLError( CE_Failure, CPLE_FileIO, 
                          "Failed to write %d bytes HFAEntry %s(%s) data,\n"
                          "out of disk space?",
                          nDataSize, szName, szType );
                return CE_Failure;
            }
        }

        VSIFFlushL( psHFA->fp );
    }

/* -------------------------------------------------------------------- */
/*      Process all the children of this node                           */
/* -------------------------------------------------------------------- */
    for( HFAEntry *poThisChild = poChild; 
         poThisChild != NULL;
         poThisChild = poThisChild->poNext )
    {
        eErr = poThisChild->FlushToDisk();
        if( eErr != CE_None )
            return eErr;
    }

    bDirty = FALSE;

    return CE_None;
}

/************************************************************************/
/*                             MarkDirty()                              */
/*                                                                      */
/*      Mark this node as dirty (in need of writing to disk), and       */
/*      also mark the tree as a whole as being dirty.                   */
/************************************************************************/

void HFAEntry::MarkDirty()

{
    bDirty = TRUE;
    psHFA->bTreeDirty = TRUE;
}
