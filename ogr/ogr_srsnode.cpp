/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGR_SRSNode class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.4  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.3  1999/07/29 18:09:20  warmerda
 * Avoid use of isdigit()
 *
 * Revision 1.2  1999/07/29 18:00:44  warmerda
 * modified quoting rules for WKT to more closely match standard examples
 *
 * Revision 1.1  1999/06/25 20:20:53  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

/************************************************************************/
/*                            OGR_SRSNode()                             */
/************************************************************************/

/**
 * Constructor.
 *
 * @param pszValueIn this optional parameter can be used to initialize
 * the value of the node upon creation.  If omitted the node will be created
 * with a value of "".  Newly created OGR_SRSNodes have no children.
 */

OGR_SRSNode::OGR_SRSNode( const char * pszValueIn )

{
    pszValue = CPLStrdup( pszValueIn );

    nChildren = 0;
    papoChildNodes = NULL;
}

/************************************************************************/
/*                            ~OGR_SRSNode()                            */
/************************************************************************/

OGR_SRSNode::~OGR_SRSNode()

{
    CPLFree( pszValue );

    ClearChildren();
}

/************************************************************************/
/*                           ClearChildren()                            */
/************************************************************************/

void OGR_SRSNode::ClearChildren()

{
    for( int i = 0; i < nChildren; i++ )
    {
        delete papoChildNodes[i];
    }

    CPLFree( papoChildNodes );

    papoChildNodes = NULL;
    nChildren = 0;
}

/************************************************************************/
/*                           GetChildCount()                            */
/************************************************************************/

/**
 * \fn int OGR_SRSNode::GetChildCount();
 *
 * Get number of children nodes.
 *
 * @return 0 for leaf nodes, or the number of children nodes. 
 */

/************************************************************************/
/*                              GetChild()                              */
/************************************************************************/

/**
 * Fetch requested child.
 *
 * @param iChild the index of the child to fetch, from 0 to
 * GetChildCount() - 1.
 *
 * @return a pointer to the child OGR_SRSNode, or NULL if there is no such
 * child. 
 */

OGR_SRSNode *OGR_SRSNode::GetChild( int iChild )

{
    if( iChild < 0 || iChild >= nChildren )
        return NULL;
    else
        return papoChildNodes[iChild];
}

/************************************************************************/
/*                              GetNode()                               */
/************************************************************************/

/**
 * Find named node in tree.
 *
 * This method does a pre-order traversal of the node tree searching for
 * a node with this exact value (case insensitive), and returns it.  Leaf
 * nodes are not considered, under the assumption that they are just
 * attribute value nodes.
 *
 * If a node appears more than once in the tree (such as UNIT for instance),
 * the first encountered will be returned.  Use GetNode() on a subtree to be
 * more specific. 
 *
 * @param pszName the name of the node to search for.
 *
 * @return a pointer to the node found, or NULL if none.
 */

OGR_SRSNode *OGR_SRSNode::GetNode( const char * pszName )

{
    if( this == NULL )
        return NULL;
    
    if( nChildren > 0 && EQUAL(pszName,pszValue) )
        return this;

    for( int i = 0; i < nChildren; i++ )
    {
        OGR_SRSNode *poNode;

        poNode = papoChildNodes[i]->GetNode( pszName );
        if( poNode != NULL )
            return poNode;
    }

    return NULL;
}

/************************************************************************/
/*                              AddChild()                              */
/************************************************************************/

/**
 * Add passed node as a child of target node.
 *
 * Note that ownership of the passed node is assumed by the node on which
 * the method is invoked ... use the Clone() method if the original is to
 * be preserved.  New children are always added at the end of the list.
 *
 * @param poNew the node to add as a child.
 */

void OGR_SRSNode::AddChild( OGR_SRSNode * poNew )

{
    nChildren++;
    papoChildNodes = (OGR_SRSNode **)
        CPLRealloc( papoChildNodes, sizeof(void*) * nChildren );
    papoChildNodes[nChildren-1] = poNew;
}

/************************************************************************/
/*                              GetValue()                              */
/************************************************************************/

/**
 * \fn const char *OGR_SRSNode::GetValue();
 *
 * Fetch value string for this node.
 *
 * @return A non-NULL string is always returned.  The returned pointer is to
 * the internal value of this node, and should not be modified, or freed.
 */

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

/**
 * Set the node value.
 *
 * @param pszNewValue the new value to assign to this node.  The passed
 * string is duplicated and remains the responsibility of the caller.
 */

void OGR_SRSNode::SetValue( const char * pszNewValue )

