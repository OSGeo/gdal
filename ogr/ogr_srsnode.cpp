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
 * Revision 1.23  2002/12/10 04:06:57  warmerda
 * Added support for a parent pointer in OGR_SRSNode.
 * Ensure that authority codes are quoted (bugzilla 201)
 *
 * Revision 1.22  2002/07/25 13:13:36  warmerda
 * fixed const correctness of some docs
 *
 * Revision 1.21  2002/04/18 14:22:45  warmerda
 * made OGRSpatialReference and co 'const correct'
 *
 * Revision 1.20  2002/03/05 14:25:14  warmerda
 * expand tabs
 *
 * Revision 1.19  2002/02/21 20:09:04  warmerda
 * Drop white space outside of quotes when ingesting WKT.  This makes the
 * ingest of 'pretty' WKT possible.
 *
 * Revision 1.18  2002/01/24 16:21:29  warmerda
 * added StripNodes method, removed simplify flag from pretty wkt
 *
 * Revision 1.17  2002/01/18 15:30:04  warmerda
 * fixed serious bug in DeleteChild()
 *
 * Revision 1.16  2001/12/01 17:01:50  warmerda
 * don't omit empty child nodes when exporting pretty wkt
 *
 * Revision 1.15  2001/10/11 19:29:57  warmerda
 * fixed redeclaration of i
 *
 * Revision 1.14  2001/10/11 19:26:16  warmerda
 * added applyRemapping
 *
 * Revision 1.13  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.12  2001/01/19 21:10:46  warmerda
 * replaced tabs
 *
 * Revision 1.11  2000/07/09 20:48:02  warmerda
 * added exportToPrettyWkt
 *
 * Revision 1.10  2000/03/31 14:40:49  warmerda
 * Fixed multiple declaration of i in GetNode().
 *
 * Revision 1.9  2000/03/16 19:03:20  warmerda
 * added DestroyChild(), FindChild()
 *
 * Revision 1.8  2000/02/25 13:23:25  warmerda
 * removed include of ogr_geometry.h
 *
 * Revision 1.7  2000/01/26 21:22:18  warmerda
 * added tentative MakeValueSafe implementation
 *
 * Revision 1.6  2000/01/12 04:12:55  warmerda
 * Fixed bug in InsertChild().
 *
 * Revision 1.5  2000/01/11 22:12:13  warmerda
 * added InsertChild
 *
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
#include "ogr_p.h"

CPL_CVSID("$Id$");

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

    poParent = NULL;
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
 * \fn int OGR_SRSNode::GetChildCount() const;
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

const OGR_SRSNode *OGR_SRSNode::GetChild( int iChild ) const

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
    int  i;

    if( this == NULL )
        return NULL;
    
    if( nChildren > 0 && EQUAL(pszName,pszValue) )
        return this;

/* -------------------------------------------------------------------- */
/*      First we check the immediate children so we will get an         */
/*      immediate child in preference to a subchild.                    */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nChildren; i++ )
    {
        if( EQUAL(papoChildNodes[i]->pszValue,pszName) 
            && papoChildNodes[i]->nChildren > 0 )
            return papoChildNodes[i];
    }

/* -------------------------------------------------------------------- */
/*      Then get each child to check their children.                    */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nChildren; i++ )
    {
        OGR_SRSNode *poNode;

        poNode = papoChildNodes[i]->GetNode( pszName );
        if( poNode != NULL )
            return poNode;
    }

    return NULL;
}

const OGR_SRSNode *OGR_SRSNode::GetNode( const char * pszName ) const

{
    return ((OGR_SRSNode *) this)->GetNode( pszName );
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
    InsertChild( poNew, nChildren );
}

/************************************************************************/
/*                            InsertChild()                             */
/************************************************************************/

/**
 * Insert the passed node as a child of target node, at the indicated
 * position. 
 *
 * Note that ownership of the passed node is assumed by the node on which
 * the method is invoked ... use the Clone() method if the original is to
 * be preserved.  All existing children at location iChild and beyond are
 * push down one space to make space for the new child. 
 *
 * @param poNew the node to add as a child.
 * @param iChild position to insert, use 0 to insert at the beginning. 
 */

