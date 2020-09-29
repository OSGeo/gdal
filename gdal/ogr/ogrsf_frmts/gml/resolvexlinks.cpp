/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader::ResolveXlinks() method.
 * Author:   Chaitanya kumar CH, chaitanya@osgeo.in
 *
 ******************************************************************************
 * Copyright (c) 2010, Chaitanya kumar CH
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gmlreader.h"
#include "gmlreaderp.h"

#include <cstddef>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                              GetID()                                 */
/*                                                                      */
/*      Returns the reference to the gml:id of psNode. NULL if not      */
/*      found.                                                          */
/************************************************************************/

static const char *GetID( CPLXMLNode *psNode )

{
    if( psNode == nullptr )
        return nullptr;

    for( CPLXMLNode *psChild = psNode->psChild;
         psChild != nullptr;
         psChild = psChild->psNext )
    {
        if( psChild->eType == CXT_Attribute &&
            EQUAL(psChild->pszValue, "gml:id") )
        {
            return psChild->psChild->pszValue;
        }
    }
    return nullptr;
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

    return strcmp( GetID(psNode2), GetID(psNode1) ) > 0;
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
    for( CPLXMLNode *psSibling = psNode;
         psSibling != NULL;
         psSibling = psSibling->psNext )
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
/*      its siblings.                                                   */
/************************************************************************/

static CPLXMLNode *FindElementByID( CPLXMLNode *psRoot, const char *pszID )

{
    if( psRoot == nullptr )
        return nullptr;

    // Check for id attribute.
    for( CPLXMLNode *psSibling = psRoot;
         psSibling != nullptr;
         psSibling = psSibling->psNext )
    {
        if( psSibling->eType == CXT_Element )
        {
            // check that sibling for id value
            const char *pszIDOfSibling = GetID(psSibling);
            if( pszIDOfSibling != nullptr && EQUAL( pszIDOfSibling, pszID) )
                return psSibling;
        }
    }

    // Search the child elements of all the psRoot's siblings.
    for( CPLXMLNode *psSibling = psRoot;
         psSibling != nullptr;
         psSibling = psSibling->psNext )
    {
        if( psSibling->eType == CXT_Element )
        {
            CPLXMLNode *psReturn = FindElementByID(psSibling->psChild, pszID);
            if( psReturn != nullptr )
                return psReturn;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                          RemoveIDs()                                 */
/*                                                                      */
/*      Remove all the gml:id nodes. Doesn't check psRoot's siblings    */
/************************************************************************/

static void RemoveIDs( CPLXMLNode *psRoot )

{
    if( psRoot == nullptr )
        return;

    CPLXMLNode *psChild = psRoot->psChild;

    // Check for id attribute.
    while( psChild != nullptr &&
           !(psChild->eType == CXT_Attribute &&
             EQUAL(psChild->pszValue, "gml:id")) )
        psChild = psChild->psNext;
    CPLRemoveXMLChild(psRoot, psChild);
    CPLDestroyXMLNode(psChild);

    // Search the child elements of psRoot.
    for( psChild = psRoot->psChild; psChild != nullptr; psChild = psChild->psNext )
        if( psChild->eType == CXT_Element )
            RemoveIDs(psChild);
}

/************************************************************************/
/*                          TrimTree()                                  */
/*                                                                      */
/*      Remove all nodes without a gml:id node in the descendants.      */
/*      Returns TRUE if there is a gml:id node in the descendants.      */
/************************************************************************/

static bool TrimTree( CPLXMLNode *psRoot )

{
    if( psRoot == nullptr )
        return false;

    CPLXMLNode *psChild = psRoot->psChild;

    // Check for id attribute.
    while( psChild != nullptr &&
           !(psChild->eType == CXT_Attribute &&
             EQUAL(psChild->pszValue, "gml:id")) )
        psChild = psChild->psNext;

    if( psChild != nullptr )
        return true;

    // Search the child elements of psRoot.
    bool bReturn = false;
    for( psChild = psRoot->psChild; psChild != nullptr;)
    {
        CPLXMLNode *psNextChild = psChild->psNext;
        if( psChild->eType == CXT_Element )
        {
            const bool bRemove = TrimTree(psChild);
            if( bRemove )
            {
                bReturn = bRemove;
            }
            else
            {
                // Remove this child.
                CPLRemoveXMLChild(psRoot, psChild);
                CPLDestroyXMLNode(psChild);
            }
        }

        psChild = psNextChild;
    }
    return bReturn;
}

/************************************************************************/
/*                          CorrectURLs()                               */
/*                                                                      */
/*  Processes the node and all its children recursively. Siblings of   */
/*  psRoot are ignored.                                                 */
/*  - Replaces all every URL in URL#id pairs with pszURL.               */
/*  - Leaves it alone if the paths are same or the URL is not relative. */
/*  - If it is relative, the path from pszURL is prepended.             */
/************************************************************************/

static void CorrectURLs( CPLXMLNode *psRoot, const char *pszURL )

{
    if( psRoot == nullptr || pszURL == nullptr )
        return;
    if( pszURL[0] == '\0' )
        return;

    CPLXMLNode *psChild = psRoot->psChild;

    // Check for xlink:href attribute.
    while( psChild != nullptr &&
           !((psChild->eType == CXT_Attribute) &&
             (EQUAL(psChild->pszValue, "xlink:href"))) )
        psChild = psChild->psNext;

    if( psChild != nullptr &&
        !(strstr(psChild->psChild->pszValue, pszURL) ==
              psChild->psChild->pszValue &&
          psChild->psChild->pszValue[strlen(pszURL)] == '#') )
    {
        // href has a different url.
        if( psChild->psChild->pszValue[0] == '#' )
        {
            // Empty URL: prepend the given URL.
            const size_t nLen = CPLStrnlen(pszURL, 1024) +
                                CPLStrnlen(psChild->psChild->pszValue, 1024) +
                                1;
            char *pszNew = static_cast<char *>(CPLMalloc(nLen * sizeof(char)));
            CPLStrlcpy(pszNew, pszURL, nLen);
            CPLStrlcat(pszNew, psChild->psChild->pszValue, nLen);
            CPLSetXMLValue(psRoot, "#xlink:href", pszNew);
            CPLFree(pszNew);
        }
        else
        {
            size_t nPathLen = strlen(pszURL);  // Used after for.
            for( ;
                 nPathLen > 0 &&
                 pszURL[nPathLen - 1] != '/' &&
                 pszURL[nPathLen - 1] != '\\';
                 nPathLen-- )
                {}

            const char *pszDash = strchr(psChild->psChild->pszValue, '#');
            if( pszDash != nullptr &&
                strncmp(pszURL, psChild->psChild->pszValue, nPathLen) != 0 )
            {
                // Different path.
                const int nURLLen =
                    static_cast<int>(pszDash - psChild->psChild->pszValue);
                char *pszURLWithoutID = static_cast<char *>(
                    CPLMalloc((nURLLen + 1) * sizeof(char)));
                strncpy(pszURLWithoutID, psChild->psChild->pszValue, nURLLen);
                pszURLWithoutID[nURLLen] = '\0';

                if( CPLIsFilenameRelative(pszURLWithoutID) &&
                    strstr(pszURLWithoutID, ":") == nullptr )
                {
                    // Relative URL: prepend the path of pszURL.
                    const size_t nLen =
                        nPathLen +
                        CPLStrnlen(psChild->psChild->pszValue, 1024) + 1;
                    char *pszNew =
                        static_cast<char *>(CPLMalloc(nLen * sizeof(char)));
                    for( size_t i = 0; i < nPathLen; i++ )
                        pszNew[i] = pszURL[i];
                    pszNew[nPathLen] = '\0';
                    CPLStrlcat(pszNew, psChild->psChild->pszValue, nLen);
                    CPLSetXMLValue(psRoot, "#xlink:href", pszNew);
                    CPLFree(pszNew);
                }
                CPLFree(pszURLWithoutID);
            }
        }
    }

    // Search the child elements of psRoot.
    for( psChild = psRoot->psChild; psChild != nullptr; psChild = psChild->psNext)
        if( psChild->eType == CXT_Element )
            CorrectURLs(psChild, pszURL);
}

/************************************************************************/
/*                          FindTreeByURL()                             */
/*                                                                      */
/*  Find a doc tree that is located at pszURL.                          */
/*  If not present in ppapsRoot, it updates it and ppapszResourceHREF.  */
/************************************************************************/

static CPLXMLNode *FindTreeByURL( CPLXMLNode ***ppapsRoot,
                                  char ***ppapszResourceHREF,
                                  const char *pszURL )

{
    if( *ppapsRoot == nullptr || ppapszResourceHREF == nullptr )
        return nullptr;

    // If found in ppapszResourceHREF.
    const int i = CSLFindString(*ppapszResourceHREF, pszURL);
    if( i >= 0 )
    {
        // Return corresponding psRoot.
        return (*ppapsRoot)[i];
    }

    CPLXMLNode *psSrcTree = nullptr;
    char *pszLocation = CPLStrdup(pszURL);
    // If it is part of filesystem.
    if( CPLCheckForFile( pszLocation, nullptr) )
    {
        // Filesystem.
        psSrcTree = CPLParseXMLFile(pszURL);
    }
    else if( CPLHTTPEnabled() )
    {
        // Web resource.
        CPLErrorReset();
        CPLHTTPResult *psResult = CPLHTTPFetch(pszURL, nullptr);
        if( psResult != nullptr )
        {
            if( psResult->nDataLen > 0 && CPLGetLastErrorNo() == 0)
                psSrcTree = CPLParseXMLString(reinterpret_cast<const char*>(psResult->pabyData));
            CPLHTTPDestroyResult( psResult );
        }
    }

    // Report error in case the resource cannot be retrieved.
    if( psSrcTree == nullptr )
        CPLError(CE_Failure, CPLE_NotSupported, "Could not access %s",
                 pszLocation);

    CPLFree(pszLocation);

/************************************************************************/
/*      In the external GML resource we will only need elements         */
/*      identified by a "gml:id". So trim them.                         */
/************************************************************************/
    CPLXMLNode *psSibling = psSrcTree;
    while( psSibling != nullptr )
    {
        TrimTree(psSibling);
        psSibling = psSibling->psNext;
    }

    // Update to lists.
    int nItems = CSLCount(*ppapszResourceHREF);
    *ppapszResourceHREF = CSLAddString(*ppapszResourceHREF, pszURL);
    *ppapsRoot = static_cast<CPLXMLNode **>(
        CPLRealloc(*ppapsRoot, (nItems + 2) * sizeof(CPLXMLNode *)));
    (*ppapsRoot)[nItems] = psSrcTree;
    (*ppapsRoot)[nItems + 1] = nullptr;

    // Return the tree.
    return (*ppapsRoot)[nItems];
}

/************************************************************************/
/*                           ResolveTree()                              */
/*  Resolves the xlinks in a node and its siblings                      */
/*  If any error is encountered or any element is skipped(papszSkip):   */
/*      If bStrict is TRUE, process is stopped and CE_Error is returned */
/*      If bStrict is FALSE, the process is continued but CE_Warning is */
/*       returned at the end.                                           */
/*  If everything goes fine, CE_None is returned.                       */
/************************************************************************/

static CPLErr Resolve( CPLXMLNode *psNode,
                       CPLXMLNode ***ppapsRoot,
                       char ***ppapszResourceHREF,
                       char **papszSkip,
                       const int bStrict,
                       int nDepth )

{
    // For each sibling.
    CPLXMLNode *psSibling = nullptr;
    CPLXMLNode *psResource = nullptr;
    CPLXMLNode *psTarget = nullptr;
    CPLErr eReturn = CE_None, eReturned;

    for( psSibling = psNode; psSibling != nullptr; psSibling = psSibling->psNext )
    {
        if( psSibling->eType != CXT_Element )
            continue;

        CPLXMLNode *psChild = psSibling->psChild;
        while( psChild != nullptr &&
               !( psChild->eType == CXT_Attribute &&
                  EQUAL(psChild->pszValue, "xlink:href") ) )
            psChild = psChild->psNext;

        // If a child has a "xlink:href" attribute.
        if( psChild != nullptr && psChild->psChild != nullptr )
        {
            if( CSLFindString(papszSkip, psSibling->pszValue) >= 0 )
            {
                // Skipping a specified element.
                eReturn = CE_Warning;
                continue;
            }

            const int nDepthCheck = 256;
            if( nDepth % nDepthCheck == 0 )
            {
                // A way to track progress.
                CPLDebug("GML",
                         "Resolving xlinks... (currently %s)",
                         psChild->psChild->pszValue);
            }

            char **papszTokens =
                CSLTokenizeString2(psChild->psChild->pszValue, "#",
                                   CSLT_ALLOWEMPTYTOKENS |
                                   CSLT_STRIPLEADSPACES |
                                   CSLT_STRIPENDSPACES);
            if( CSLCount(papszTokens) != 2 || papszTokens[1][0] == '\0' )
            {
                CPLError(bStrict ? CE_Failure : CE_Warning,
                         CPLE_NotSupported,
                         "Error parsing the href %s.%s",
                         psChild->psChild->pszValue,
                         bStrict ? "" : " Skipping...");
                CSLDestroy(papszTokens);
                if( bStrict )
                    return CE_Failure;
                eReturn = CE_Warning;
                continue;
            }

            // Look for the resource with that URL.
            psResource =
                FindTreeByURL(ppapsRoot, ppapszResourceHREF, papszTokens[0]);
            if( psResource == nullptr )
            {
                CSLDestroy(papszTokens);
                if( bStrict )
                    return CE_Failure;
                eReturn = CE_Warning;
                continue;
            }

            // Look for the element with the ID.
            psTarget = FindElementByID(psResource, papszTokens[1]);
            if( psTarget != nullptr )
            {
                // Remove the xlink:href attribute.
                CPLRemoveXMLChild(psSibling, psChild);
                CPLDestroyXMLNode(psChild);

                // Make a copy of psTarget.
                CPLXMLNode *psCopy =
                    CPLCreateXMLNode(nullptr, CXT_Element, psTarget->pszValue);
                psCopy->psChild = CPLCloneXMLTree(psTarget->psChild);
                RemoveIDs(psCopy);
                // Correct empty URLs in URL#id pairs.
                if( CPLStrnlen(papszTokens[0], 1) > 0 )
                {
                    CorrectURLs(psCopy, papszTokens[0]);
                }
                CPLAddXMLChild(psSibling, psCopy);
                CSLDestroy(papszTokens);
            }
            else
            {
                // Element not found.
                CSLDestroy(papszTokens);
                CPLError(bStrict ? CE_Failure : CE_Warning,
                         CPLE_ObjectNull,
                         "Couldn't find the element with id %s.",
                         psChild->psChild->pszValue);
                if( bStrict )
                    return CE_Failure;
                eReturn = CE_Warning;
            }
        }

        // Recurse with the first child.
        eReturned = Resolve(psSibling->psChild, ppapsRoot, ppapszResourceHREF,
                            papszSkip, bStrict, nDepth + 1);

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

bool GMLReader::ResolveXlinks( const char *pszFile,
                               bool *pbOutIsTempFile,
                               char **papszSkip,
                               const bool bStrict )

{
    *pbOutIsTempFile = false;

    // Check if the original source file is set.
    if( m_pszFilename == nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GML source file needs to be set first with "
                 "GMLReader::SetSourceFile().");
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Load the raw XML file into a XML Node tree.                     */
/* -------------------------------------------------------------------- */
    CPLXMLNode **papsSrcTree =
        static_cast<CPLXMLNode **>(CPLCalloc(2, sizeof(CPLXMLNode *)));
    papsSrcTree[0] = CPLParseXMLFile(m_pszFilename);

    if( papsSrcTree[0] == nullptr )
    {
        CPLFree(papsSrcTree);
        return false;
    }

    // Make all the URLs absolute.
    CPLXMLNode *psSibling = nullptr;
    for( psSibling = papsSrcTree[0]; psSibling != nullptr;
         psSibling = psSibling->psNext )
        CorrectURLs(psSibling, m_pszFilename);

    // Setup resource data structure.
    char **papszResourceHREF = nullptr;
    // "" is the href of the original source file.
    papszResourceHREF = CSLAddString(papszResourceHREF, m_pszFilename);

    // Call resolver.
    const CPLErr eReturned = Resolve(papsSrcTree[0], &papsSrcTree,
                                     &papszResourceHREF, papszSkip, bStrict, 0);

    bool bReturn = true;
    if( eReturned != CE_Failure )
    {
        char *pszTmpName = nullptr;
        bool bTryWithTempFile = false;
        if( STARTS_WITH_CI(pszFile, "/vsitar/") ||
            STARTS_WITH_CI(pszFile, "/vsigzip/") ||
            STARTS_WITH_CI(pszFile, "/vsizip/") ||
            STARTS_WITH_CI(pszFile, "/vsicurl") )
        {
            bTryWithTempFile = true;
        }
        else if( !CPLSerializeXMLTreeToFile(papsSrcTree[0], pszFile) )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Cannot serialize resolved file %s to %s.",
                     m_pszFilename, pszFile);
            bTryWithTempFile = true;
        }

        if (bTryWithTempFile)
        {
            pszTmpName = CPLStrdup(CPLGenerateTempFilename("ResolvedGML"));
            if( !CPLSerializeXMLTreeToFile(papsSrcTree[0], pszTmpName) )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Cannot serialize resolved file %s to %s either.",
                         m_pszFilename, pszTmpName);
                CPLFree(pszTmpName);
                bReturn = false;
            }
            else
            {
                // Set the source file to the resolved file.
                CPLFree(m_pszFilename);
                m_pszFilename = pszTmpName;
                *pbOutIsTempFile = true;
            }
        }
        else
        {
            // Set the source file to the resolved file.
            CPLFree(m_pszFilename);
            m_pszFilename = CPLStrdup(pszFile);
        }
    }
    else
    {
        bReturn = false;
    }

    const int nItems = CSLCount(papszResourceHREF);
    CSLDestroy(papszResourceHREF);
    for(int i = 0; i < nItems; i++)
        CPLDestroyXMLNode(papsSrcTree[i]);
    CPLFree(papsSrcTree);

    return bReturn;
}
