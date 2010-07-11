/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader::ResolveXlinks() method.
 * Author:   Chaitanya kumar CH, chaitanya@osgeo.in
 *
 ******************************************************************************
 * Copyright (c) 2010, Chaitanya kumar CH
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

#if HAVE_XERCES != 0 || defined(HAVE_EXPAT)

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

static int CompareNodeIDs( CPLXMLNode * psNode1, CPLXMLNode * psNode2 )

{
    if( psNode2 == NULL )
        return TRUE;

    if( psNode1 == NULL )
        return FALSE;

    return ( strcmp( GetID(psNode2), GetID(psNode1) ) > 0 );
}

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
/*      Replaces all empty URLs in URL#id pairs with pszURL in the      */
/*      node and it's children recursively                              */
/************************************************************************/

static void CorrectURLs( CPLXMLNode * psRoot, const char *pszURL )

{
    if( psRoot == NULL )
        return;

    CPLXMLNode *psChild = psRoot->psChild;

// check for xlink:href attribute
    while( psChild != NULL && !( ( psChild->eType == CXT_Attribute ) &&
                                 ( EQUAL(psChild->pszValue, "xlink:href") ) &&
                                 ( psChild->psChild->pszValue[0] == '#' ) ) )
        psChild = psChild->psNext;

    if( psChild != NULL )
    {
        size_t nLen = CPLStrnlen( pszURL, 512 ) +
                      CPLStrnlen( psChild->psChild->pszValue, 512 ) + 1;
        char *pszNew;
        pszNew = (char *)CPLMalloc( nLen * sizeof(char));
        CPLStrlcpy( pszNew, pszURL, nLen );
        CPLStrlcat( pszNew, psChild->psChild->pszValue, nLen );
        CPLSetXMLValue( psRoot, "#xlink:href", pszNew );
        CPLFree( pszNew );
    }

// search the child elements of psRoot
    for( psChild = psRoot->psChild; psChild != NULL; psChild = psChild->psNext)
        if( psChild->eType == CXT_Element )
            CorrectURLs( psChild, pszURL );
}

/************************************************************************/
/*                          FindTreeByURL()                             */
/*                                                                      */
/*      Find a doc tree that is located at pszURL.                      */
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
        CPLFree( pszLocation );


/************************************************************************/
/*      In the external GML resource we will only need elements         */
/*      identified by a "gml:id".                                       */
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

    return NULL;
}

/************************************************************************/
/*                           ResolveTree()                              */
/*  Resolves the xlinks in a node and it's siblings                     */
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
    CPLErr eReturn;
    
    for( psSibling = psNode; psSibling != NULL; psSibling = psSibling->psNext )
    {
        if( psSibling->eType != CXT_Element )
            continue;

        if( CSLFindString( papszSkip, psSibling->pszValue ) >= 0 )
            continue;

        CPLXMLNode *psChild = psSibling->psChild;
        while( psChild != NULL &&
               !( psChild->eType == CXT_Attribute &&
                  EQUAL( psChild->pszValue, "xlink:href" ) ) )
            psChild = psChild->psNext;

        //if a child has a "xlink:href" attribute
        if( psChild != NULL )
        {
            char **papszTokens;
            if( strstr( psChild->psChild->pszValue, "#" ) == NULL )
            {
                CPLError( bStrict ? CE_Failure : CE_Warning,
                          CPLE_NotSupported,
                          "Couldn't find '#' while parsing the href %s. "
                          "Can't possibly have an id.",
                          psChild->psChild->pszValue );
                if( bStrict ) break;
                else          continue;
            }
            papszTokens = CSLTokenizeString2( psChild->psChild->pszValue, "#",
                                              CSLT_ALLOWEMPTYTOKENS |
                                              CSLT_STRIPLEADSPACES |
                                              CSLT_STRIPENDSPACES );
            if( CSLCount( papszTokens ) != 2 || strlen(papszTokens[1]) <= 0 )
            {
                CPLError( bStrict ? CE_Failure : CE_Warning,
                          CPLE_NotSupported,
                          "Error parsing the href %s",
                          psChild->psChild->pszValue );
                CSLDestroy( papszTokens );
                if( bStrict ) break;
                else          continue;
            }

            //look for the resource with that URL
            psResource = FindTreeByURL( ppapsRoot,
                                        ppapszResourceHREF,
                                        papszTokens[0] );
            if( bStrict && psResource == NULL )
            {
                CSLDestroy( papszTokens );
                return CE_Failure;
            }

            //look for the element with the ID
            psTarget = FindElementByID( psResource, papszTokens[1] );
            static int i = 0;
            if( i-- == 0 )
            {
                i = 256;
                CPLDebug( "GML",
                          "Resolving xlinks... (currently %s)",
                          psChild->psChild->pszValue );
            }
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
                //nothing found
                CSLDestroy( papszTokens );
                if( bStrict )
                {
                    CPLError( CE_Failure,
                              CPLE_ObjectNull,
                              "Couldn't find the element with id %s.",
                              psChild->psChild->pszValue );
                    return CE_Failure;
                }
            }
        }

        //Recurse with the first child
        eReturn = Resolve( psSibling->psChild,
                           ppapsRoot,
                           ppapszResourceHREF,
                           papszSkip,
                           bStrict );
        if( bStrict && eReturn != CE_None )
            return eReturn;
    }
    return CE_None;
}

/************************************************************************/
/*                           ResolveXlinks()                            */
/*      Returns TRUE for success                                        */
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

//setup resource data structure
    char **papszResourceHREF = NULL;
    // "" is the href of the original source file
    papszResourceHREF = CSLAddString( papszResourceHREF, "" );

//call resolver
    Resolve( papsSrcTree[0], &papsSrcTree, &papszResourceHREF, papszSkip, bStrict );

    char *pszTmpName = NULL;
    int bTryWithTempFile = FALSE;
    int bReturn = TRUE;
    if( EQUALN(pszFile, "/vsitar/", strlen("/vsitar/")) ||
        EQUALN(pszFile, "/vsigzip/", strlen("/vsigzip/")) ||
        EQUALN(pszFile, "/vsizip/", strlen("/vsizip/")) )
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

    int nItems = CSLCount( papszResourceHREF );
    CSLDestroy( papszResourceHREF );
    while( nItems > 0 )
        CPLDestroyXMLNode( papsSrcTree[--nItems] );
    CPLFree( papsSrcTree );

    return bReturn;
}

#endif /* HAVE_XERCES == 1  || defined(HAVE_EXPAT) */
