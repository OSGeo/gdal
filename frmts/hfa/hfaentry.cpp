/******************************************************************************
 * $Id$
 *
 * Name:     hfaentry.cpp
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
    papbyFieldData = NULL;
    panFieldItemCount = NULL;

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
    GByte	*pabyCur;
    int		iField;
    
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

/* -------------------------------------------------------------------- */
/*      Allocate per-field information structures.                      */
/* -------------------------------------------------------------------- */
    panFieldItemCount = (int *) CPLCalloc(sizeof(int),poType->nFields);
    papbyFieldData = (GByte **) CPLCalloc(sizeof(GByte*),poType->nFields);
    
/* -------------------------------------------------------------------- */
/*      Process fields one at a time.                                   */
/* -------------------------------------------------------------------- */
    pabyCur = pabyData;
    for( iField = 0; iField < poType->nFields; iField++ )
    {
        HFAField	*poField = poType->papoFields[iField];
        
        /* for now we don't try to handle pointers */
        if( poField->chPointer )
            break;

        /* for now we don't try to handle sub objects */
        if( poField->pszItemObjectType != NULL || poField->chItemType == 'o' )
            break;

        panFieldItemCount[iField] = poField->nItemCount;
        papbyFieldData[iField] = pabyCur;

        pabyCur += poField->nItemCount *
            psHFA->poDictionary->GetItemSize( poField->chItemType );
    }

    CPLAssert( (int) (pabyCur - pabyData) <= (int) nDataSize );
}

/************************************************************************/
/*                              GetField()                              */
/*                                                                      */
/*      Get a field index by name.                                      */
/************************************************************************/

int HFAEntry::GetField( const char * pszFieldName )

{
    int		iField;
    
    LoadData();

    if( poType == NULL )
        return -1;
    
    for( iField = 0; iField < poType->nFields; iField++ )
    {
        if( EQUAL(pszFieldName,poType->papoFields[iField]->pszFieldName) )
            return( iField );
    }

    return -1;
}

/************************************************************************/
/*                            GetIntField()                             */
/************************************************************************/

int HFAEntry::GetIntField( const char * pszFieldName, int iArrayEntry )

{
    int		iField = GetField( pszFieldName );
    HFAField	*poField;

    if( iField == -1 || papbyFieldData[iField] == NULL )
        return 0;

    if( iArrayEntry < 0 || iArrayEntry > panFieldItemCount[iField] )
        return 0;


    poField = poType->papoFields[iField];

    switch( poField->chItemType )
    {
      /* notdef: doesn't handle U1, U2 or U4 */
        
      case 'c':
      case 'C':
        return( papbyFieldData[iField][iArrayEntry] );

      case 'e':
      case 's':
      {
          unsigned short nNumber;

          memcpy( &nNumber, papbyFieldData[iField] + iArrayEntry * 2, 2 );
          HFAStandard( 2, &nNumber );
          return( nNumber );
      }

      case 'S':
      {
          short nNumber;

          memcpy( &nNumber, papbyFieldData[iField] + iArrayEntry * 2, 2 );
          HFAStandard( 2, &nNumber );
          return( nNumber );
      }

      case 't':
      case 'l':
      {
          unsigned long	nULong;

          memcpy( &nULong, papbyFieldData[iField] + iArrayEntry * 4, 4 );
          HFAStandard( 4, &nULong );
          return( nULong );
      }
      
      case 'L':
      {
          long	nLong;

          memcpy( &nLong, papbyFieldData[iField] + iArrayEntry * 4, 4 );
          HFAStandard( 4, &nLong );
          return( nLong );
      }
      
      case 'f':
      {
          float		fNumber;

          memcpy( &fNumber, papbyFieldData[iField] + iArrayEntry * 4, 4 );
          HFAStandard( 4, &fNumber );
          return( (int) fNumber );
      }
      
      case 'd':
      {
          double	dfNumber;

          memcpy( &dfNumber, papbyFieldData[iField] + iArrayEntry * 8, 8 );
          HFAStandard( 8, &dfNumber );
          return( (int) dfNumber );
      }

      default:
        return 0;
    }
}

/************************************************************************/
/*                           GetDoubleField()                           */
/************************************************************************/

double HFAEntry::GetDoubleField( const char * pszFieldName, int iArrayEntry )

{
    int		iField = GetField( pszFieldName );
    HFAField	*poField;

    if( iField == -1 || papbyFieldData[iField] == NULL )
        return 0;

    if( iArrayEntry < 0 || iArrayEntry > panFieldItemCount[iField] )
        return 0;

    poField = poType->papoFields[iField];

    switch( poField->chItemType )
    {
      /* notdef: doesn't handle U1, U2 or U4 */
        
      case 'f':
      {
          float		fNumber;

          memcpy( &fNumber, papbyFieldData + iArrayEntry * 4, 4 );
          HFAStandard( 4, &fNumber );
          return( fNumber );
      }
      
      case 'd':
      {
          double	dfNumber;

          memcpy( &dfNumber, papbyFieldData + iArrayEntry * 8, 8 );
          HFAStandard( 8, &dfNumber );
          return( dfNumber );
      }

      default:
        return( (double) GetIntField( pszFieldName, iArrayEntry ) );
    }
}

/************************************************************************/
/*                          DumpFieldValues()                           */
/************************************************************************/

void HFAEntry::DumpFieldValues( FILE * fp, const char * pszPrefix )

{
    int		iField;
    
    if( pszPrefix == NULL )
        pszPrefix = "";

    LoadData();

    if( poType == NULL )
        return;

    for( iField = 0; iField < poType->nFields; iField++ )
    {
        HFAField	*poField = poType->papoFields[iField];
        int		iIndex;

        for( iIndex = 0; iIndex < panFieldItemCount[iField]; iIndex++ )
        {
            switch( poField->chItemType )
            {
              case 'f':
              case 'd':
                VSIFPrintf( fp, "%s%s[%d] = %f\n",
                            pszPrefix, poField->pszFieldName, iIndex,
                            GetDoubleField( poField->pszFieldName, iIndex ) );
                break;
                
              default:
                VSIFPrintf( fp, "%s%s[%d] = %d\n",
                            pszPrefix, poField->pszFieldName, iIndex,
                            GetIntField( poField->pszFieldName, iIndex ) );
                break;
            }
        }
    }
}
