/******************************************************************************
 * $Id$
 *
 * Project:  DGN Tag Read/Write Bindings for Pacific Gas and Electric
 * Purpose:  DGNReadTags / DGNWriteTags implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Pacific Gas and Electric Co, San Franciso, CA, USA.
 *
 * All rights reserved.  Not to be used, reproduced or disclosed without
 * permission.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2002/03/15 15:07:06  warmerda
 * use dgn_pge.h
 *
 * Revision 1.1  2002/03/14 21:40:37  warmerda
 * New
 *
 */

#include "dgn_pge.h"
#include "dgnlib.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static void CPLEscapeString( const char *pszSource, char *pszDest );
static int DGNUpdateTagValue( DGNHandle, DGNElemTagValue *, const char * );

#define MAX_ITEM_SIZE (768*6)

/************************************************************************/
/*                            DGNReadTags()                             */
/************************************************************************/

char *pgeDGNReadTags( const char *pszFilename, int nTagScheme )

{
    DGNHandle hDGN;
    DGNElemCore **papsTagValues = NULL, **papsTagSets = NULL;
    int  nTagValueCount = 0, nTagSetCount = 0;

/* -------------------------------------------------------------------- */
/*      Open the file for read access.                                  */
/* -------------------------------------------------------------------- */
    hDGN = DGNOpen( pszFilename, FALSE );

    if( hDGN == NULL )
    {
        if( strlen(CPLGetLastErrorMsg()) > 0 )
            return CPLStrdup(CPLGetLastErrorMsg());
        else
            return CPLStrdup("Failure opening DGN file.");
    }

/* -------------------------------------------------------------------- */
/*      Scan the file, keeping all tag set and tag value elements.      */
/* -------------------------------------------------------------------- */
    DGNElemCore *psElement; 

    while( (psElement = DGNReadElement( hDGN )) != NULL )
    {
        if( psElement->stype == DGNST_TAG_VALUE 
            && !psElement->deleted )
        {
            papsTagValues = (DGNElemCore **) 
                CPLRealloc(papsTagValues,sizeof(void*) * (++nTagValueCount));
            papsTagValues[nTagValueCount-1] = psElement;
        }
        else if( psElement->stype == DGNST_TAG_SET 
                 && !psElement->deleted )
        {
            papsTagSets = (DGNElemCore **) 
                CPLRealloc(papsTagSets,sizeof(void*) * (++nTagSetCount));
            papsTagSets[nTagSetCount-1] = psElement;
        }
        else
            DGNFreeElement( hDGN, psElement );
    }
    
/* -------------------------------------------------------------------- */
/*      For each Tag find the corresponding tagset definition.          */
/* -------------------------------------------------------------------- */
    DGNElemTagValue *psTag;
    DGNElemTagSet   *psSet;
    int	iTagSet;
    int	iTag;
    char *pszResult;
    char szItem[MAX_ITEM_SIZE];

    pszResult = CPLStrdup( "TAG_LIST;" );

    for( iTag = 0; iTag < nTagValueCount; iTag++ )
    {
        DGNTagDef       *psTagDef = NULL;

        psTag = (DGNElemTagValue *) papsTagValues[iTag];
        psSet = NULL;

/* -------------------------------------------------------------------- */
/*      Find the tagset and tag definition within it associated with    */
/*      this tag.                                                       */
/* -------------------------------------------------------------------- */
        for( iTagSet = 0; iTagSet < nTagSetCount; iTagSet++ )
        {
            psSet = (DGNElemTagSet *) papsTagSets[iTagSet];

            if( psSet->tagSet == psTag->tagSet )
                break;
        }

        if( psSet != NULL && psSet->tagSet == psTag->tagSet )
        {
            int	i;

            for( i = 0; i < psSet->tagCount; i++ )
            {
                if( psSet->tagList[i].id == psTag->tagIndex )
                {
                    psTagDef = psSet->tagList + i;
                    break;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Put the tagsetname and tag index in the result, or if we        */
/*      didn't find the associated tagset, just use the numeric         */
/*      values.                                                         */
/* -------------------------------------------------------------------- */
        if( psTagDef != NULL )
        {
            strcpy( szItem, "\"" );
            CPLEscapeString( psSet->tagSetName, szItem + strlen(szItem) );
            strcat( szItem, "\":\"" );
            CPLEscapeString( psTagDef->name, szItem + strlen(szItem) );
            strcat( szItem, "\":" );
        }
        else
        {
            sprintf( szItem, "%d:%d:", psTag->tagSet, psTag->tagIndex );
        }

/* -------------------------------------------------------------------- */
/*      Add the value.                                                  */
/* -------------------------------------------------------------------- */
        if( psTag->tagType == 1 )
        {
            strcat( szItem, "\"" );
            CPLEscapeString( psTag->tagValue.string, szItem+strlen(szItem) );
            strcat( szItem, "\";" );
        }
        else if( psTag->tagType == 3 )
        {
            sprintf( szItem+strlen(szItem), "%d;", psTag->tagValue.integer );
        }
        else if( psTag->tagType == 4 )
        {
            sprintf( szItem+strlen(szItem), "%g;", psTag->tagValue.real );
        }
        else
        {
            sprintf( szItem+strlen(szItem), "<unrecognised tag type %d>;",
                     psTag->tagType );
        }

/* -------------------------------------------------------------------- */
/*      Add this item to the overall result string.                     */
/* -------------------------------------------------------------------- */
        pszResult = (char *) 
            CPLRealloc( pszResult, strlen(pszResult) + strlen(szItem) + 1);
        strcat( pszResult, szItem );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup tag elements.                                           */
/* -------------------------------------------------------------------- */
    for( iTagSet = 0; iTagSet < nTagSetCount; iTagSet++ )
        DGNFreeElement( hDGN, papsTagSets[iTagSet] );

    CPLFree( papsTagSets );
    
    for( iTag = 0; iTag < nTagValueCount; iTag++ )
        DGNFreeElement( hDGN, papsTagValues[iTag] );

    CPLFree( papsTagValues );
    
    DGNClose( hDGN );

    return pszResult;
}

/************************************************************************/
/*                            DGNWriteTags()                            */
/************************************************************************/

char *pgeDGNWriteTags( const char *pszFilename, int nTagScheme, 
                       const char *pszTagList )
    
{
    int  iTag, i;
    char **papszTagList;

/* -------------------------------------------------------------------- */
/*      Split the tag list into individual tagset:tagname:value         */
/*      items.                                                          */
/* -------------------------------------------------------------------- */
    papszTagList = CSLTokenizeString2( pszTagList, ";", 
                                       CSLT_HONOURSTRINGS
                                       | CSLT_PRESERVEQUOTES
                                       | CSLT_PRESERVEESCAPES );

    if( CSLCount(papszTagList) < 1 
        || !EQUAL(papszTagList[0],"TAGLIST") )
    {
        return CPLStrdup( "Corrupt TAGLIST" );
    }

/* -------------------------------------------------------------------- */
/*      open the file in update mode, and index it.                     */
/* -------------------------------------------------------------------- */
    DGNHandle hDGN;
    int       nElementCount; 

    hDGN = DGNOpen( pszFilename, TRUE );
    if( hDGN == NULL )
        return CPLStrdup( "ERROR: Failed to open file with update access." );

    const DGNElementInfo *pasIndex = DGNGetElementIndex(hDGN,&nElementCount);

    DGNSetOptions( hDGN, DGNO_CAPTURE_RAW_DATA );

/* -------------------------------------------------------------------- */
/*      Load a list of all the tag sets, so we can translate tag set    */
/*      names and tag names into numeric values.                        */
/* -------------------------------------------------------------------- */
    int iElem;
    DGNElemTagSet **papsTagSets = NULL;
    int nTagSetCount = 0;

    for( iElem = 0; iElem < nElementCount; iElem++ )
    {
        if( pasIndex[iElem].stype != DGNST_TAG_SET
            || (pasIndex[iElem].flags & DGNEIF_DELETED) )
            continue;

        papsTagSets = (DGNElemTagSet **)
            CPLRealloc(papsTagSets,sizeof(void*) * (nTagSetCount+1));

        if( !DGNGotoElement( hDGN, iElem ) )
            continue;

        papsTagSets[nTagSetCount++] = (DGNElemTagSet *) DGNReadElement( hDGN );
    }

/* -------------------------------------------------------------------- */
/*      Load all tag value elements.					*/
/* -------------------------------------------------------------------- */
    DGNElemTagValue **papsTagValues = NULL;
    int nTagValueCount = 0;

    for( iElem = 0; iElem < nElementCount; iElem++ )
    {
        if( pasIndex[iElem].stype != DGNST_TAG_VALUE 
            || (pasIndex[iElem].flags & DGNEIF_DELETED) )
            continue;

        papsTagValues = (DGNElemTagValue **)
            CPLRealloc(papsTagValues,sizeof(void*) * (nTagValueCount+1));

        if( !DGNGotoElement( hDGN, iElem ) )
        {
            continue;
        }

        papsTagValues[nTagValueCount++] = 
            (DGNElemTagValue *) DGNReadElement( hDGN );
    }

/* -------------------------------------------------------------------- */
/*	Process each tag passed by the application.			*/
/* -------------------------------------------------------------------- */
    for( iTag = 0; iTag < CSLCount(papszTagList); iTag++ )
    {
        char **papszTokens;
        int  iTagSet, nTagSetId=-1, nTagId=-1;
        DGNElemTagSet *psTagSet = NULL;

        papszTokens = CSLTokenizeString2( papszTagList[iTag], ":",
                                          CSLT_HONOURSTRINGS
                                          | CSLT_ALLOWEMPTYTOKENS );

        if( CSLCount(papszTokens) < 3 )
            continue;

        // Find matching tagset.
        for( iTagSet = 0; iTagSet < nTagSetCount; iTagSet++ )
        {
            if( EQUAL(papszTokens[0],papsTagSets[iTagSet]->tagSetName) )
                psTagSet = papsTagSets[iTagSet];
        }

        // Find matching tag.
        for( i = 0; psTagSet != NULL && i < psTagSet->tagCount; i++ )
        {
            if( EQUAL(papszTokens[1],psTagSet->tagList[i].name) )
            {
                nTagSetId = psTagSet->tagSet;
                nTagId = psTagSet->tagList[i].id;
                break;
            }
        }

        if( nTagId == -1 )
        {
            //notdef: we must record an error for this tag. 
            CSLDestroy( papszTokens );
            continue;
        }

        // Now we find the associated tag value element.
        for( i = 0; i < nTagValueCount; i++ )
        {
            if( papsTagValues[i]->tagSet == nTagSetId
                && papsTagValues[i]->tagIndex == nTagId )
                break;
        }

        if( i == nTagValueCount )
        {
            //notdef: we must record an error for this tag. 
            CSLDestroy( papszTokens );
            continue;
        }

        // Update tag value with new value.
        if( !DGNUpdateTagValue( hDGN, papsTagValues[i], papszTokens[2] ) )
        {
            // notdef - should indicate error.
        }
    }

    CSLDestroy( papszTagList );

    DGNClose( hDGN );

    return CPLStrdup("SUCCESS");
}

/************************************************************************/
/*                           DGNFreeResult()                            */
/************************************************************************/

void pgeDGNFreeResult( char *pszResult )

{
    CPLFree( pszResult );
}


/************************************************************************/
/*                         DGNUpdateTagValue()                          */
/************************************************************************/

int DGNUpdateTagValue( DGNHandle hDGN, DGNElemTagValue *psTag, 
                       const char *pszNewValue )

{
    if( psTag->core.raw_data == NULL )
        return FALSE;

    if( psTag->tagType == 1 )
    {
        int nNewSize, nDataBytes;

        nNewSize = 154 + strlen(pszNewValue) + 1;
        nNewSize += nNewSize % 2; // round up to multiple of two

        if( !DGNResizeElement( hDGN, (DGNElemCore *) psTag, nNewSize ) )
            return FALSE;

        strcpy( (char *) psTag->core.raw_data + 154, pszNewValue );

        nDataBytes = strlen(pszNewValue)+1;
        psTag->core.raw_data[150] = nDataBytes % 256;
        psTag->core.raw_data[151] = nDataBytes / 256;
        psTag->core.raw_data[152] = 0;
        psTag->core.raw_data[153] = 0;
    }
    else if( psTag->tagType == 3 )
    {
        GInt32  nTagValue = CPL_LSBWORD32(atoi(pszNewValue));

        memcpy( psTag->core.raw_data + 154, &nTagValue, 4 );
    }
    else if( psTag->tagType == 4 )
    {
        CPLAssert( FALSE );

        // How do we transform IEEE double to vax floating point?
        return FALSE;
    }

    return DGNWriteElement( hDGN, (DGNElemCore *) psTag );
}

/************************************************************************/
/*                          CPLEscapeString()                           */
/************************************************************************/

static void CPLEscapeString( const char *pszSource, char *pszDest )

{
    while( *pszSource != '\0' )
    {
        if( *pszSource == '\\' || *pszSource == '\"' )
        {
            *(pszDest++) = '\\';
            *(pszDest++) = *pszSource;
        }
        else
            *(pszDest++) = *pszSource;

        pszSource++;
    }

    *pszDest = '\0';
}
