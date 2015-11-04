/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader::ResolveXlinks() method.
 * Author:   Chaitanya kumar CH, chaitanya@osgeo.in
 *
 ******************************************************************************
 * Copyright (c) 2010, Chaitanya kumar CH
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gmlreader.h"
#include "cpl_error.h"

CPL_CVSID("$Id$");

#include "gmlreaderp.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_http.h"

#include <stack>

/************************************************************************/
/*                              GetID()                                 */
/*                                                                      */
/*      Returns the reference to the gml:id of psNode. NULL if not      */
/*      found.                                                          */
/************************************************************************/

static const char* GetID( CPLXMLNode * psNode )

{
    if( psNode == NULL )
        return NULL;

    CPLXMLNode *psChild;
    for( psChild = psNode->psChild; psChild != NULL; psChild = psChild->psNext )
    {
        if( psChild->eType == CXT_Attribute
            && EQUAL(psChild->pszValue, "gml:id") )
        {
            return psChild->psChild->pszValue;
        }
    }
    return NULL;
}

/************************************************************************/
/*                       CompareNodeIDs()                               */
/*                                                                      */
/*      Compares two nodes by their IDs                                 */
/************************************************************************/

/*static int CompareNodeIDs( CPLXMLNode * psNode1, CPLXMLNode * psNode2 )

{
    if( psNode2 == NULL )
        return TRUE;

    if( psNode1 == NULL )
        return FALSE;

    return ( strcmp( GetID(psNode2), GetID(psNode1) ) > 0 );
}*/

/************************************************************************/
/*                       BuildIDIndex()                                 */
/*                                                                      */
/*      Returns an array of nodes sorted by their gml:id strings        */
/*      XXX: This method can be used to build an array of pointers to   */
/*      nodes sorted by their id values.                                */
/************************************************************************/
/*
static std::vector<CPLXMLNode*> BuildIDIndex( CPLXMLNode* psNode,
                                   std::vector<CPLXMLNode*> &apsNode )

{
    CPLXMLNode *psSibling;
    for( psSibling = psNode; psSibling != NULL; psSibling = psSibling->psNext )
    {
        if( GetID( psSibling ) != NULL )
            apsNode.push_back( psSibling );
        BuildIDIndex( psNode->psChild, apsNode );
    }
    return NULL;
}*/

/************************************************************************/
/*                          FindElementByID()                           */
/*                                                                      */
/*      Find a node with the indicated "gml:id" in the node tree and    */
/*      it's siblings.                                                  */
/************************************************************************/

static CPLXMLNode *FindElementByID( CPLXMLNode * psRoot,
                                    const char *pszID )

{
    if( psRoot == NULL )
        return NULL;

    CPLXMLNode *psSibling, *psReturn = NULL;

// check for id attribute
    for( psSibling = psRoot; psSibling != NULL; psSibling = psSibling->psNext)
    {
        if( psSibling->eType == CXT_Element )
        {
            // check that sibling for id value
            const char* pszIDOfSibling = GetID( psSibling );
            if( pszIDOfSibling != NULL && EQUAL( pszIDOfSibling, pszID) )
                return psSibling;
        }
    }

// search the child elements of all the psRoot's siblings
    for( psSibling = psRoot; psSibling != NULL; psSibling = psSibling->psNext)
    {
        if( psSibling->eType == CXT_Element )
        {
            psReturn = FindElementByID( psSibling->psChild, pszID );
            if( psReturn != NULL )
                return psReturn;
        }
    }
    return NULL;
}

/************************************************************************/
/*                          RemoveIDs()                                 */
/*                                                                      */
/*      Remove all the gml:id nodes. Doesn't check psRoot's siblings    */
/************************************************************************/

static void RemoveIDs( CPLXMLNode * psRoot )

