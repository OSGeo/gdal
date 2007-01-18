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
 ****************************************************************************/

#include "dgn_pge.h"
#include "dgnlib.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static int DGNUpdateTagValue( DGNHandle, DGNElemTagValue *, const char * );

/************************************************************************/
/*                            DGNReadTags()                             */
/************************************************************************/

int DGNReadTags( const char *pszFilename, int nTagScheme,
                 char ***ppapszRetTagSets, 
                 char ***ppapszRetTagNames, 
                 char ***ppapszRetTagValues )

{
    DGNHandle hDGN;
    DGNElemCore **papsTagValues = NULL, **papsTagSets = NULL;
    int  nTagValueCount = 0, nTagSetCount = 0;

    *ppapszRetTagSets = NULL;
    *ppapszRetTagNames = NULL;
    *ppapszRetTagValues = NULL;

/* -------------------------------------------------------------------- */
/*      Open the file for read access.                                  */
/* -------------------------------------------------------------------- */
    hDGN = DGNOpen( pszFilename, FALSE );

    if( hDGN == NULL )
        return FALSE;

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
    int iTagSet;
    int iTag;
    char *pszResult;

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
            int i;

            for( i = 0; i < psSet->tagCount; i++ )
            {
                if( psSet->tagList[i].id == psTag->tagIndex )
                {
                    psTagDef = psSet->tagList + i;
                    break;
                }
            }
        }

        if( psTagDef == NULL )
        {
            CPLDebug( "PGE_DGN", 
                      "Skipping tag %d:%d since no matching tagset found.",
                      psTag->tagSet, psTag->tagIndex );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Put the tagsetname and tag index in the result, or if we        */
/*      didn't find the associated tagset, just use the numeric         */
/*      values.                                                         */
/* -------------------------------------------------------------------- */
        if( psTagDef != NULL )
        {
            *ppapszRetTagSets = 
                CSLAddString( *ppapszRetTagSets, psSet->tagSetName );
            *ppapszRetTagNames = 
                CSLAddString( *ppapszRetTagNames, psTagDef->name );
        }
        else
        {
            char        szId[32];

            sprintf( szId, "%d", psTag->tagSet );
            *ppapszRetTagSets = CSLAddString( *ppapszRetTagSets, szId );

            sprintf( szId, "%d", psTag->tagIndex );
            *ppapszRetTagNames = CSLAddString( *ppapszRetTagNames, szId );
        }

/* -------------------------------------------------------------------- */
/*      Add the value.                                                  */
/* -------------------------------------------------------------------- */
        if( psTag->tagType == 1 )
        {
            *ppapszRetTagValues = 
                CSLAddString( *ppapszRetTagValues, psTag->tagValue.string );
        }
        else if( psTag->tagType == 3 )
        {
            char szValue[128];
            
            sprintf( szValue, "%d", psTag->tagValue.integer );
            *ppapszRetTagValues = 
                CSLAddString( *ppapszRetTagValues, szValue );
        }
        else if( psTag->tagType == 4 )
        {
            char szValue[128];
            
            sprintf( szValue, "%g", psTag->tagValue.real );
            *ppapszRetTagValues = 
                CSLAddString( *ppapszRetTagValues, szValue );
        }
        else
        {
            *ppapszRetTagValues = 
                CSLAddString( *ppapszRetTagValues, 
                              "<unrecognised tag type %d>;" );
        }
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

    return TRUE;
}

/************************************************************************/
/*                            DGNWriteTags()                            */
/************************************************************************/

int DGNWriteTags( const char *pszFilename, int nTagScheme, 
                  char **papszTagSets, 
                  char **papszTagNames, 
                  char **papszTagValues )
    
{
    int  iTag, i, bResult = TRUE;

/* -------------------------------------------------------------------- */
/*      open the file in update mode, and index it.                     */
/* -------------------------------------------------------------------- */
    DGNHandle hDGN;
    int       nElementCount; 

    hDGN = DGNOpen( pszFilename, TRUE );
    if( hDGN == NULL )
        return FALSE;

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
/*      Load all tag value elements.                                    */
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
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "DGNGotoElement(%d) failed - skipping", iElem );
            bResult = FALSE;
            continue;
        }

        papsTagValues[nTagValueCount++] = 
            (DGNElemTagValue *) DGNReadElement( hDGN );
    }

/* -------------------------------------------------------------------- */
/*      Process each tag passed by the application.                     */
/* -------------------------------------------------------------------- */
    for( iTag = 0; iTag < CSLCount(papszTagSets); iTag++ )
    {
        int  iTagSet, nTagSetId=-1, nTagId=-1, bFoundTag = FALSE;
        DGNElemTagSet *psTagSet = NULL;

        // Find matching tagset.
        for( iTagSet = 0; iTagSet < nTagSetCount; iTagSet++ )
        {
            if( !EQUAL(papszTagSets[iTag],"*")
               && !EQUAL(papszTagSets[iTag],papsTagSets[iTagSet]->tagSetName) )
                continue;

            psTagSet = papsTagSets[iTagSet];

            // Find matching tag.
            for( i = 0; psTagSet != NULL && i < psTagSet->tagCount; i++ )
            {
                if( EQUAL(papszTagNames[iTag],psTagSet->tagList[i].name) )
                {
                    nTagSetId = psTagSet->tagSet;
                    nTagId = psTagSet->tagList[i].id;
                    break;
                }
            }

            if( nTagId == -1 )
            {
                if( !EQUAL(papszTagSets[iTag],"*") )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Failed to find tag %s:%s", 
                              papszTagSets[iTag], papszTagNames[iTag] );
                    bResult = FALSE;
                }
                continue;
            }

            bFoundTag = TRUE;

            // Now we find the associated tag value element.
            for( i = 0; i < nTagValueCount; i++ )
            {
                if( papsTagValues[i]->tagSet == nTagSetId
                    && papsTagValues[i]->tagIndex == nTagId )
                    break;
            }
            
            if( i == nTagValueCount )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Failed to find tag for %s:%s",
                          papszTagSets[iTag], papszTagNames[iTag] );
                bResult = FALSE;
                continue;
            }
            
            // Update tag value with new value.
            if( !DGNUpdateTagValue( hDGN, papsTagValues[i], 
                                    papszTagValues[iTag] ) )
            {
                bResult = FALSE;
            }
        }

        if( psTagSet == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to find tagset %s", 
                      papszTagSets[iTag] );
            bResult = FALSE;
        }

        if( !bFoundTag && EQUAL(papszTagSets[iTag],"*") )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Found no tags matching *:%s", 
                      papszTagNames[iTag] );
            bResult = FALSE;
        }
    }

    DGNClose( hDGN );

    return bResult;
}

/************************************************************************/
/*                         DGNUpdateTagValue()                          */
/************************************************************************/

int DGNUpdateTagValue( DGNHandle hDGN, DGNElemTagValue *psTag, 
                       const char *pszNewValue )

{
    if( psTag->core.raw_data == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "core.raw_data is NULL." );
        return FALSE;
    }

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
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Currently writing floating point tags not supported." );
        return FALSE;
    }

    return DGNWriteElement( hDGN, (DGNElemCore *) psTag );
}