void OGR_SRSNode::InsertChild( OGR_SRSNode * poNew, int iChild )

{
    if( iChild > nChildren )
        iChild = nChildren;

    nChildren++;
    papoChildNodes = (OGR_SRSNode **)
        CPLRealloc( papoChildNodes, sizeof(void*) * nChildren );

    memmove( papoChildNodes + iChild + 1, papoChildNodes + iChild,
             sizeof(void*) * (nChildren - iChild - 1) );
    
    papoChildNodes[iChild] = poNew;
    poNew->poParent = this;
}

/************************************************************************/
/*                            DestroyChild()                            */
/************************************************************************/

/**
 * Remove a child node, and it's subtree.
 *
 * Note that removing a child node will result in children after it
 * being renumbered down one.
 *
 * @param iChild the index of the child.
 */

void OGR_SRSNode::DestroyChild( int iChild )

{
    if( iChild < 0 || iChild >= nChildren )
        return;

    delete papoChildNodes[iChild];
    while( iChild < nChildren-1 )
    {
        papoChildNodes[iChild] = papoChildNodes[iChild+1];
        iChild++;
    }

    nChildren--;
}

/************************************************************************/
/*                             FindChild()                              */
/************************************************************************/

/**
 * Find the index of the child matching the given string.
 *
 * Note that the node value must match pszValue with the exception of
 * case.  The comparison is case insensitive.
 *
 * @param pszValue the node value being searched for.
 *
 * @return the child index, or -1 on failure. 
 */

int OGR_SRSNode::FindChild( const char * pszValue ) const