{
    if( psRoot == NULL )
        return;

    CPLXMLNode *psChild = psRoot->psChild;

// check for id attribute
    while( psChild != NULL && !( psChild->eType == CXT_Attribute && EQUAL(psChild->pszValue, "gml:id")))
        psChild = psChild->psNext;
    CPLRemoveXMLChild( psRoot, psChild );
    CPLDestroyXMLNode( psChild );

// search the child elements of psRoot
    for( psChild = psRoot->psChild; psChild != NULL; psChild = psChild->psNext)
        if( psChild->eType == CXT_Element )
            RemoveIDs( psChild );
}

/************************************************************************/
/*                          TrimTree()                                  */
/*                                                                      */
/*      Remove all nodes without a gml:id node in the descendents.      */
/*      Returns TRUE if there is a gml:id node in the descendents.      */
/************************************************************************/

static int TrimTree( CPLXMLNode * psRoot )

{
    if( psRoot == NULL )
        return FALSE;

    CPLXMLNode *psChild = psRoot->psChild;

// check for id attribute
    while( psChild != NULL && !( psChild->eType == CXT_Attribute && EQUAL(psChild->pszValue, "gml:id")))
        psChild = psChild->psNext;

    if( psChild != NULL )
        return TRUE;

// search the child elements of psRoot
    int bReturn = FALSE, bRemove;
    for( psChild = psRoot->psChild; psChild != NULL;)
    {
        CPLXMLNode* psNextChild = psChild->psNext;
        if( psChild->eType == CXT_Element )
        {
            bRemove = TrimTree( psChild );
            if( bRemove )
            {
                bReturn = bRemove;
            }
            else
            {
                //remove this child
                CPLRemoveXMLChild( psRoot, psChild );
                CPLDestroyXMLNode( psChild );
            }
        }

        psChild = psNextChild;
    }
    return bReturn;
}

/************************************************************************/
/*                          CorrectURLs()                               */
/*                                                                      */
/*  Processes the node and all it's children recursively. Siblings of   */
/*  psRoot are ignored.                                                 */
/*  - Replaces all every URL in URL#id pairs with pszURL.               */
/*  - Leaves it alone if the paths are same or the URL is not relative. */
/*  - If it is relative, the path from pszURL is prepended.             */
/************************************************************************/

static void CorrectURLs( CPLXMLNode * psRoot, const char *pszURL )

{
    if( psRoot == NULL || pszURL == NULL )
        return;
    if( pszURL[0] == '\0' )
        return;

    CPLXMLNode *psChild = psRoot->psChild;

// check for xlink:href attribute
    while( psChild != NULL && !( ( psChild->eType == CXT_Attribute ) &&
                                 ( EQUAL(psChild->pszValue, "xlink:href") )) )
        psChild = psChild->psNext;

    if( psChild != NULL &&
        !( strstr( psChild->psChild->pszValue, pszURL ) == psChild->psChild->pszValue
        && psChild->psChild->pszValue[strlen(pszURL)] == '#' ) )
    {
    //href has a different url
        size_t nLen;
        char *pszNew;
        if( psChild->psChild->pszValue[0] == '#' )
        {
        //empty URL: prepend the given URL
            nLen = CPLStrnlen( pszURL, 1024 ) +
                   CPLStrnlen( psChild->psChild->pszValue, 1024 ) + 1;
            pszNew = (char *)CPLMalloc( nLen * sizeof(char));
            CPLStrlcpy( pszNew, pszURL, nLen );
            CPLStrlcat( pszNew, psChild->psChild->pszValue, nLen );
            CPLSetXMLValue( psRoot, "#xlink:href", pszNew );
            CPLFree( pszNew );
        }
        else
        {
            size_t nPathLen;
            for( nPathLen = strlen(pszURL);
                 nPathLen > 0 && pszURL[nPathLen - 1] != '/'
                              && pszURL[nPathLen - 1] != '\\';
                 nPathLen--);

            const char* pszDash = strchr( psChild->psChild->pszValue, '#' );
            if( pszDash != NULL &&
                strncmp( pszURL, psChild->psChild->pszValue, nPathLen ) != 0 )
            {
            //different path
                int nURLLen = pszDash - psChild->psChild->pszValue;
                char *pszURLWithoutID = (char *)CPLMalloc( (nURLLen+1) * sizeof(char));
                strncpy( pszURLWithoutID, psChild->psChild->pszValue, nURLLen );
                pszURLWithoutID[nURLLen] = '\0';

                if( CPLIsFilenameRelative( pszURLWithoutID ) &&
                    strstr( pszURLWithoutID, ":" ) == NULL )
                {
                    //relative URL: prepend the path of pszURL
                    nLen = nPathLen +
                           CPLStrnlen( psChild->psChild->pszValue, 1024 ) + 1;
                    pszNew = (char *)CPLMalloc( nLen * sizeof(char));
                    size_t i;
                    for( i = 0; i < nPathLen; i++ )
                        pszNew[i] = pszURL[i];
                    pszNew[nPathLen] = '\0';
                    CPLStrlcat( pszNew, psChild->psChild->pszValue, nLen );
                    CPLSetXMLValue( psRoot, "#xlink:href", pszNew );
                    CPLFree( pszNew );
                }
                CPLFree( pszURLWithoutID );
            }
        }
    }

// search the child elements of psRoot
    for( psChild = psRoot->psChild; psChild != NULL; psChild = psChild->psNext)
        if( psChild->eType == CXT_Element )
            CorrectURLs( psChild, pszURL );
}

