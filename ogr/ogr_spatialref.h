/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes for manipulating spatial reference systems in a
 *           platform non-specific manner.
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
 * Revision 1.3  1999/06/25 20:21:18  warmerda
 * fleshed out classes
 *
 * Revision 1.2  1999/05/31 17:14:34  warmerda
 * Fixed define.
 *
 * Revision 1.1  1999/05/20 14:35:00  warmerda
 * New
 */

#ifndef _OGR_SPATIALREF_H_INCLUDED
#define _OGR_SPATIALREF_H_INCLUDED

#include "ogr_core.h"

/************************************************************************/
/*                             OGR_SRSNode                              */
/************************************************************************/

/**
 * Objects of this class are used to represent value nodes in the parsed
 * representation of the WKT SRS format.  For instance UNIT["METER",1]
 * would be rendered into three OGR_SRSNodes.  The root node would have a
 * value of UNIT, and two children, the first with a value of METER, and the
 * second with a value of 1.
 *
 * Normally application code just interacts with the OGRSpatialReference
 * object, which uses the OGR_SRSNode to implement it's data structure;
 * however, this class is user accessable for detailed access to components
 * of an SRS definition.
 */

class OGR_SRSNode
{
    char	*pszValue;

    int		nChildren;
    OGR_SRSNode	**papoChildNodes;

    void	ClearChildren();
    
  public:
    		OGR_SRSNode(const char * = NULL);
    		~OGR_SRSNode();

    int         IsLeafNode() { return nChildren == 0; }
    
    int		GetChildCount() { return nChildren; }
    OGR_SRSNode *GetChild( int );

    OGR_SRSNode *GetNode( const char * );

    void	AddChild( OGR_SRSNode * );

    const char  *GetValue() { return pszValue; }
    void        SetValue( const char * );

    OGR_SRSNode *Clone();

    OGRErr      importFromWkt( char ** );
    OGRErr	exportToWkt( char ** );
};

/************************************************************************/
/*                         OGRSpatialReference                          */
/************************************************************************/

/**
 * This class respresents a OpenGIS Spatial Reference System, and contains
 * methods for converting between this object organization and well known
 * text (WKT) format.  This object is reference counted as one instance of
 * the object is normally shared between many OGRGeometry objects.
 *
 * Normally application code can fetch needed parameter values for this
 * SRS using GetAttrValue(), but in special cases the underlying parse tree
 * (or OGR_SRSNode objects) can be accessed more directly.
 *
 * At this time, no methods for reprojection, validation of SRS semantics
 * have been implemented.  This will follow at some point in the future. 
 */

class OGRSpatialReference
{
    int		nRefCount;

    OGR_SRSNode *poRoot;
    
  public:
                OGRSpatialReference(const char * = NULL);
                
    virtual    ~OGRSpatialReference();
    		
    int		Reference();
    int		Dereference();
    int		GetReferenceCount() { return nRefCount; }

    OGR_SRSNode *GetRoot() { return poRoot; }
    void        SetRoot( OGR_SRSNode * );
    
    OGR_SRSNode *GetAttrNode(const char *);
    const char  *GetAttrValue(const char *, int = 0);

    OGRErr	Validate();

    OGRSpatialReference *Clone();

    OGRErr      importFromWkt( char ** );
    OGRErr      exportToWkt( char ** );
};

#endif /* ndef _OGR_SPATIALREF_H_INCLUDED */