{
    for( int i = 0; i < nChildren; i++ )
    {
        if( EQUAL(papoChildNodes[i]->pszValue,pszValue) )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                              GetValue()                              */
/************************************************************************/

/**
 * \fn const char *OGR_SRSNode::GetValue() const;
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

OGR_SRSNode *OGR_SRSNode::Clone() const

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
 

OGRErr OGR_SRSNode::exportToWkt( char ** ppszResult ) const

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

        // As per bugzilla bug 201, the OGC spec says the authority code
        // needs to be quoted even though it appears well behaved.
        if( poParent != NULL && EQUAL(poParent->GetValue(),"AUTHORITY") )
            bNeedQuoting = TRUE;
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
/*                         exportToPrettyWkt()                          */
/************************************************************************/

OGRErr OGR_SRSNode::exportToPrettyWkt( char ** ppszResult, int nDepth ) const

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
        papoChildNodes[i]->exportToPrettyWkt( papszChildrenWkt + i,
                                              nDepth + 1);
        nLength += strlen(papszChildrenWkt[i]) + 2 + nDepth*4;
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

        // As per bugzilla bug 201, the OGC spec says the authority code
        // needs to be quoted even though it appears well behaved.
        if( poParent != NULL && EQUAL(poParent->GetValue(),"AUTHORITY") )
            bNeedQuoting = TRUE;
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
        if( papoChildNodes[i]->GetChildCount() > 0 )
        {
            int  j;

            strcat( *ppszResult, "\n" );
            for( j = 0; j < 4*nDepth; j++ )
                strcat( *ppszResult, " " );
        }
        strcat( *ppszResult, papszChildrenWkt[i] );
        if( i < nChildren-1 )
            strcat( *ppszResult, "," );
    }

    if( nChildren > 0 )
    {
        if( (*ppszResult)[strlen(*ppszResult)-1] == ',' )
            (*ppszResult)[strlen(*ppszResult)-1] = '\0';
        
        strcat( *ppszResult, "]" );
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
        else if( !bInQuotedString 
                 && (*pszInput == ' ' || *pszInput == '\t' 
                     || *pszInput == 10 || *pszInput == 13) )
        {
            /* just skip over whitespace */
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

/************************************************************************/
/*                           MakeValueSafe()                            */
/************************************************************************/

/**
 * Massage value string, stripping special characters so it will be a
 * database safe string.
 *
 * The operation is also applies to all subnodes of the current node.
 */


void OGR_SRSNode::MakeValueSafe()

{
    int         i, j;

/* -------------------------------------------------------------------- */
/*      First process subnodes.                                         */
/* -------------------------------------------------------------------- */
    for( int iChild = 0; iChild < GetChildCount(); iChild++ )
    {
        GetChild(iChild)->MakeValueSafe();
    }

/* -------------------------------------------------------------------- */
/*      Skip numeric nodes.                                             */
/* -------------------------------------------------------------------- */
    if( (pszValue[0] >= '0' && pszValue[0] <= '9') || pszValue[0] != '.' )
        return;
    
/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszValue[i] != '\0'; i++ )
    {
        if( !(pszValue[i] >= 'A' && pszValue[i] <= 'Z')
            && !(pszValue[i] >= 'a' && pszValue[i] <= 'z')
            && !(pszValue[i] >= '0' && pszValue[i] <= '9') )
        {
            pszValue[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    for( i = 1, j = 0; pszValue[i] != '\0'; i++ )
    {
        if( pszValue[j] == '_' && pszValue[i] == '_' )
            continue;

        pszValue[++j] = pszValue[i];
    }
    
    if( pszValue[j] == '_' )
        pszValue[j] = '\0';
    else
        pszValue[j+1] = '\0';
}

/************************************************************************/
/*                           applyRemapper()                            */
/************************************************************************/

/**
 * Remap node values matching list.
 *
 * Remap the value of this node or any of it's children if it matches
 * one of the values in the source list to the corresponding value from
 * the destination list.  If the pszNode value is set, only do so if the
 * parent node matches that value.  Even if a replacement occurs, searching
 * continues.
 *
 * @param pszNode Restrict remapping to children of this type of node 
 *                (eg. "PROJECTION")
 * @param papszSrcValues a NULL terminated array of source string.  If the
 * node value matches one of these (case insensitive) then replacement occurs.
 * @param papszDstValue an array of destination strings.  On a match, the
 * one corresponding to a source value will be used to replace a node.
 * @param nStepSize increment when stepping through source and destination
 * arrays, allowing source and destination arrays to be one interleaved array
 * for instances.  Defaults to 1.
 * 
 * @return returns OGRERR_NONE unless something bad happens.  There is no
 * indication returned about whether any replacement occured.  
 */

OGRErr OGR_SRSNode::applyRemapper( const char *pszNode, 
                                   char **papszSrcValues, 
                                   char **papszDstValues, 
                                   int nStepSize, int bChildOfHit )

{
    int i;

/* -------------------------------------------------------------------- */
/*      Scan for value, and replace if our parent was a "hit".          */
/* -------------------------------------------------------------------- */
    if( bChildOfHit || pszNode == NULL )
    {
        for( i = 0; papszSrcValues[i] != NULL; i += nStepSize )
        {
            if( EQUAL(papszSrcValues[i],pszValue) )
            {
                SetValue( papszDstValues[i] );
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Are the the target node?                                        */
/* -------------------------------------------------------------------- */
    if( pszNode != NULL )
        bChildOfHit = EQUAL(pszValue,pszNode);

/* -------------------------------------------------------------------- */
/*      Recurse                                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < GetChildCount(); i++ )
    {
        GetChild(i)->applyRemapper( pszNode, papszSrcValues, 
                                    papszDstValues, nStepSize, bChildOfHit );
    }

    return OGRERR_NONE;
}
                                   
/************************************************************************/
/*                             StripNodes()                             */
/************************************************************************/

/**
 * Strip child nodes matching name.
 *
 * Removes any decendent nodes of this node that match the given name. 
 * Of course children of removed nodes are also discarded.
 *
 * @param pszName the name for nodes that should be removed.
 */

void OGR_SRSNode::StripNodes( const char * pszName )

{
/* -------------------------------------------------------------------- */
/*      Strip any children matching this name.                          */
/* -------------------------------------------------------------------- */
    while( FindChild( pszName ) >= 0 )
        DestroyChild( FindChild( pszName ) );

/* -------------------------------------------------------------------- */
/*      Recurse                                                         */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < GetChildCount(); i++ )
        GetChild(i)->StripNodes( pszName );
}