/************************************************************************/
/*                          FindTreeByURL()                             */
/*                                                                      */
/*  Find a doc tree that is located at pszURL.                          */
/*  If not present in ppapsRoot, it updates it and ppapszResourceHREF.  */
/************************************************************************/

static CPLXMLNode *FindTreeByURL( CPLXMLNode *** ppapsRoot,
                                  char *** ppapszResourceHREF,
                                  const char *pszURL )

{
    if( *ppapsRoot == NULL || ppapszResourceHREF == NULL )
        return NULL;

//if found in ppapszResourceHREF
    int i, nItems;
    char *pszLocation;
    if( ( i = CSLFindString( *ppapszResourceHREF, pszURL )) >= 0 )
    {
    //return corresponding psRoot
        return (*ppapsRoot)[i];
    }
    else
    {
        CPLXMLNode *psSrcTree = NULL, *psSibling;
        pszLocation = CPLStrdup( pszURL );
        //if it is part of filesystem
        if( CPLCheckForFile( pszLocation, NULL) )
        {//filesystem
            psSrcTree = CPLParseXMLFile( pszURL );
        }
        else if( CPLHTTPEnabled() )
        {//web resource
            CPLErrorReset();
            CPLHTTPResult *psResult = CPLHTTPFetch( pszURL, NULL );
            if( psResult != NULL )
            {
                if( psResult->nDataLen > 0 && CPLGetLastErrorNo() == 0)
                    psSrcTree = CPLParseXMLString( (const char*)psResult->pabyData );
                CPLHTTPDestroyResult( psResult );
            }
        }

        //report error in case the resource cannot be retrieved.
        if( psSrcTree == NULL )
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Could not access %s", pszLocation );

        CPLFree( pszLocation );

/************************************************************************/
/*      In the external GML resource we will only need elements         */
/*      identified by a "gml:id". So trim them.                         */
/************************************************************************/
        psSibling = psSrcTree;
        while( psSibling != NULL )
        {
            TrimTree( psSibling );
            psSibling = psSibling->psNext;
        }

    //update to lists
        nItems = CSLCount(*ppapszResourceHREF);
        *ppapszResourceHREF = CSLAddString( *ppapszResourceHREF, pszURL );
        *ppapsRoot = (CPLXMLNode**)CPLRealloc(*ppapsRoot,
                                            (nItems+2)*sizeof(CPLXMLNode*));
        (*ppapsRoot)[nItems] = psSrcTree;
        (*ppapsRoot)[nItems+1] = NULL;

    //return the tree
        return (*ppapsRoot)[nItems];
    }
}