{
    CPLFree( pszValue );
    pszValue = CPLStrdup( pszNewValue );
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * Make a duplicate of this node, and it's children.
 *
 * @return a new node tree, which becomes the responsiblity of the caller.
 */

OGR_SRSNode *OGR_SRSNode::Clone()

{
    OGR_SRSNode *poNew;

    poNew = new OGR_SRSNode( pszValue );

    for( int i = 0; i < nChildren; i++ )
    {
        poNew->AddChild( papoChildNodes[i]->Clone() );
    }

    return poNew;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

/**
 * Convert this tree of nodes into WKT format.
 *
 * Note that the returned WKT string should be freed with OGRFree() or
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * @param ppszResult the resulting string is returned in this pointer.
 *
 * @return currently OGRERR_NONE is always returned, but the future it
 * is possible error conditions will develop. 
 */
 

OGRErr OGR_SRSNode::exportToWkt( char ** ppszResult )

{
    char        **papszChildrenWkt = NULL;
    int         nLength = strlen(pszValue)+4;
    int         i;

/* -------------------------------------------------------------------- */
/*      Build a list of the WKT format for the children.                */
/* -------------------------------------------------------------------- */
    papszChildrenWkt = (char **) CPLCalloc(sizeof(char*),(nChildren+1));
    
    for( i = 0; i < nChildren; i++ )
    {
        papoChildNodes[i]->exportToWkt( papszChildrenWkt + i );
        nLength += strlen(papszChildrenWkt[i]) + 1;
    }

/* -------------------------------------------------------------------- */
/*      Allocate the result string.                                     */
/* -------------------------------------------------------------------- */
    *ppszResult = (char *) CPLMalloc(nLength);
    *ppszResult[0] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Do we need to quote this value?  Determine whether or not       */
/*      this is a terminal string value.                                */
/* -------------------------------------------------------------------- */
    int         bNeedQuoting = FALSE;

    if( GetChildCount() == 0 )
    {
        for( i = 0; pszValue[i] != '\0'; i++ )
        {
            if( (pszValue[i] < '0' || pszValue[i] > '9')
                && pszValue[i] != '.'
                && pszValue[i] != '-' && pszValue[i] != '+'
                && pszValue[i] != 'e' && pszValue[i] != 'E' )
                bNeedQuoting = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Capture this nodes value.  We put it in double quotes if        */
/*      this is a leaf node, otherwise we assume it is a well formed    */
/*      node name.                                                      */
/* -------------------------------------------------------------------- */
    if( bNeedQuoting )
    {
        strcat( *ppszResult, "\"" );
        strcat( *ppszResult, pszValue ); /* should we do quoting? */
        strcat( *ppszResult, "\"" );
    }
    else
        strcat( *ppszResult, pszValue );

/* -------------------------------------------------------------------- */
/*      Add the children strings with appropriate brackets and commas.  */
/* -------------------------------------------------------------------- */
    if( nChildren > 0 )
        strcat( *ppszResult, "[" );
    
    for( i = 0; i < nChildren; i++ )
    {
        strcat( *ppszResult, papszChildrenWkt[i] );
        if( i == nChildren-1 )
            strcat( *ppszResult, "]" );
        else
            strcat( *ppszResult, "," );
    }

    CSLDestroy( papszChildrenWkt );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

/**
 * Import from WKT string.
 *
 * This method will wipe the existing children and value of this node, and
 * reassign them based on the contents of the passed WKT string.  Only as
 * much of the input string as needed to construct this node, and it's
 * children is consumed from the input string, and the input string pointer
 * is then updated to point to the remaining (unused) input.
 *
 * @param ppszInput Pointer to pointer to input.  The pointer is updated to
 * point to remaining unused input text.
 *
 * @return OGRERR_NONE if import succeeds, or OGRERR_CORRUPT_DATA if it
 * fails for any reason.
 */

OGRErr OGR_SRSNode::importFromWkt( char ** ppszInput )

{
    const char  *pszInput = *ppszInput;
    int         bInQuotedString = FALSE;
    
/* -------------------------------------------------------------------- */
/*      Clear any existing children of this node.                       */
/* -------------------------------------------------------------------- */
    ClearChildren();
    
/* -------------------------------------------------------------------- */
/*      Read the ``value'' for this node.                               */
/* -------------------------------------------------------------------- */
    char        szToken[512];
    int         nTokenLen = 0;
    
    while( *pszInput != '\0' && nTokenLen < (int) sizeof(szToken)-1 )
    {
        if( *pszInput == '"' )
        {
            bInQuotedString = !bInQuotedString;
        }
        else if( !bInQuotedString
              && (*pszInput == '[' || *pszInput == ']' || *pszInput == ','
                  || *pszInput == '(' || *pszInput == ')' ) )
        {
            break;
        }
        else
        {
            szToken[nTokenLen++] = *pszInput;
        }

        pszInput++;
    }

    if( *pszInput == '\0' || nTokenLen == sizeof(szToken) - 1 )
        return OGRERR_CORRUPT_DATA;

    szToken[nTokenLen++] = '\0';
    SetValue( szToken );

/* -------------------------------------------------------------------- */
/*      Read children, if we have a sublist.                            */
/* -------------------------------------------------------------------- */
    if( *pszInput == '[' || *pszInput == '(' )
    {
        do
        {
            OGR_SRSNode *poNewChild;
            OGRErr      eErr;

            pszInput++; // Skip bracket or comma.

            poNewChild = new OGR_SRSNode();

            eErr = poNewChild->importFromWkt( (char **) &pszInput );
            if( eErr != OGRERR_NONE )
                return eErr;

            AddChild( poNewChild );
            
        } while( *pszInput == ',' );

        if( *pszInput != ')' && *pszInput != ']' )
            return OGRERR_CORRUPT_DATA;

        pszInput++;
    }

    *ppszInput = (char *) pszInput;

    return OGRERR_NONE;
}
