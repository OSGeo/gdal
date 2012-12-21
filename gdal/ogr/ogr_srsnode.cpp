/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGR_SRSNode class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

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
/*                            NeedsQuoting()                            */
/*                                                                      */
/*      Does this node need to be quoted when it is exported to Wkt?    */
/************************************************************************/

int OGR_SRSNode::NeedsQuoting() const

{
    // non-terminals are never quoted.
    if( GetChildCount() != 0 )
        return FALSE;

    // As per bugzilla bug 201, the OGC spec says the authority code
    // needs to be quoted even though it appears well behaved.
    if( poParent != NULL && EQUAL(poParent->GetValue(),"AUTHORITY") )
        return TRUE;
    
    // As per bugzilla bug 294, the OGC spec says the direction
    // values for the AXIS keywords should *not* be quoted.
    if( poParent != NULL && EQUAL(poParent->GetValue(),"AXIS") 
        && this != poParent->GetChild(0) )
        return FALSE;

    // Strings starting with e or E are not valid numeric values, so they
    // need quoting, like in AXIS["E",EAST] 
    if( (pszValue[0] == 'e' || pszValue[0] == 'E') )
        return TRUE;

    // Non-numeric tokens are generally quoted while clean numeric values
    // are generally not. 
    for( int i = 0; pszValue[i] != '\0'; i++ )
    {
        if( (pszValue[i] < '0' || pszValue[i] > '9')
            && pszValue[i] != '.'
            && pszValue[i] != '-' && pszValue[i] != '+'
            && pszValue[i] != 'e' && pszValue[i] != 'E' )
            return TRUE;
    }

    return FALSE;
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
/*      Capture this nodes value.  We put it in double quotes if        */
/*      this is a leaf node, otherwise we assume it is a well formed    */
/*      node name.                                                      */
/* -------------------------------------------------------------------- */
    if( NeedsQuoting() )
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
/*      Capture this nodes value.  We put it in double quotes if        */
/*      this is a leaf node, otherwise we assume it is a well formed    */
/*      node name.                                                      */
/* -------------------------------------------------------------------- */
    if( NeedsQuoting() )
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
            {
                delete poNewChild;
                return eErr;
            }

            AddChild( poNewChild );
            
            // swallow whitespace
            while( isspace(*pszInput) ) 
                pszInput++;

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
 * @param papszDstValues an array of destination strings.  On a match, the
 * one corresponding to a source value will be used to replace a node.
 * @param nStepSize increment when stepping through source and destination
 * arrays, allowing source and destination arrays to be one interleaved array
 * for instances.  Defaults to 1.
 * @param bChildOfHit Only TRUE if we the current node is the child of a match,
 * and so needs to be set.  Application code would normally pass FALSE for this
 * argument.
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
            if( EQUAL(papszSrcValues[i],pszValue) && 
                ! EQUAL(papszDstValues[i],"") )
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

/************************************************************************/
/*                           FixupOrdering()                            */
/************************************************************************/

/* EXTENSION ... being a OSR extension... is arbitrary placed before the AUTHORITY */
static const char * const apszPROJCSRule[] = 
{ "PROJCS", "GEOGCS", "PROJECTION", "PARAMETER", "UNIT", "AXIS", "EXTENSION", "AUTHORITY", 
  NULL };

static const char * const apszDATUMRule[] = 
{ "DATUM", "SPHEROID", "TOWGS84", "AUTHORITY", NULL };

static const char * const apszGEOGCSRule[] = 
{ "GEOGCS", "DATUM", "PRIMEM", "UNIT", "AXIS", "AUTHORITY", NULL };

static const char * const apszGEOCCSRule[] = 
{ "GEOCCS", "DATUM", "PRIMEM", "UNIT", "AXIS", "AUTHORITY", NULL };

static const char * const *apszOrderingRules[] = {
    apszPROJCSRule, apszGEOGCSRule, apszDATUMRule, apszGEOCCSRule, NULL };

/**
 * Correct parameter ordering to match CT Specification.
 *
 * Some mechanisms to create WKT using OGRSpatialReference, and some
 * imported WKT fail to maintain the order of parameters required according
 * to the BNF definitions in the OpenGIS SF-SQL and CT Specifications.  This
 * method attempts to massage things back into the required order.
 *
 * This method will reorder the children of the node it is invoked on and
 * then recurse to all children to fix up their children.
 *
 * @return OGRERR_NONE on success or an error code if something goes 
 * wrong.  
 */

OGRErr OGR_SRSNode::FixupOrdering()

{
    int    i;

/* -------------------------------------------------------------------- */
/*      Recurse ordering children.                                      */
/* -------------------------------------------------------------------- */
    for( i = 0; i < GetChildCount(); i++ )
        GetChild(i)->FixupOrdering();

    if( GetChildCount() < 3 )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Is this a node for which an ordering rule exists?               */
/* -------------------------------------------------------------------- */
    const char * const * papszRule = NULL;

    for( i = 0; apszOrderingRules[i] != NULL; i++ )
    {
        if( EQUAL(apszOrderingRules[i][0],pszValue) )
        {
            papszRule = apszOrderingRules[i] + 1;
            break;
        }
    }

    if( papszRule == NULL )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      If we have a rule, apply it.  We create an array                */
/*      (panChildPr) with the priority code for each child (derived     */
/*      from the rule) and we then bubble sort based on this.           */
/* -------------------------------------------------------------------- */
    int  *panChildKey = (int *) CPLCalloc(sizeof(int),GetChildCount());

    for( i = 1; i < GetChildCount(); i++ )
    {
        panChildKey[i] = CSLFindString( (char**) papszRule, 
                                        GetChild(i)->GetValue() );
        if( panChildKey[i] == -1 )
        {
            CPLDebug( "OGRSpatialReference", 
                      "Found unexpected key %s when trying to order SRS nodes.",
                      GetChild(i)->GetValue() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Sort - Note we don't try to do anything with the first child    */
/*      which we assume is a name string.                               */
/* -------------------------------------------------------------------- */
    int j, bChange = TRUE;

    for( i = 1; bChange && i < GetChildCount()-1; i++ )
    {
        bChange = FALSE;
        for( j = 1; j < GetChildCount()-i; j++ )
        {
            if( panChildKey[j] == -1 || panChildKey[j+1] == -1 )
                continue;

            if( panChildKey[j] > panChildKey[j+1] )
            {
                OGR_SRSNode *poTemp = papoChildNodes[j];
                int          nKeyTemp = panChildKey[j];

                papoChildNodes[j] = papoChildNodes[j+1];
                papoChildNodes[j+1] = poTemp;

                nKeyTemp = panChildKey[j];
                panChildKey[j] = panChildKey[j+1];
                panChildKey[j+1] = nKeyTemp;

                bChange = TRUE;
            }
        }
    }

    CPLFree( panChildKey );

    return OGRERR_NONE;
}