/************************************************************************/
/*                           ResolveTree()                              */
/*  Resolves the xlinks in a node and it's siblings                     */
/*  If any error is encountered or any element is skipped(papszSkip):   */
/*      If bStrict is TRUE, process is stopped and CE_Error is returned */
/*      If bStrict is FALSE, the process is continued but CE_Warning is */
/*       returned at the end.                                           */
/*  If everything goes fine, CE_None is returned.                       */
/************************************************************************/

static CPLErr Resolve( CPLXMLNode * psNode,
                CPLXMLNode *** ppapsRoot,
                char *** ppapszResourceHREF,
                char ** papszSkip,
                const int bStrict )

{
    //for each sibling
    CPLXMLNode *psSibling = NULL;
    CPLXMLNode *psResource = NULL;
    CPLXMLNode *psTarget = NULL;
    CPLErr eReturn = CE_None, eReturned;
    
    for( psSibling = psNode; psSibling != NULL; psSibling = psSibling->psNext )
    {
        if( psSibling->eType != CXT_Element )
            continue;

        CPLXMLNode *psChild = psSibling->psChild;
        while( psChild != NULL &&
               !( psChild->eType == CXT_Attribute &&
                  EQUAL( psChild->pszValue, "xlink:href" ) ) )
            psChild = psChild->psNext;

        //if a child has a "xlink:href" attribute
        if( psChild != NULL && psChild->psChild != NULL )
        {
            if( CSLFindString( papszSkip, psSibling->pszValue ) >= 0 )
            {//Skipping a specified element
                eReturn = CE_Warning;
                continue;
            }

            static int i = 0;
            if( i-- == 0 )
            {//a way to track progress
                i = 256;
                CPLDebug( "GML",
                          "Resolving xlinks... (currently %s)",
                          psChild->psChild->pszValue );
            }

            char **papszTokens;
            papszTokens = CSLTokenizeString2( psChild->psChild->pszValue, "#",
                                              CSLT_ALLOWEMPTYTOKENS |
                                              CSLT_STRIPLEADSPACES |
                                              CSLT_STRIPENDSPACES );
            if( CSLCount( papszTokens ) != 2 || strlen(papszTokens[1]) <= 0 )
            {
                CPLError( bStrict ? CE_Failure : CE_Warning,
                          CPLE_NotSupported,
                          "Error parsing the href %s.%s",
                          psChild->psChild->pszValue,
                          bStrict ? "" : " Skipping..." );
                CSLDestroy( papszTokens );
                if( bStrict )
                    return CE_Failure;
                eReturn = CE_Warning;
                continue;
            }

            //look for the resource with that URL
            psResource = FindTreeByURL( ppapsRoot,
                                        ppapszResourceHREF,
                                        papszTokens[0] );
            if( psResource == NULL )
            {
                CSLDestroy( papszTokens );
                if( bStrict )
                    return CE_Failure;
                eReturn = CE_Warning;
                continue;
            }

            //look for the element with the ID
            psTarget = FindElementByID( psResource, papszTokens[1] );
            if( psTarget != NULL )
            {
                //remove the xlink:href attribute
                CPLRemoveXMLChild( psSibling, psChild );
                CPLDestroyXMLNode( psChild );

                //make a copy of psTarget
                CPLXMLNode *psCopy = CPLCreateXMLNode( NULL,
                                                       CXT_Element,
                                                       psTarget->pszValue );
                psCopy->psChild = CPLCloneXMLTree( psTarget->psChild );
                RemoveIDs( psCopy );
                //correct empty URLs in URL#id pairs
                if( CPLStrnlen( papszTokens[0], 1 ) > 0 )
                {
                    CorrectURLs( psCopy, papszTokens[0] );
                }
                CPLAddXMLChild( psSibling, psCopy );
                CSLDestroy( papszTokens );
            }
            else
            {
                //element not found
                CSLDestroy( papszTokens );
                CPLError( bStrict ? CE_Failure : CE_Warning,
                          CPLE_ObjectNull,
                          "Couldn't find the element with id %s.",
                          psChild->psChild->pszValue );
                if( bStrict )
                    return CE_Failure;
                eReturn = CE_Warning;
            }
        }

        //Recurse with the first child
        eReturned=Resolve( psSibling->psChild,
                           ppapsRoot,
                           ppapszResourceHREF,
                           papszSkip,
                           bStrict );

        if( eReturned == CE_Failure )
            return CE_Failure;

        if( eReturned == CE_Warning )
                eReturn = CE_Warning;
    }
    return eReturn;
}

/************************************************************************/
/*                           ResolveXlinks()                            */
/*      Returns TRUE for success                                        */
/*    - Returns CE_None for success,                                    */
/*      CE_Warning if the resolved file is saved to a different file or */
/*      CE_Failure if it could not be saved at all.                     */
/*    - m_pszFilename will be set to the file the resolved file was     */
/*      saved to.                                                       */
/************************************************************************/

int GMLReader::ResolveXlinks( const char *pszFile,
                              int* pbOutIsTempFile,
                              char **papszSkip,
                              const int bStrict)

{
    *pbOutIsTempFile = FALSE;

// Check if the original source file is set.
    if( m_pszFilename == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GML source file needs to be set first with "
                  "GMLReader::SetSourceFile()." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Load the raw XML file into a XML Node tree.                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode **papsSrcTree;
    papsSrcTree = (CPLXMLNode **)CPLCalloc( 2, sizeof(CPLXMLNode *));
    papsSrcTree[0] = CPLParseXMLFile( m_pszFilename );

    if( papsSrcTree[0] == NULL )
    {
        CPLFree(papsSrcTree);
        return FALSE;
    }

    //make all the URLs absolute
    CPLXMLNode *psSibling = NULL;
    for( psSibling = papsSrcTree[0]; psSibling != NULL; psSibling = psSibling->psNext )
        CorrectURLs( psSibling, m_pszFilename );

    //setup resource data structure
    char **papszResourceHREF = NULL;
    // "" is the href of the original source file
    papszResourceHREF = CSLAddString( papszResourceHREF, m_pszFilename );

    //call resolver
    CPLErr eReturned = CE_None;
    eReturned = Resolve( papsSrcTree[0], &papsSrcTree, &papszResourceHREF, papszSkip, bStrict );

    int bReturn = TRUE;
    if( eReturned != CE_Failure )
    {
        char *pszTmpName = NULL;
        int bTryWithTempFile = FALSE;
        if( STARTS_WITH_CI(pszFile, "/vsitar/") ||
            STARTS_WITH_CI(pszFile, "/vsigzip/") ||
            STARTS_WITH_CI(pszFile, "/vsizip/") )
        {
            bTryWithTempFile = TRUE;
        }
        else if( !CPLSerializeXMLTreeToFile( papsSrcTree[0], pszFile ) )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot serialize resolved file %s to %s.",
                      m_pszFilename, pszFile );
            bTryWithTempFile = TRUE;
        }

        if (bTryWithTempFile)
        {
            pszTmpName = CPLStrdup( CPLGenerateTempFilename( "ResolvedGML" ) );
            if( !CPLSerializeXMLTreeToFile( papsSrcTree[0], pszTmpName ) )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Cannot serialize resolved file %s to %s either.",
                          m_pszFilename, pszTmpName );
                CPLFree( pszTmpName );
                bReturn = FALSE;
            }
            else
            {
            //set the source file to the resolved file
                CPLFree( m_pszFilename );
                m_pszFilename = pszTmpName;
                *pbOutIsTempFile = TRUE;
            }
        }
        else
        {
        //set the source file to the resolved file
            CPLFree( m_pszFilename );
            m_pszFilename = CPLStrdup( pszFile );
        }
    }
    else
    {
        bReturn = FALSE;
    }

    int nItems = CSLCount( papszResourceHREF );
    CSLDestroy( papszResourceHREF );
    while( nItems > 0 )
        CPLDestroyXMLNode( papsSrcTree[--nItems] );
    CPLFree( papsSrcTree );

    return bReturn;
}
